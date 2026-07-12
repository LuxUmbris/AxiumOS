#include "config.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <utility>

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

std::vector<std::uint8_t> byte_sequence(const std::string &line, const std::string &marker) {
  const std::size_t position = line.find(marker);
  if (position == std::string::npos) return {};
  const std::size_t start = position + marker.size();
  const std::size_t end = line.find(']', start);
  if (end == std::string::npos) throw std::runtime_error("unterminated byte sequence in target configuration");
  std::vector<std::uint8_t> bytes;
  std::istringstream values(line.substr(start, end - start));
  std::string value;
  while (std::getline(values, value, ',')) {
    const std::string token = trim(value);
    if (token.empty()) throw std::runtime_error("empty byte in target configuration");
    std::size_t consumed = 0;
    const unsigned long parsed = std::stoul(token, &consumed, 0);
    if (consumed != token.size() || parsed > 0xff) throw std::runtime_error("invalid target byte '" + token + "'");
    bytes.push_back(static_cast<std::uint8_t>(parsed));
  }
  return bytes;
}

void parse_syscall_slots(const std::string &line, SyscallTemplate &syscall) {
  const std::size_t marker = line.find("slots: {");
  if (marker == std::string::npos) return;
  const std::size_t start = marker + std::string("slots: {").size();
  const std::size_t end = line.find('}', start);
  if (end == std::string::npos) throw std::runtime_error("unterminated syscall slot mapping in target configuration");
  std::istringstream entries(line.substr(start, end - start));
  std::string entry;
  while (std::getline(entries, entry, ',')) {
    const std::size_t separator = entry.find(':');
    if (separator == std::string::npos) throw std::runtime_error("invalid syscall slot mapping in target configuration");
    const std::string key = trim(entry.substr(0, separator));
    const std::string value = trim(entry.substr(separator + 1));
    std::size_t consumed = 0;
    const unsigned long long slot = std::stoull(value, &consumed, 0);
    if (consumed != value.size()) throw std::runtime_error("invalid syscall slot '" + value + "'");
    if (key == "result") {
      syscall.has_result_slot = true;
      syscall.result_slot = static_cast<std::uint64_t>(slot);
    } else {
      syscall.argument_slots.push_back(static_cast<std::uint64_t>(slot));
    }
  }
}

} // namespace

TargetConfig load_target_config(std::string_view executable_path, std::string_view target_name) {
  if (!valid_target_name(target_name)) throw std::runtime_error("invalid target name '" + std::string(target_name) + "'");

  const std::filesystem::path executable = std::filesystem::absolute(std::filesystem::path(executable_path));
  const std::filesystem::path path = executable.parent_path() / "targets" / (std::string(target_name) + ".yml");
  std::ifstream input(path);
  if (!input) throw std::runtime_error("cannot open target configuration '" + path.string() + "'");

  constexpr std::array<std::string_view, 14> required_syscalls = {
      "read", "write", "read_file", "write_file", "open", "close", "seek", "stat", "remove", "mkdir", "rmdir", "time", "sleep", "exit"};
  std::array<bool, required_syscalls.size()> syscall_sections{};
  std::array<bool, required_syscalls.size()> syscall_bytes{};
  std::array<bool, required_syscalls.size()> syscall_slot_mappings{};
  std::size_t active_syscall = required_syscalls.size();
  std::vector<std::uint8_t> exit_status_prefix;
  std::vector<std::uint8_t> exit_number_sequence;
  std::vector<std::uint8_t> syscall_sequence;
  std::vector<SyscallTemplate> syscalls;
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
    if (text.rfind("mov_edi_imm32:", 0) == 0) exit_status_prefix = byte_sequence(text, "bytes: [");
    if (text.rfind("mov_eax_exit:", 0) == 0) exit_number_sequence = byte_sequence(text, "bytes: [");
    if (text.rfind("syscall:", 0) == 0 && active_syscall == required_syscalls.size()) syscall_sequence = byte_sequence(text, "bytes: [");
    for (std::size_t index = 0; index < required_syscalls.size(); ++index) {
      if (text == "syscall " + std::string(required_syscalls[index]) + ":") {
        syscall_sections[index] = true;
        active_syscall = index;
        syscalls.push_back({std::string(required_syscalls[index]), {}, false, 0, {}, {}});
      }
    }
    if (active_syscall < required_syscalls.size() && text.rfind("slots:", 0) == 0) {
      syscall_slot_mappings[active_syscall] = true;
      parse_syscall_slots(text, syscalls.back());
    }
    if (active_syscall < required_syscalls.size() && text.rfind("bytes:", 0) == 0) {
      syscall_bytes[active_syscall] = true;
      SyscallTemplate &syscall = syscalls.back();
      syscall.load_number = byte_sequence(text, "load_number: [");
      syscall.invoke = byte_sequence(text, "invoke: [");
    }
  }

  if (schema != "1") throw std::runtime_error("target configuration '" + path.string() + "' must declare schema_version: 1");
  if (name != target_name) throw std::runtime_error("target configuration name '" + name + "' does not match requested target '" + std::string(target_name) + "'");
  if (!encoding_section || !byte_template) throw std::runtime_error("target configuration '" + path.string() + "' must declare byte-based encoding templates");
  if (exit_status_prefix.empty() || exit_number_sequence.empty() || syscall_sequence.empty()) throw std::runtime_error("target configuration '" + path.string() + "' is missing process-exit byte templates");
  for (std::size_t index = 0; index < required_syscalls.size(); ++index) {
    if (!syscall_sections[index]) throw std::runtime_error("target configuration '" + path.string() + "' is missing section 'syscall " + std::string(required_syscalls[index]) + "'");
    if (!syscall_bytes[index]) throw std::runtime_error("target configuration '" + path.string() + "' is missing bytes for 'syscall " + std::string(required_syscalls[index]) + "'");
    if (!syscall_slot_mappings[index]) throw std::runtime_error("target configuration '" + path.string() + "' is missing slot mapping for 'syscall " + std::string(required_syscalls[index]) + "'");
  }
  return {name, path, std::move(exit_status_prefix), std::move(exit_number_sequence), std::move(syscall_sequence), std::move(syscalls)};
}

} // namespace axir
