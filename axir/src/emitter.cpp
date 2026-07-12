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

} // namespace

void emit_linux_x86_64_executable(const Program &program, const TargetConfig &target, const std::filesystem::path &output_path) {
  if (target.name != "linux_x86_64") throw std::runtime_error("direct executable emission currently requires the linux_x86_64 target");
  if (program.instructions.size() != 1 || program.instructions.front().opcode != "hlt" || program.instructions.front().operands.front().kind != OperandKind::Immediate) {
    throw std::runtime_error("the first direct-emission stage accepts exactly one 'hlt <immediate>' instruction");
  }

  std::vector<std::uint8_t> code;
  append_exit_code(code, program.instructions.front().operands.front().immediate, target);

  constexpr std::uint64_t base_address = 0x400000;
  constexpr std::size_t code_offset = 0x1000;
  const std::uint64_t image_size = code_offset + code.size();
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
