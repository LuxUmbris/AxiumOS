#pragma once

#include "config.hpp"
#include "tokenizer.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace axir {

enum SlotClass { Integer, Floating };
enum OperandKind { IntegerSlot, FloatSlot, Immediate, Label };

struct Operand {
  OperandKind kind;
  std::uint64_t slot = 0;
  std::uint64_t immediate = 0;
  std::string label;
};

struct Instruction {
  std::string opcode;
  std::vector<Operand> operands;
  std::size_t line = 0;
};

struct DataObject {
  std::string name;
  std::vector<std::uint8_t> bytes;
  std::size_t line = 0;
};

struct Function {
  std::string name;
  std::vector<Operand> parameters;
  std::size_t first_instruction = 0;
  std::size_t line = 0;
};

struct Program {
  std::vector<Instruction> instructions;
  std::vector<DataObject> data;
  std::unordered_map<std::string, std::size_t> labels;
  std::unordered_map<std::string, Function> functions;
};

Program parse(const std::vector<Token> &tokens);

void validate(const Program &program);
std::string dump(const Program &program);
int run(const Program &program, const RunConfig &config);

} // namespace axir
