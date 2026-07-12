#include "codegen.hpp"

#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace {

void usage(const char *argv0) {
  std::cerr << "Usage: " << argv0 << " target <target_name>\n"
            << "       " << argv0 << " <check|run|dump> <program.axir> [options]\n"
            << "       " << argv0 << " <program.axir>  # same as check\n\n"
            << "Options:\n"
            << "  --target <target_name>  Validate ./targets/<target_name>.yml beside axirc\n"
            << "  --memory <bytes>        Reference memory size for run (default: 65536)\n"
            << "  --max-steps <n>         Reference step limit for run (default: 1000000)\n"
            << "  -h, --help              Show this help text\n";
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

bool command_name(const std::string &name) {
  return name == "check" || name == "run" || name == "dump";
}

bool parse_options(int argc, char **argv, int &arg, axir::RunConfig &run_config, std::string &target_name) {
  while (arg < argc) {
    const std::string option = argv[arg++];
    if (option == "--target" && arg < argc) {
      target_name = argv[arg++];
    } else if ((option == "--memory" || option == "--max-steps") && arg < argc) {
      std::size_t value = 0;
      if (!parse_size(argv[arg++], value) || value == 0) {
        std::cerr << "error: " << option << " expects a positive integer\n";
        return false;
      }
      if (option == "--memory") run_config.memory_size = value;
      else run_config.max_steps = value;
    } else {
      std::cerr << "error: unknown or incomplete option '" << option << "'\n";
      return false;
    }
  }
  return true;
}

} // namespace

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
    return 2;
  }

  const std::string first = argv[1];
  if (first == "--help" || first == "-h") {
    usage(argv[0]);
    return 0;
  }

  try {
    if (first == "target") {
      if (argc != 3) {
        usage(argv[0]);
        return 2;
      }
      const axir::TargetConfig target = axir::load_target_config(argv[0], argv[2]);
      std::cout << "AXIR target loaded: " << target.name << " (" << target.path.string() << ")\n";
      return 0;
    }

    std::string command = "check";
    int arg = 1;
    if (command_name(first)) {
      command = first;
      ++arg;
    }
    if (arg >= argc) {
      usage(argv[0]);
      return 2;
    }

    const std::string source_path = argv[arg++];
    axir::RunConfig run_config;
    std::string target_name;
    if (!parse_options(argc, argv, arg, run_config, target_name)) return 2;
    if (!target_name.empty()) axir::load_target_config(argv[0], target_name);

    std::ifstream input(source_path, std::ios::binary);
    if (!input) throw std::runtime_error("cannot open '" + source_path + "'");
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
    const int status = axir::run(program, run_config);
    std::cout << "AXIR program exited with status " << status << "\n";
    return status;
  } catch (const std::exception &error) {
    std::cerr << "axirc: " << error.what() << '\n';
    return 1;
  }
}
