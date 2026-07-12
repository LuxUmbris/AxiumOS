#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace axir {

enum TokenKind {
  Word,
  String,
  Comma,
  Colon,
  EndOfLine,
  EndOfFile,
};

struct Token {
  TokenKind kind;
  std::string text;
  std::size_t line;
  std::size_t column;
};

std::vector<Token> tokenize(std::string_view source);

} // namespace axir
