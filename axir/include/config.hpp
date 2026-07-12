#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace axir {

struct RunConfig {
  std::size_t memory_size = 64 * 1024;
  std::size_t max_steps = 1'000'000;
};

struct SyscallTemplate {
  std::string name;
  std::vector<std::uint64_t> argument_slots;
  bool has_result_slot = false;
  std::uint64_t result_slot = 0;
  std::vector<std::uint8_t> load_number;
  std::vector<std::uint8_t> invoke;
};

struct TargetConfig {
  std::string name;
  std::filesystem::path path;
  std::vector<std::uint8_t> exit_status_prefix;
  std::vector<std::uint8_t> exit_number_sequence;
  std::vector<std::uint8_t> syscall_sequence;
  std::vector<SyscallTemplate> syscalls;
};

TargetConfig load_target_config(std::string_view executable_path, std::string_view target_name);

} // namespace axir
