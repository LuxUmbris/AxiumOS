#pragma once

#include <cstddef>

namespace axir {

struct RunConfig {
  std::size_t memory_size = 64 * 1024;
  std::size_t max_steps = 1'000'000;
};

} // namespace axir
