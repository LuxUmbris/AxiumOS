#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <string_view>

namespace axir {

struct RunConfig {
  std::size_t memory_size = 64 * 1024;
  std::size_t max_steps = 1'000'000;
};

struct TargetConfig {
  std::string name;
  std::filesystem::path path;
};

TargetConfig load_target_config(std::string_view executable_path, std::string_view target_name);

} // namespace axir
