#include "emitter.hpp"

#include <cstdint>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>
#include <vector>

namespace axir {
namespace {

void append_u16(std::vector<std::uint8_t> &image, std::uint16_t value) {
  image.push_back(static_cast<std::uint8_t>(value));
  image.push_back(static_cast<std::uint8_t>(value >> 8));
}

void append_u32(std::vector<std::uint8_t> &image, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) image.push_back(static_cast<std::uint8_t>(value >> shift));
}

void append_u64(std::vector<std::uint8_t> &image, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8) image.push_back(static_cast<std::uint8_t>(value >> shift));
}

void append_exit_code(std::vector<std::uint8_t> &code, std::uint64_t value, const TargetConfig &target) {
  if (value > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("Linux x86-64 exit status must fit in 32 bits");
  code.insert(code.end(), target.exit_status_prefix.begin(), target.exit_status_prefix.end());
  append_u32(code, static_cast<std::uint32_t>(value));
  code.insert(code.end(), target.exit_number_sequence.begin(), target.exit_number_sequence.end());
  code.insert(code.end(), target.syscall_sequence.begin(), target.syscall_sequence.end());
}

const SyscallTemplate &syscall_template(const TargetConfig &target, std::string_view name) {
  for (const SyscallTemplate &syscall : target.syscalls) {
    if (syscall.name == name) return syscall;
  }
  throw std::runtime_error("target '" + target.name + "' has no syscall template for '" + std::string(name) + "'");
}

std::uint32_t slot_offset(std::uint64_t slot) {
  if (slot > std::numeric_limits<std::uint32_t>::max() / 8) throw std::runtime_error("integer slot is outside the direct-emission stack frame range");
  return static_cast<std::uint32_t>(slot * 8);
}

void append_load_slot(std::vector<std::uint8_t> &code, std::uint8_t register_code, std::uint64_t slot) {
  code.insert(code.end(), {0x48, 0x8b, static_cast<std::uint8_t>(0x85 | (register_code << 3))});
  append_u32(code, slot_offset(slot));
}

void append_store_slot(std::vector<std::uint8_t> &code, std::uint8_t register_code, std::uint64_t slot) {
  code.insert(code.end(), {0x48, 0x89, static_cast<std::uint8_t>(0x85 | (register_code << 3))});
  append_u32(code, slot_offset(slot));
}

void append_slot_immediate(std::vector<std::uint8_t> &code, std::uint64_t slot, std::uint64_t value) {
  code.insert(code.end(), {0x48, 0xb8});
  append_u64(code, value);
  append_store_slot(code, 0, slot);
}

void append_slot_binary_slots(std::vector<std::uint8_t> &code, const std::string &opcode, std::uint64_t destination, std::uint64_t left, std::uint64_t right) {
  append_load_slot(code, 0, left);
  if (opcode == "shl" || opcode == "shrl" || opcode == "shra") {
    append_load_slot(code, 1, right);
    const std::uint8_t extension = opcode == "shl" ? 4 : opcode == "shrl" ? 5 : 7;
    code.insert(code.end(), {0x48, 0xd3, static_cast<std::uint8_t>(0xc0 | (extension << 3))});
    append_store_slot(code, 0, destination);
    return;
  }
  append_load_slot(code, 3, right);
  if (opcode == "add") code.insert(code.end(), {0x48, 0x01, 0xd8});
  else if (opcode == "sub") code.insert(code.end(), {0x48, 0x29, 0xd8});
  else if (opcode == "and") code.insert(code.end(), {0x48, 0x21, 0xd8});
  else if (opcode == "or") code.insert(code.end(), {0x48, 0x09, 0xd8});
  else if (opcode == "xor") code.insert(code.end(), {0x48, 0x31, 0xd8});
  else if (opcode == "mul") code.insert(code.end(), {0x48, 0x0f, 0xaf, 0xc3});
  else throw std::runtime_error("internal unsupported integer binary opcode");
  append_store_slot(code, 0, destination);
}

void append_slot_binary_immediate(std::vector<std::uint8_t> &code, const std::string &opcode, std::uint64_t destination, std::uint64_t left, std::uint64_t immediate) {
  if (immediate > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("direct arithmetic immediate must fit in 32 bits");
  append_load_slot(code, 0, left);
  if (opcode == "shl" || opcode == "shrl" || opcode == "shra") {
    if (immediate > 255) throw std::runtime_error("direct shift immediate must fit in 8 bits");
    const std::uint8_t extension = opcode == "shl" ? 4 : opcode == "shrl" ? 5 : 7;
    code.insert(code.end(), {0x48, 0xc1, static_cast<std::uint8_t>(0xc0 | (extension << 3)), static_cast<std::uint8_t>(immediate)});
    append_store_slot(code, 0, destination);
    return;
  }
  if (opcode == "mul") {
    code.insert(code.end(), {0x48, 0x69, 0xc0});
  } else {
    std::uint8_t extension = 0;
    if (opcode == "add") extension = 0;
    else if (opcode == "or") extension = 1;
    else if (opcode == "and") extension = 4;
    else if (opcode == "sub") extension = 5;
    else if (opcode == "xor") extension = 6;
    else throw std::runtime_error("internal unsupported integer immediate opcode");
    code.insert(code.end(), {0x48, 0x81, static_cast<std::uint8_t>(0xc0 | (extension << 3))});
  }
  append_u32(code, static_cast<std::uint32_t>(immediate));
  append_store_slot(code, 0, destination);
}

void append_memory_access(std::vector<std::uint8_t> &code, const Instruction &instruction) {
  if (instruction.opcode.rfind("load", 0) == 0) {
    append_load_slot(code, 0, instruction.operands[1].slot);
    if (instruction.opcode == "load8u") code.insert(code.end(), {0x48, 0x0f, 0xb6, 0x00});
    else if (instruction.opcode == "load8s" || instruction.opcode == "load8") code.insert(code.end(), {0x48, 0x0f, 0xbe, 0x00});
    else if (instruction.opcode == "load16u") code.insert(code.end(), {0x48, 0x0f, 0xb7, 0x00});
    else if (instruction.opcode == "load16s" || instruction.opcode == "load16") code.insert(code.end(), {0x48, 0x0f, 0xbf, 0x00});
    else if (instruction.opcode == "load32u" || instruction.opcode == "load32") code.insert(code.end(), {0x8b, 0x00});
    else if (instruction.opcode == "load32s") code.insert(code.end(), {0x48, 0x63, 0x00});
    else if (instruction.opcode == "load64") code.insert(code.end(), {0x48, 0x8b, 0x00});
    else throw std::runtime_error("internal unsupported load opcode");
    append_store_slot(code, 0, instruction.operands[0].slot);
    return;
  }
  append_load_slot(code, 0, instruction.operands[0].slot);
  append_load_slot(code, 3, instruction.operands[1].slot);
  if (instruction.opcode == "store8") code.insert(code.end(), {0x88, 0x18});
  else if (instruction.opcode == "store16") code.insert(code.end(), {0x66, 0x89, 0x18});
  else if (instruction.opcode == "store32") code.insert(code.end(), {0x89, 0x18});
  else if (instruction.opcode == "store64") code.insert(code.end(), {0x48, 0x89, 0x18});
  else throw std::runtime_error("internal unsupported store opcode");
}

void append_slot_division(std::vector<std::uint8_t> &code, const std::string &opcode, std::uint64_t destination, std::uint64_t left, const Operand &right) {
  if (right.kind == Immediate && right.immediate == 0) throw std::runtime_error("direct division or modulus by zero");
  append_load_slot(code, 0, left);
  if (right.kind == IntegerSlot) append_load_slot(code, 3, right.slot);
  else {
    code.insert(code.end(), {0x48, 0xbb});
    append_u64(code, right.immediate);
  }
  if (opcode == "divu" || opcode == "modu") code.insert(code.end(), {0x31, 0xd2, 0x48, 0xf7, 0xf3});
  else if (opcode == "divs" || opcode == "mods") code.insert(code.end(), {0x48, 0x99, 0x48, 0xf7, 0xfb});
  else throw std::runtime_error("internal unsupported division opcode");
  append_store_slot(code, opcode == "mods" || opcode == "modu" ? 2 : 0, destination);
}

void append_slot_comparison(std::vector<std::uint8_t> &code, const std::string &opcode, std::uint64_t destination, std::uint64_t left, const Operand &right) {
  append_load_slot(code, 0, left);
  if (right.kind == IntegerSlot) {
    append_load_slot(code, 3, right.slot);
    code.insert(code.end(), {0x48, 0x39, 0xd8});
  } else {
    if (right.immediate > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("direct comparison immediate must fit in 32 bits");
    code.insert(code.end(), {0x48, 0x81, 0xf8});
    append_u32(code, static_cast<std::uint32_t>(right.immediate));
  }
  std::uint8_t condition = 0;
  if (opcode == "cmp_eq") condition = 0x94;
  else if (opcode == "cmp_ne") condition = 0x95;
  else if (opcode == "cmp_lts") condition = 0x9c;
  else if (opcode == "cmp_gts") condition = 0x9f;
  else if (opcode == "cmp_les") condition = 0x9e;
  else if (opcode == "cmp_ges") condition = 0x9d;
  else if (opcode == "cmp_ltu") condition = 0x92;
  else if (opcode == "cmp_gtu") condition = 0x97;
  else if (opcode == "cmp_leu") condition = 0x96;
  else if (opcode == "cmp_geu") condition = 0x93;
  else throw std::runtime_error("internal unsupported comparison opcode");
  code.insert(code.end(), {0x0f, condition, 0xc0, 0x48, 0x0f, 0xb6, 0xc0});
  append_store_slot(code, 0, destination);
}

void append_syscall_arguments(std::vector<std::uint8_t> &code, const SyscallTemplate &syscall) {
  constexpr std::uint8_t argument_registers[] = {7, 6, 2};
  for (std::size_t index = 0; index < syscall.argument_slots.size(); ++index) append_load_slot(code, argument_registers[index], syscall.argument_slots[index]);
}

void append_syscall_result(std::vector<std::uint8_t> &code, std::uint64_t slot) {
  append_store_slot(code, 0, slot);
}

void validate_direct_syscall_abi(const SyscallTemplate &syscall) {
  if (syscall.argument_slots.size() > 3) throw std::runtime_error("the current direct emitter supports at most three syscall arguments");
}

void write_u32(std::vector<std::uint8_t> &code, std::size_t offset, std::uint32_t value) {
  if (offset + 4 > code.size()) throw std::runtime_error("internal direct-emission relocation error");
  for (unsigned shift = 0; shift < 32; shift += 8) code[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}

void write_u64(std::vector<std::uint8_t> &code, std::size_t offset, std::uint64_t value) {
  if (offset + 8 > code.size()) throw std::runtime_error("internal direct-emission relocation error");
  for (unsigned shift = 0; shift < 64; shift += 8) code[offset + shift / 8] = static_cast<std::uint8_t>(value >> shift);
}

std::size_t align_up(std::size_t value, std::size_t alignment) {
  return (value + alignment - 1) / alignment * alignment;
}

std::uint32_t stack_frame_size(const Program &program, const TargetConfig &target) {
  std::uint64_t largest_slot = 0;
  bool has_slot = false;
  const auto observe = [&largest_slot, &has_slot](std::uint64_t slot) {
    if (slot > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max() / 8 - 1)) {
      throw std::runtime_error("integer slot frame is too large for direct emission");
    }
    if (!has_slot || slot > largest_slot) largest_slot = slot;
    has_slot = true;
  };
  for (const Instruction &instruction : program.instructions) {
    for (const Operand &operand : instruction.operands) {
      if (operand.kind == IntegerSlot) observe(operand.slot);
    }
    if (instruction.opcode == "syscall") {
      const SyscallTemplate &syscall = syscall_template(target, instruction.operands[0].label);
      for (std::uint64_t slot : syscall.argument_slots) observe(slot);
      if (syscall.has_result_slot) observe(syscall.result_slot);
    }
  }
  const std::uint64_t bytes = has_slot ? (largest_slot + 1) * 8 : 0;
  if (bytes > static_cast<std::uint64_t>(std::numeric_limits<std::int32_t>::max() - 15)) {
    throw std::runtime_error("integer slot frame is too large for direct emission");
  }
  return static_cast<std::uint32_t>(align_up(static_cast<std::size_t>(bytes), 16));
}

} // namespace

void emit_linux_x86_64_executable(const Program &program, const TargetConfig &target, const std::filesystem::path &output_path) {
  if (target.name != "linux_x86_64") throw std::runtime_error("direct executable emission currently requires the linux_x86_64 target");

  struct DataPatch {
    std::size_t code_offset;
    std::string symbol;
  };
  struct BranchPatch {
    std::size_t displacement_offset;
    std::string symbol;
  };
  std::vector<std::uint8_t> code;
  const std::uint32_t frame_size = stack_frame_size(program, target);
  code.insert(code.end(), {0x48, 0x89, 0xe5});
  if (frame_size != 0) {
    code.insert(code.end(), {0x48, 0x81, 0xec});
    append_u32(code, frame_size);
  }
  std::vector<DataPatch> patches;
  std::vector<BranchPatch> branch_patches;
  std::vector<std::size_t> instruction_offsets;
  instruction_offsets.reserve(program.instructions.size() + 1);
  for (const Instruction &instruction : program.instructions) {
    instruction_offsets.push_back(code.size());
    if (instruction.opcode == "mov" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Immediate) {
      append_slot_immediate(code, instruction.operands[0].slot, instruction.operands[1].immediate);
    } else if (instruction.opcode == "mov" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot) {
      append_load_slot(code, 0, instruction.operands[1].slot);
      append_store_slot(code, 0, instruction.operands[0].slot);
    } else if ((instruction.opcode == "add" || instruction.opcode == "sub" || instruction.opcode == "and" || instruction.opcode == "or" || instruction.opcode == "xor" || instruction.opcode == "mul" || instruction.opcode == "shl" || instruction.opcode == "shrl" || instruction.opcode == "shra") && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && instruction.operands[2].kind == Immediate) {
      append_slot_binary_immediate(code, instruction.opcode, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2].immediate);
    } else if ((instruction.opcode == "add" || instruction.opcode == "sub" || instruction.opcode == "and" || instruction.opcode == "or" || instruction.opcode == "xor" || instruction.opcode == "mul" || instruction.opcode == "shl" || instruction.opcode == "shrl" || instruction.opcode == "shra") && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && instruction.operands[2].kind == IntegerSlot) {
      append_slot_binary_slots(code, instruction.opcode, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2].slot);
    } else if ((instruction.opcode == "load8" || instruction.opcode == "load16" || instruction.opcode == "load32" || instruction.opcode == "load64" || instruction.opcode == "load8u" || instruction.opcode == "load16u" || instruction.opcode == "load32u" || instruction.opcode == "load8s" || instruction.opcode == "load16s" || instruction.opcode == "load32s" || instruction.opcode == "store8" || instruction.opcode == "store16" || instruction.opcode == "store32" || instruction.opcode == "store64") && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot) {
      append_memory_access(code, instruction);
    } else if ((instruction.opcode == "divs" || instruction.opcode == "divu" || instruction.opcode == "mods" || instruction.opcode == "modu") && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && (instruction.operands[2].kind == IntegerSlot || instruction.operands[2].kind == Immediate)) {
      append_slot_division(code, instruction.opcode, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2]);
    } else if ((instruction.opcode == "cmp_eq" || instruction.opcode == "cmp_ne" || instruction.opcode == "cmp_lts" || instruction.opcode == "cmp_gts" || instruction.opcode == "cmp_les" || instruction.opcode == "cmp_ges" || instruction.opcode == "cmp_ltu" || instruction.opcode == "cmp_gtu" || instruction.opcode == "cmp_leu" || instruction.opcode == "cmp_geu") && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && (instruction.operands[2].kind == IntegerSlot || instruction.operands[2].kind == Immediate)) {
      append_slot_comparison(code, instruction.opcode, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2]);
    } else if (instruction.opcode == "addr" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Label) {
      const std::size_t address_offset = code.size() + 2;
      append_slot_immediate(code, instruction.operands[0].slot, 0);
      patches.push_back({address_offset, instruction.operands[1].label});
    } else if (instruction.opcode == "call" && instruction.operands.size() == 1 && instruction.operands[0].kind == Label) {
      code.insert(code.end(), {0xe8, 0, 0, 0, 0});
      branch_patches.push_back({code.size() - 4, instruction.operands[0].label});
    } else if (instruction.opcode == "ret_void") {
      code.push_back(0xc3);
    } else if (instruction.opcode == "ret" && instruction.operands[0].kind == IntegerSlot) {
      append_load_slot(code, 0, instruction.operands[0].slot);
      code.push_back(0xc3);
    } else if (instruction.opcode == "ret" && instruction.operands[0].kind == Immediate) {
      code.insert(code.end(), {0x48, 0xb8});
      append_u64(code, instruction.operands[0].immediate);
      code.push_back(0xc3);
    } else if (instruction.opcode == "jmp" && instruction.operands[0].kind == Label) {
      code.insert(code.end(), {0xe9, 0, 0, 0, 0});
      branch_patches.push_back({code.size() - 4, instruction.operands[0].label});
    } else if (instruction.opcode == "cjump" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Label) {
      append_load_slot(code, 0, instruction.operands[0].slot);
      code.insert(code.end(), {0x48, 0x85, 0xc0, 0x0f, 0x85, 0, 0, 0, 0});
      branch_patches.push_back({code.size() - 4, instruction.operands[1].label});
    } else if (instruction.opcode == "syscall") {
      const std::string &name = instruction.operands[0].label;
      const SyscallTemplate &syscall = syscall_template(target, name);
      if (syscall.load_number.empty() || syscall.invoke.empty()) throw std::runtime_error("direct emission requires a primitive byte template for syscall '" + name + "'");
      validate_direct_syscall_abi(syscall);
      append_syscall_arguments(code, syscall);
      code.insert(code.end(), syscall.load_number.begin(), syscall.load_number.end());
      code.insert(code.end(), syscall.invoke.begin(), syscall.invoke.end());
      if (syscall.has_result_slot) append_syscall_result(code, syscall.result_slot);
    } else if (instruction.opcode == "hlt" && instruction.operands[0].kind == Immediate) {
      append_exit_code(code, instruction.operands[0].immediate, target);
    } else if (instruction.opcode == "hlt" && instruction.operands[0].kind == IntegerSlot) {
      const SyscallTemplate &exit = syscall_template(target, "exit");
      validate_direct_syscall_abi(exit);
      append_load_slot(code, 7, instruction.operands[0].slot);
      code.insert(code.end(), exit.load_number.begin(), exit.load_number.end());
      code.insert(code.end(), exit.invoke.begin(), exit.invoke.end());
    } else {
      throw std::runtime_error("the current direct-emission subset supports mov I[n], immediate arithmetic; xor; addr I[n], data; jmp/cjump; syscall primitive; and hlt");
    }
  }
  instruction_offsets.push_back(code.size());
  for (const BranchPatch &patch : branch_patches) {
    const auto label = program.labels.find(patch.symbol);
    if (label == program.labels.end() || label->second >= instruction_offsets.size()) {
      throw std::runtime_error("unknown branch label during direct emission: '" + patch.symbol + "'");
    }
    const std::int64_t displacement = static_cast<std::int64_t>(instruction_offsets[label->second]) - static_cast<std::int64_t>(patch.displacement_offset + 4);
    if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max()) {
      throw std::runtime_error("branch target is outside the direct-emission rel32 range");
    }
    write_u32(code, patch.displacement_offset, static_cast<std::uint32_t>(static_cast<std::int32_t>(displacement)));
  }

  constexpr std::uint64_t base_address = 0x400000;
  constexpr std::size_t code_offset = 0x1000;
  const std::size_t data_offset = align_up(code_offset + code.size(), 0x1000);
  std::size_t data_size = 0;
  for (const DataObject &data : program.data) data_size += data.bytes.size();
  for (const DataPatch &patch : patches) {
    std::size_t relative_offset = 0;
    bool found = false;
    for (const DataObject &data : program.data) {
      if (data.name == patch.symbol) {
        write_u64(code, patch.code_offset, base_address + data_offset + relative_offset);
        found = true;
        break;
      }
      relative_offset += data.bytes.size();
    }
    if (!found) throw std::runtime_error("unknown static data symbol during direct emission: '" + patch.symbol + "'");
  }
  const std::uint64_t image_size = data_offset + data_size;
  std::vector<std::uint8_t> image;
  image.reserve(static_cast<std::size_t>(image_size));

  image.insert(image.end(), {0x7f, 'E', 'L', 'F', 2, 1, 1, 0});
  image.insert(image.end(), 8, 0);
  append_u16(image, 2);
  append_u16(image, 62);
  append_u32(image, 1);
  append_u64(image, base_address + code_offset);
  append_u64(image, 64);
  append_u64(image, 0);
  append_u32(image, 0);
  append_u16(image, 64);
  append_u16(image, 56);
  append_u16(image, 2);
  append_u16(image, 0);
  append_u16(image, 0);
  append_u16(image, 0);

  append_u32(image, 1);
  append_u32(image, 5);
  append_u64(image, 0);
  append_u64(image, base_address);
  append_u64(image, base_address);
  append_u64(image, data_offset);
  append_u64(image, data_offset);
  append_u64(image, code_offset);

  append_u32(image, 1);
  append_u32(image, 6);
  append_u64(image, data_offset);
  append_u64(image, base_address + data_offset);
  append_u64(image, base_address + data_offset);
  append_u64(image, data_size);
  append_u64(image, data_size);
  append_u64(image, code_offset);

  if (image.size() != 176) throw std::runtime_error("internal ELF header size error");
  image.resize(code_offset, 0);
  image.insert(image.end(), code.begin(), code.end());
  image.resize(data_offset, 0);
  for (const DataObject &data : program.data) image.insert(image.end(), data.bytes.begin(), data.bytes.end());

  std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
  if (!output) throw std::runtime_error("cannot create executable '" + output_path.string() + "'");
  output.write(reinterpret_cast<const char *>(image.data()), static_cast<std::streamsize>(image.size()));
  if (!output) throw std::runtime_error("cannot write executable '" + output_path.string() + "'");
  output.close();

  std::error_code error;
  std::filesystem::permissions(output_path, std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::add, error);
  if (error) throw std::runtime_error("cannot mark executable '" + output_path.string() + "': " + error.message());
}

} // namespace axir
