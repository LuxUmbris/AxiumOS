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

struct TargetConfig {
  std::string name;
  std::filesystem::path path;
  std::vector<std::uint8_t> exit_status_prefix;
  std::vector<std::uint8_t> exit_number_sequence;
  std::vector<std::uint8_t> syscall_sequence;
};

TargetConfig load_target_config(std::string_view executable_path, std::string_view target_name);

} // namespace axir
