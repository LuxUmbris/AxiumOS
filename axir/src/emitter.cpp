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

std::uint8_t slot_register(std::uint64_t slot) {
  if (slot == 0) return 7;
  if (slot == 1) return 6;
  if (slot == 2) return 2;
  throw std::runtime_error("the direct-emission stage supports integer slots I[0] through I[2]");
}

void append_slot_immediate(std::vector<std::uint8_t> &code, std::uint64_t slot, std::uint64_t value) {
  code.push_back(0x48);
  code.push_back(static_cast<std::uint8_t>(0xb8 + slot_register(slot)));
  append_u64(code, value);
}

void append_slot_move(std::vector<std::uint8_t> &code, std::uint64_t destination, std::uint64_t source) {
  code.insert(code.end(), {0x48, 0x89, static_cast<std::uint8_t>(0xc0 | (slot_register(source) << 3) | slot_register(destination))});
}

void append_slot_add_immediate(std::vector<std::uint8_t> &code, std::uint64_t destination, std::uint64_t left, std::uint64_t immediate) {
  if (immediate > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("direct arithmetic immediate must fit in 32 bits");
  append_slot_move(code, destination, left);
  code.insert(code.end(), {0x48, 0x81, static_cast<std::uint8_t>(0xc0 | slot_register(destination))});
  append_u32(code, static_cast<std::uint32_t>(immediate));
}

void append_slot_sub_immediate(std::vector<std::uint8_t> &code, std::uint64_t destination, std::uint64_t left, std::uint64_t immediate) {
  if (immediate > std::numeric_limits<std::uint32_t>::max()) throw std::runtime_error("direct arithmetic immediate must fit in 32 bits");
  append_slot_move(code, destination, left);
  code.insert(code.end(), {0x48, 0x81, static_cast<std::uint8_t>(0xe8 | slot_register(destination))});
  append_u32(code, static_cast<std::uint32_t>(immediate));
}

void append_slot_xor(std::vector<std::uint8_t> &code, std::uint64_t destination, std::uint64_t left, std::uint64_t right) {
  append_slot_move(code, destination, left);
  code.insert(code.end(), {0x48, 0x31, static_cast<std::uint8_t>(0xc0 | (slot_register(right) << 3) | slot_register(destination))});
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
  std::vector<DataPatch> patches;
  std::vector<BranchPatch> branch_patches;
  std::vector<std::size_t> instruction_offsets;
  instruction_offsets.reserve(program.instructions.size() + 1);
  for (const Instruction &instruction : program.instructions) {
    instruction_offsets.push_back(code.size());
    if (instruction.opcode == "mov" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Immediate) {
      append_slot_immediate(code, instruction.operands[0].slot, instruction.operands[1].immediate);
    } else if (instruction.opcode == "add" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && instruction.operands[2].kind == Immediate) {
      append_slot_add_immediate(code, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2].immediate);
    } else if (instruction.opcode == "sub" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && instruction.operands[2].kind == Immediate) {
      append_slot_sub_immediate(code, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2].immediate);
    } else if (instruction.opcode == "xor" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == IntegerSlot && instruction.operands[2].kind == IntegerSlot) {
      append_slot_xor(code, instruction.operands[0].slot, instruction.operands[1].slot, instruction.operands[2].slot);
    } else if (instruction.opcode == "addr" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Label) {
      append_slot_immediate(code, instruction.operands[0].slot, 0);
      patches.push_back({code.size() - 8, instruction.operands[1].label});
    } else if (instruction.opcode == "jmp" && instruction.operands[0].kind == Label) {
      code.insert(code.end(), {0xe9, 0, 0, 0, 0});
      branch_patches.push_back({code.size() - 4, instruction.operands[0].label});
    } else if (instruction.opcode == "cjump" && instruction.operands[0].kind == IntegerSlot && instruction.operands[1].kind == Label) {
      const std::uint8_t condition = slot_register(instruction.operands[0].slot);
      code.insert(code.end(), {0x48, 0x85, static_cast<std::uint8_t>(0xc0 | (condition << 3) | condition), 0x0f, 0x85, 0, 0, 0, 0});
      branch_patches.push_back({code.size() - 4, instruction.operands[1].label});
    } else if (instruction.opcode == "syscall") {
      const std::string &name = instruction.operands[0].label;
      const SyscallTemplate &syscall = syscall_template(target, name);
      if (syscall.load_number.empty() || syscall.invoke.empty()) throw std::runtime_error("direct emission requires a primitive byte template for syscall '" + name + "'");
      code.insert(code.end(), syscall.load_number.begin(), syscall.load_number.end());
      code.insert(code.end(), syscall.invoke.begin(), syscall.invoke.end());
    } else if (instruction.opcode == "hlt" && instruction.operands[0].kind == Immediate) {
      append_exit_code(code, instruction.operands[0].immediate, target);
    } else if (instruction.opcode == "hlt" && instruction.operands[0].kind == IntegerSlot && instruction.operands[0].slot == 0) {
      const SyscallTemplate &exit = syscall_template(target, "exit");
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
  const std::size_t data_offset = align_up(code_offset + code.size(), 16);
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
  append_u16(image, 1);
  append_u16(image, 0);
  append_u16(image, 0);
  append_u16(image, 0);

  append_u32(image, 1);
  append_u32(image, 5);
  append_u64(image, 0);
  append_u64(image, base_address);
  append_u64(image, base_address);
  append_u64(image, image_size);
  append_u64(image, image_size);
  append_u64(image, code_offset);

  if (image.size() != 120) throw std::runtime_error("internal ELF header size error");
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
