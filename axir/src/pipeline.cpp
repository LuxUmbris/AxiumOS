#include "pipeline.hpp"

#include <stdexcept>
#include <utility>

namespace axir {
namespace {

bool no_op_move(const Instruction &instruction) {
  if (instruction.opcode != "mov" || instruction.operands.size() != 2) return false;
  const Operand &destination = instruction.operands[0];
  const Operand &source = instruction.operands[1];
  return destination.kind == IntegerSlot && source.kind == IntegerSlot && destination.slot == source.slot;
}

} // namespace

Program link_modules(const std::vector<Program> &modules) {
  if (modules.empty()) throw std::runtime_error("link requires at least one AXIR module");

  Program linked;
  for (const Program &module : modules) {
    const std::size_t instruction_base = linked.instructions.size();
    for (const DataObject &data : module.data) {
      for (const DataObject &existing : linked.data) {
        if (existing.name == data.name) throw std::runtime_error("duplicate data symbol during link: '" + data.name + "'");
      }
      linked.data.push_back(data);
    }
    linked.instructions.insert(linked.instructions.end(), module.instructions.begin(), module.instructions.end());
    for (const auto &[name, offset] : module.labels) {
      if (linked.labels.count(name)) throw std::runtime_error("duplicate label during link: '" + name + "'");
      linked.labels.emplace(name, instruction_base + offset);
    }
    for (const auto &[name, function] : module.functions) {
      if (linked.functions.count(name)) throw std::runtime_error("duplicate function during link: '" + name + "'");
      Function resolved = function;
      resolved.first_instruction += instruction_base;
      linked.functions.emplace(name, std::move(resolved));
    }
  }
  validate(linked);
  return linked;
}

Program optimize(Program program) {
  std::vector<std::size_t> remap(program.instructions.size() + 1);
  std::vector<Instruction> optimized;
  optimized.reserve(program.instructions.size());
  for (std::size_t index = 0; index < program.instructions.size(); ++index) {
    remap[index] = optimized.size();
    if (!no_op_move(program.instructions[index])) optimized.push_back(std::move(program.instructions[index]));
  }
  remap.back() = optimized.size();
  for (auto &[name, offset] : program.labels) {
    (void)name;
    offset = remap[offset];
  }
  for (auto &[name, function] : program.functions) {
    (void)name;
    function.first_instruction = remap[function.first_instruction];
  }
  program.instructions = std::move(optimized);
  validate(program);
  return program;
}

} // namespace axir
