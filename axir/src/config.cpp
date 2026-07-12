#include "config.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <stdexcept>

namespace axir {
namespace {

std::string trim(const std::string &text) {
  const auto first = std::find_if_not(text.begin(), text.end(), [](unsigned char c) { return std::isspace(c); });
  const auto last = std::find_if_not(text.rbegin(), text.rend(), [](unsigned char c) { return std::isspace(c); }).base();
  return first < last ? std::string(first, last) : std::string();
}

bool valid_target_name(std::string_view name) {
  return !name.empty() && std::all_of(name.begin(), name.end(), [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '-';
  });
}

std::string scalar_value(const std::string &line, const std::string &key) {
  const std::string prefix = key + ":";
  if (line.rfind(prefix, 0) != 0) return {};
  return trim(line.substr(prefix.size()));
}

} // namespace

TargetConfig load_target_config(std::string_view executable_path, std::string_view target_name) {
  if (!valid_target_name(target_name)) throw std::runtime_error("invalid target name '" + std::string(target_name) + "'");

  const std::filesystem::path executable = std::filesystem::absolute(std::filesystem::path(executable_path));
  const std::filesystem::path path = executable.parent_path() / "targets" / (std::string(target_name) + ".yml");
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open target configuration '" + path.string() + "'");

  std::string schema;
  std::string name;
  bool encoding_section = false;
  bool byte_template = false;
  std::string line;
  while (std::getline(input, line)) {
    const std::string text = trim(line);
    if (text.empty() || text[0] == '#') continue;
    if (schema.empty()) schema = scalar_value(text, "schema_version");
    if (name.empty()) name = scalar_value(text, "name");
    if (text == "encoding:") encoding_section = true;
    if (encoding_section && text.find("bytes:") != std::string::npos) byte_template = true;
  }

  if (schema != "1") throw std::runtime_error("target configuration '" + path.string() + "' must declare schema_version: 1");
  if (name != target_name) throw std::runtime_error("target configuration name '" + name + "' does not match requested target '" + std::string(target_name) + "'");
  if (!encoding_section || !byte_template) throw std::runtime_error("target configuration '" + path.string() + "' must declare byte-based encoding templates");
  return {name, path};
}

} // namespace axir
