#pragma once

#include "codegen.hpp"

#include <vector>

namespace axir {

Program link_modules(const std::vector<Program> &modules);
Program optimize(Program program);

} // namespace axir
