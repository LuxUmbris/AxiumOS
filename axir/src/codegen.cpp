#include "codegen.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace axir {
namespace {

[[noreturn]] void fail(std::size_t line, const std::string &message) {
  throw std::runtime_error("line " + std::to_string(line) + ": " + message);
}

bool identifier(const std::string &text) {
  if (text.empty() || !(std::isalpha(static_cast<unsigned char>(text[0])) || text[0] == '_')) return false;
  return std::all_of(text.begin() + 1, text.end(), [](char c) {
    return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
  });
}

bool number(const std::string &text, std::uint64_t &out) {
  try {
    std::string value = text;
    int base = 10;
    if (value.size() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) base = 16;
    else if (value.size() > 2 && value[0] == '0' && (value[1] == 'b' || value[1] == 'B')) { base = 2; value.erase(0, 2); }
    std::size_t consumed = 0;
    out = std::stoull(value, &consumed, base);
    return consumed == value.size();
  } catch (...) { return false; }
}

bool immediate(const std::string &text, std::uint64_t &out) {
  if (text.empty()) return false;
  if (text[0] != '-') return number(text, out);
  std::uint64_t value = 0;
  if (!number(text.substr(1), value)) return false;
  out = 0 - value;
  return true;
}

Operand parse_operand(const Token &token) {
  const auto &text = token.text;
  if (text.size() >= 4 && (text[0] == 'I' || text[0] == 'F') && text[1] == '[' && text.back() == ']') {
    std::uint64_t slot = 0;
    if (!number(text.substr(2, text.size() - 3), slot)) fail(token.line, "invalid slot '" + text + "'");
    return {text[0] == 'I' ? OperandKind::IntegerSlot : OperandKind::FloatSlot, slot, 0, {}};
  }
  std::uint64_t value = 0;
  if (immediate(text, value)) return {OperandKind::Immediate, 0, value, {}};
  if (!identifier(text)) fail(token.line, "invalid operand '" + text + "'");
  return {OperandKind::Label, 0, 0, text};
}

std::string display(const Operand &operand) {
  if (operand.kind == OperandKind::IntegerSlot) return "I[" + std::to_string(operand.slot) + "]";
  if (operand.kind == OperandKind::FloatSlot) return "F[" + std::to_string(operand.slot) + "]";
  if (operand.kind == OperandKind::Immediate) return std::to_string(static_cast<std::int64_t>(operand.immediate));
  return operand.label;
}

bool int_binary(const std::string &op) {
  static const std::unordered_set<std::string> values = {
      "add", "sub", "mul", "divs", "divu", "mods", "modu", "and", "or", "xor", "shl", "shrl", "shra",
      "cmp_eq", "cmp_ne", "cmp_lts", "cmp_gts", "cmp_les", "cmp_ges", "cmp_ltu", "cmp_gtu", "cmp_leu", "cmp_geu"};
  return values.count(op) != 0;
}

bool float_binary(const std::string &op) { return op == "addf" || op == "subf" || op == "mulf" || op == "divf"; }
bool float_compare(const std::string &op) { return op == "cmp_eqf" || op == "cmp_nef" || op == "cmp_ltf" || op == "cmp_gtf" || op == "cmp_lef" || op == "cmp_gef"; }
bool int_load(const std::string &op) { return op == "load8" || op == "load16" || op == "load32" || op == "load64" || op == "load8u" || op == "load16u" || op == "load32u" || op == "load8s" || op == "load16s" || op == "load32s"; }
bool int_store(const std::string &op) { return op == "store8" || op == "store16" || op == "store32" || op == "store64"; }

void count(const Instruction &in, std::size_t expected) {
  if (in.operands.size() != expected) fail(in.line, in.opcode + " expects " + std::to_string(expected) + " operand(s)");
}
void kind(const Instruction &in, std::size_t index, OperandKind expected) {
  if (in.operands[index].kind != expected) fail(in.line, in.opcode + " has an operand of the wrong slot class");
}
void int_or_imm(const Instruction &in, std::size_t index) {
  if (in.operands[index].kind != OperandKind::IntegerSlot && in.operands[index].kind != OperandKind::Immediate) fail(in.line, in.opcode + " expects an integer slot or immediate");
}

std::uint64_t sdiv(std::uint64_t lhs, std::uint64_t rhs) {
  const auto a = static_cast<std::int64_t>(lhs), b = static_cast<std::int64_t>(rhs);
  if (!b) throw std::runtime_error("signed division by zero");
  if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return lhs;
  return static_cast<std::uint64_t>(a / b);
}
std::uint64_t smod(std::uint64_t lhs, std::uint64_t rhs) {
  const auto a = static_cast<std::int64_t>(lhs), b = static_cast<std::int64_t>(rhs);
  if (!b) throw std::runtime_error("signed modulus by zero");
  if (a == std::numeric_limits<std::int64_t>::min() && b == -1) return 0;
  return static_cast<std::uint64_t>(a % b);
}

} // namespace

Program parse(const std::vector<Token> &tokens) {
  Program program;
  std::size_t cursor = 0;
  std::set<std::string> data_names;
  while (tokens[cursor].kind != TokenKind::EndOfFile) {
    while (tokens[cursor].kind == TokenKind::EndOfLine) ++cursor;
    if (tokens[cursor].kind == TokenKind::EndOfFile) break;
    std::vector<Token> line;
    while (tokens[cursor].kind != TokenKind::EndOfLine && tokens[cursor].kind != TokenKind::EndOfFile) line.push_back(tokens[cursor++]);
    if (tokens[cursor].kind == TokenKind::EndOfLine) ++cursor;
    const std::size_t source_line = line.front().line;
    if (line.size() == 2 && line[0].kind == TokenKind::Word && line[1].kind == TokenKind::Colon) {
      if (!identifier(line[0].text)) fail(source_line, "invalid label");
      if (!program.labels.emplace(line[0].text, program.instructions.size()).second) fail(source_line, "duplicate label '" + line[0].text + "'");
      continue;
    }
    if (line[0].text == "func") {
      if (line.size() < 2 || line[1].kind != TokenKind::Word || !identifier(line[1].text)) fail(source_line, "func expects an identifier");
      if (program.labels.count(line[1].text)) fail(source_line, "duplicate function '" + line[1].text + "'");
      Function function{line[1].text, {}, program.instructions.size(), source_line};
      bool need = false;
      for (std::size_t i = 2; i < line.size(); ++i) {
        if (line[i].kind == TokenKind::Comma) { if (need) fail(source_line, "unexpected comma in func"); need = true; continue; }
        if (!need || line[i].kind != TokenKind::Word) fail(source_line, "function parameters must be comma-separated slots");
        Operand param = parse_operand(line[i]);
        if (param.kind != OperandKind::IntegerSlot && param.kind != OperandKind::FloatSlot) fail(source_line, "function parameters must be slots");
        function.parameters.push_back(param); need = false;
      }
      if (need) fail(source_line, "trailing comma in func");
      program.labels.emplace(function.name, function.first_instruction);
      program.functions.emplace(function.name, std::move(function));
      continue;
    }
    if (line[0].text == "data") {
      if (line.size() < 4 || line[1].kind != TokenKind::Word || line[2].kind != TokenKind::Colon || !identifier(line[1].text)) fail(source_line, "data syntax is: data name: reserve N | values");
      if (!data_names.insert(line[1].text).second) fail(source_line, "duplicate data label '" + line[1].text + "'");
      DataObject data{line[1].text, {}, source_line};
      if (line[3].kind == TokenKind::Word && line[3].text == "reserve") {
        std::uint64_t bytes = 0;
        if (line.size() != 5 || line[4].kind != TokenKind::Word || !number(line[4].text, bytes) || bytes > std::numeric_limits<std::size_t>::max()) fail(source_line, "reserve expects one valid byte count");
        data.bytes.resize(static_cast<std::size_t>(bytes));
      } else {
        bool need = true;
        for (std::size_t i = 3; i < line.size(); ++i) {
          if (line[i].kind == TokenKind::Comma) { if (need) fail(source_line, "unexpected comma in data"); need = true; continue; }
          if (!need) fail(source_line, "data values must be comma-separated");
          if (line[i].kind == TokenKind::String) data.bytes.insert(data.bytes.end(), line[i].text.begin(), line[i].text.end());
          else if (line[i].kind == TokenKind::Word) { std::uint64_t byte = 0; if (!number(line[i].text, byte) || byte > 255) fail(source_line, "data bytes must be in 0..255"); data.bytes.push_back(static_cast<std::uint8_t>(byte)); }
          else fail(source_line, "invalid data value");
          need = false;
        }
        if (need) fail(source_line, "trailing comma in data");
      }
      program.data.push_back(std::move(data));
      continue;
    }
    if (line[0].kind != TokenKind::Word) fail(source_line, "instruction must start with an opcode");
    Instruction in{line[0].text, {}, source_line};
    bool need = true;
    for (std::size_t i = 1; i < line.size(); ++i) {
      if (line[i].kind == TokenKind::Comma) { if (need) fail(source_line, "unexpected comma"); need = true; continue; }
      if (!need || line[i].kind != TokenKind::Word) fail(source_line, "operands must be comma-separated");
      in.operands.push_back(parse_operand(line[i])); need = false;
    }
    if (need && line.size() > 1) fail(source_line, "trailing comma");
    program.instructions.push_back(std::move(in));
  }
  return program;
}

void validate(const Program &program) {
  const auto label = [&](const Instruction &in, std::size_t i, const std::string &what) {
    kind(in, i, OperandKind::Label); if (!program.labels.count(in.operands[i].label)) fail(in.line, "unknown " + what + " '" + in.operands[i].label + "'");
  };
  const auto data_label = [&](const Instruction &in) {
    kind(in, 1, OperandKind::Label);
    if (!std::any_of(program.data.begin(), program.data.end(), [&](const DataObject &data) { return data.name == in.operands[1].label; })) fail(in.line, "unknown data label '" + in.operands[1].label + "'");
  };
  for (const auto &in : program.instructions) {
    const auto &op = in.opcode;
    if (int_load(op) || int_store(op)) { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::IntegerSlot); }
    else if (op == "loadf32" || op == "loadf64") { count(in, 2); kind(in, 0, OperandKind::FloatSlot); kind(in, 1, OperandKind::IntegerSlot); }
    else if (op == "storef32" || op == "storef64") { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::FloatSlot); }
    else if (op == "addr") { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); data_label(in); }
    else if (int_binary(op)) { count(in, 3); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::IntegerSlot); int_or_imm(in, 2); }
    else if (op == "mov") { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::IntegerSlot); }
    else if (float_binary(op)) { count(in, 3); kind(in, 0, OperandKind::FloatSlot); kind(in, 1, OperandKind::FloatSlot); kind(in, 2, OperandKind::FloatSlot); }
    else if (op == "movf") { count(in, 2); kind(in, 0, OperandKind::FloatSlot); kind(in, 1, OperandKind::FloatSlot); }
    else if (op == "itof") { count(in, 2); kind(in, 0, OperandKind::FloatSlot); kind(in, 1, OperandKind::IntegerSlot); }
    else if (op == "ftoi") { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::FloatSlot); }
    else if (float_compare(op)) { count(in, 3); kind(in, 0, OperandKind::IntegerSlot); kind(in, 1, OperandKind::FloatSlot); kind(in, 2, OperandKind::FloatSlot); }
    else if (op == "jmp") { count(in, 1); label(in, 0, "jump label"); }
    else if (op == "cjump") { count(in, 2); kind(in, 0, OperandKind::IntegerSlot); label(in, 1, "jump label"); }
    else if (op == "call") { if (in.operands.empty()) fail(in.line, "call expects a target"); label(in, 0, "call target"); for (std::size_t i = 1; i < in.operands.size(); ++i) if (in.operands[i].kind != OperandKind::IntegerSlot && in.operands[i].kind != OperandKind::FloatSlot) fail(in.line, "call arguments must be slots"); }
    else if (op == "call_ptr") { if (in.operands.empty()) fail(in.line, "call_ptr expects a target slot"); kind(in, 0, OperandKind::IntegerSlot); for (std::size_t i = 1; i < in.operands.size(); ++i) if (in.operands[i].kind != OperandKind::IntegerSlot && in.operands[i].kind != OperandKind::FloatSlot) fail(in.line, "call_ptr arguments must be slots"); }
    else if (op == "ret" || op == "hlt") { count(in, 1); int_or_imm(in, 0); }
    else if (op == "ret_void") count(in, 0);
    else if (op == "syscall") { count(in, 1); kind(in, 0, OperandKind::Label); }
    else fail(in.line, "unknown AXIR opcode '" + op + "'");
  }
}

std::string dump(const Program &program) {
  std::ostringstream out;
  for (const auto &data : program.data) { out << "data " << data.name << ":"; for (std::size_t i = 0; i < data.bytes.size(); ++i) out << (i ? ", " : " ") << unsigned(data.bytes[i]); out << '\n'; }
  std::unordered_multimap<std::size_t, std::string> labels;
  for (const auto &[name, pc] : program.labels) labels.emplace(pc, name);
  for (std::size_t pc = 0; pc <= program.instructions.size(); ++pc) {
    auto [first, last] = labels.equal_range(pc); for (auto it = first; it != last; ++it) out << it->second << ":\n";
    if (pc == program.instructions.size()) break;
    const auto &in = program.instructions[pc]; out << "  " << in.opcode;
    for (std::size_t i = 0; i < in.operands.size(); ++i) {
      out << (i ? ", " : " ") << display(in.operands[i]);
    }
    out << '\n';
  }
  return out.str();
}

int run(const Program &program, const RunConfig &config) {
  if (!config.memory_size) throw std::runtime_error("memory size must be positive");
  std::vector<std::uint8_t> memory(config.memory_size);
  std::unordered_map<std::string, std::uint64_t> addresses;
  std::size_t cursor = 0;
  for (const auto &data : program.data) { if (data.bytes.size() > memory.size() - cursor) throw std::runtime_error("static data exceeds configured memory"); addresses[data.name] = cursor; std::copy(data.bytes.begin(), data.bytes.end(), memory.begin() + cursor); cursor += data.bytes.size(); }
  std::unordered_map<std::uint64_t, std::uint64_t> islots;
  std::unordered_map<std::uint64_t, double> fslots;
  const auto get_i = [&](const Operand &op) { return op.kind == OperandKind::Immediate ? op.immediate : islots[op.slot]; };
  const auto read = [&](std::uint64_t address, unsigned bytes) { if (address > memory.size() || bytes > memory.size() - std::size_t(address)) throw std::runtime_error("memory read out of bounds"); std::uint64_t value = 0; for (unsigned i = 0; i < bytes; ++i) value |= std::uint64_t(memory[std::size_t(address) + i]) << (8 * i); return value; };
  const auto write = [&](std::uint64_t address, std::uint64_t value, unsigned bytes) { if (address > memory.size() || bytes > memory.size() - std::size_t(address)) throw std::runtime_error("memory write out of bounds"); for (unsigned i = 0; i < bytes; ++i) memory[std::size_t(address) + i] = std::uint8_t(value >> (8 * i)); };
  std::size_t pc = 0;
  for (std::size_t steps = 0; steps < config.max_steps; ++steps) {
    if (pc >= program.instructions.size()) return 0;
    const auto &in = program.instructions[pc++]; const auto &op = in.opcode;
    const auto error = [&](const std::string &message) { throw std::runtime_error("line " + std::to_string(in.line) + ": " + message); };
    if (int_load(op)) { unsigned bytes = op.find("16") != std::string::npos ? 2 : op.find("32") != std::string::npos ? 4 : op.find("64") != std::string::npos ? 8 : 1; std::uint64_t value = read(get_i(in.operands[1]), bytes); if ((op == "load8s" || op == "load16s" || op == "load32s") && (value & (std::uint64_t(1) << (bytes * 8 - 1)))) value |= ~((std::uint64_t(1) << (bytes * 8)) - 1); islots[in.operands[0].slot] = value; }
    else if (int_store(op)) { const unsigned bytes = op == "store8" ? 1 : op == "store16" ? 2 : op == "store32" ? 4 : 8; write(get_i(in.operands[0]), get_i(in.operands[1]), bytes); }
    else if (op == "loadf32" || op == "loadf64") { if (op == "loadf32") { std::uint32_t bits = std::uint32_t(read(get_i(in.operands[1]), 4)); float value; std::memcpy(&value, &bits, sizeof value); fslots[in.operands[0].slot] = value; } else { std::uint64_t bits = read(get_i(in.operands[1]), 8); double value; std::memcpy(&value, &bits, sizeof value); fslots[in.operands[0].slot] = value; } }
    else if (op == "storef32" || op == "storef64") { if (op == "storef32") { float value = float(fslots[in.operands[1].slot]); std::uint32_t bits; std::memcpy(&bits, &value, sizeof bits); write(get_i(in.operands[0]), bits, 4); } else { double value = fslots[in.operands[1].slot]; std::uint64_t bits; std::memcpy(&bits, &value, sizeof bits); write(get_i(in.operands[0]), bits, 8); } }
    else if (op == "addr") islots[in.operands[0].slot] = addresses.at(in.operands[1].label);
    else if (int_binary(op)) { const std::uint64_t a = get_i(in.operands[1]), b = get_i(in.operands[2]); std::uint64_t value = 0; if (op == "add") value = a + b; else if (op == "sub") value = a - b; else if (op == "mul") value = a * b; else if (op == "divs") value = sdiv(a, b); else if (op == "divu") { if (!b) error("unsigned division by zero"); value = a / b; } else if (op == "mods") value = smod(a, b); else if (op == "modu") { if (!b) error("unsigned modulus by zero"); value = a % b; } else if (op == "and") value = a & b; else if (op == "or") value = a | b; else if (op == "xor") value = a ^ b; else if (op == "shl") value = a << (b & 63); else if (op == "shrl") value = a >> (b & 63); else if (op == "shra") { unsigned n = b & 63; value = a >> n; if (n && (a >> 63)) value |= ~std::uint64_t(0) << (64 - n); } else if (op == "cmp_eq") value = a == b; else if (op == "cmp_ne") value = a != b; else if (op == "cmp_lts") value = std::int64_t(a) < std::int64_t(b); else if (op == "cmp_gts") value = std::int64_t(a) > std::int64_t(b); else if (op == "cmp_les") value = std::int64_t(a) <= std::int64_t(b); else if (op == "cmp_ges") value = std::int64_t(a) >= std::int64_t(b); else if (op == "cmp_ltu") value = a < b; else if (op == "cmp_gtu") value = a > b; else if (op == "cmp_leu") value = a <= b; else value = a >= b; islots[in.operands[0].slot] = value; }
    else if (op == "mov") islots[in.operands[0].slot] = islots[in.operands[1].slot];
    else if (float_binary(op)) { const double a = fslots[in.operands[1].slot], b = fslots[in.operands[2].slot]; fslots[in.operands[0].slot] = op == "addf" ? a + b : op == "subf" ? a - b : op == "mulf" ? a * b : a / b; }
    else if (op == "movf") fslots[in.operands[0].slot] = fslots[in.operands[1].slot];
    else if (op == "itof") fslots[in.operands[0].slot] = double(std::int64_t(islots[in.operands[1].slot]));
    else if (op == "ftoi") { double value = fslots[in.operands[1].slot]; if (!std::isfinite(value) || value < double(std::numeric_limits<std::int64_t>::min()) || value > double(std::numeric_limits<std::int64_t>::max())) error("ftoi value is out of range"); islots[in.operands[0].slot] = std::uint64_t(std::int64_t(value)); }
    else if (float_compare(op)) { const double a = fslots[in.operands[1].slot], b = fslots[in.operands[2].slot]; islots[in.operands[0].slot] = op == "cmp_eqf" ? a == b : op == "cmp_nef" ? a != b : op == "cmp_ltf" ? a < b : op == "cmp_gtf" ? a > b : op == "cmp_lef" ? a <= b : a >= b; }
    else if (op == "jmp") pc = program.labels.at(in.operands[0].label);
    else if (op == "cjump") { if (islots[in.operands[0].slot]) pc = program.labels.at(in.operands[1].label); }
    else if (op == "ret" || op == "hlt") return int(get_i(in.operands[0]));
    else if (op == "ret_void") return 0;
    else if (op == "call" || op == "call_ptr") error("calls require a backend ABI and cannot run in the reference interpreter");
    else if (op == "syscall") error("syscalls are backend-defined and cannot run in the reference interpreter");
  }
  throw std::runtime_error("execution exceeded configured step limit");
}

} // namespace axir
