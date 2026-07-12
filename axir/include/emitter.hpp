#pragma once

#include "codegen.hpp"
#include "config.hpp"

#include <filesystem>

namespace axir {

void emit_linux_x86_64_executable(const Program &program, const TargetConfig &target, const std::filesystem::path &output_path);

} // namespace axir
