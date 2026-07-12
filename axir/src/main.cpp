#include "codegen.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

void usage(const char *argv0) {
  std::cerr << "Usage: " << argv0 << " <check|run|dump> <program.axir> [options]\n"
            << "       " << argv0 << " <program.axir>  # same as check\n\n"
            << "Options for run:\n"
            << "  --memory <bytes>    Reference memory size (default: 65536)\n"
            << "  --max-steps <n>     Execution step limit (default: 1000000)\n"
            << "  -h, --help          Show this help text\n";
}

bool parse_size(const std::string &text, std::size_t &out) {
  try {
    std::size_t consumed = 0;
    const auto value = std::stoull(text, &consumed, 10);
    if (consumed != text.size() || value > std::numeric_limits<std::size_t>::max()) return false;
    out = static_cast<std::size_t>(value);
    return true;
  } catch (...) {
    return false;
  }
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }
  if (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
    usage(argv[0]);
    return 0;
  }
  std::string command = "check";
  int arg = 1;
  const std::string first = argv[arg];
  if (first == "check" || first == "run" || first == "dump") {
    command = first;
    ++arg;
  }
  if (arg >= argc) {
    usage(argv[0]);
    return 2;
  }
  const std::string path = argv[arg++];
  axir::RunConfig config;
  while (arg < argc) {
    const std::string option = argv[arg++];
    if ((option == "--memory" || option == "--max-steps") && arg < argc) {
      std::size_t value = 0;
      if (!parse_size(argv[arg++], value) || value == 0) {
        std::cerr << "error: " << option << " expects a positive integer\n";
        return 2;
      }
      if (option == "--memory") config.memory_size = value;
      else config.max_steps = value;
    } else {
      std::cerr << "error: unknown or incomplete option '" << option << "'\n";
      return 2;
    }
  }

  try {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open '" + path + "'");
    std::ostringstream source;
    source << input.rdbuf();
    axir::Program program = axir::parse(axir::tokenize(source.str()));
    axir::validate(program);
    if (command == "dump") {
      std::cout << axir::dump(program);
      return 0;
    }
    if (command == "check") {
      std::cout << "AXIR validation passed: " << program.instructions.size() << " instruction(s), "
                << program.data.size() << " data object(s)\n";
      return 0;
    }
    const int status = axir::run(program, config);
    std::cout << "AXIR program exited with status " << status << "\n";
    return status;
  } catch (const std::exception &error) {
    std::cerr << "axirc: " << error.what() << '\n';
    return 1;
  }
}
