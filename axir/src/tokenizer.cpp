#include "tokenizer.hpp"

#include <cctype>
#include <stdexcept>

namespace axir {

std::vector<Token> tokenize(std::string_view source) {
  std::vector<Token> tokens;
  std::size_t pos = 0;
  std::size_t line = 1;
  std::size_t column = 1;

  const auto push = [&](TokenKind kind, std::string text, std::size_t token_line,
                        std::size_t token_column) {
    tokens.push_back({kind, std::move(text), token_line, token_column});
  };

  while (pos < source.size()) {
    const char current = source[pos];
    if (current == '\r') {
      ++pos;
      continue;
    }
    if (current == '\n') {
      push(TokenKind::EndOfLine, "", line, column);
      ++pos;
      ++line;
      column = 1;
      continue;
    }
    if (std::isspace(static_cast<unsigned char>(current))) {
      ++pos;
      ++column;
      continue;
    }
    if (current == '/' && pos + 1 < source.size() && source[pos + 1] == '/') {
      while (pos < source.size() && source[pos] != '\n') {
        ++pos;
        ++column;
      }
      continue;
    }
    if (current == '#') {
      while (pos < source.size() && source[pos] != '\n') {
        ++pos;
        ++column;
      }
      continue;
    }
    if (current == ',') {
      push(TokenKind::Comma, ",", line, column);
      ++pos;
      ++column;
      continue;
    }
    if (current == ':') {
      push(TokenKind::Colon, ":", line, column);
      ++pos;
      ++column;
      continue;
    }
    if (current == '"') {
      const std::size_t token_line = line;
      const std::size_t token_column = column;
      ++pos;
      ++column;
      std::string value;
      bool closed = false;
      while (pos < source.size()) {
        char ch = source[pos++];
        ++column;
        if (ch == '"') {
          closed = true;
          break;
        }
        if (ch == '\n') {
          throw std::runtime_error("line " + std::to_string(token_line) +
                                   ": unterminated string literal");
        }
        if (ch != '\\') {
          value.push_back(ch);
          continue;
        }
        if (pos == source.size()) {
          throw std::runtime_error("line " + std::to_string(token_line) +
                                   ": unterminated escape sequence");
        }
        const char escaped = source[pos++];
        ++column;
        switch (escaped) {
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case '0': value.push_back('\0'); break;
        case '\\': value.push_back('\\'); break;
        case '"': value.push_back('"'); break;
        default:
          throw std::runtime_error("line " + std::to_string(token_line) +
                                   ": unsupported escape sequence");
        }
      }
      if (!closed) {
        throw std::runtime_error("line " + std::to_string(token_line) +
                                 ": unterminated string literal");
      }
      push(TokenKind::String, std::move(value), token_line, token_column);
      continue;
    }

    const std::size_t token_line = line;
    const std::size_t token_column = column;
    std::string word;
    while (pos < source.size()) {
      const char ch = source[pos];
      if (std::isspace(static_cast<unsigned char>(ch)) || ch == ',' || ch == ':' || ch == '#' ||
          (ch == '/' && pos + 1 < source.size() && source[pos + 1] == '/')) {
        break;
      }
      word.push_back(ch);
      ++pos;
      ++column;
    }
    if (word.empty()) {
      throw std::runtime_error("line " + std::to_string(line) + ": unexpected character");
    }
    push(TokenKind::Word, std::move(word), token_line, token_column);
  }

  if (tokens.empty() || tokens.back().kind != TokenKind::EndOfLine) {
    tokens.push_back({TokenKind::EndOfLine, "", line, column});
  }
  tokens.push_back({TokenKind::EndOfFile, "", line, column});
  return tokens;
}

} // namespace axir
