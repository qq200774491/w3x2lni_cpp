#include "converter/jass2lua/jass_tokenizer.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace w3x_toolkit::converter {

namespace {

const std::vector<std::string>& GetJassKeywords() {
  static const std::vector<std::string> kKeywords = {
      "function", "endfunction", "takes",    "returns",  "return",
      "local",    "set",         "call",     "if",       "then",
      "else",     "elseif",      "endif",    "loop",     "endloop",
      "exitwhen", "globals",     "endglobals","constant", "native",
      "type",     "extends",     "array",    "and",      "or",
      "not",      "null",        "true",     "false",    "nothing",
  };
  return kKeywords;
}

}  // namespace

core::Result<std::vector<JassToken>> TokenizeJass(std::string_view source) {
  std::vector<JassToken> tokens;
  tokens.reserve(source.size() / 4);

  const auto& keywords = GetJassKeywords();
  std::size_t i = 0;
  std::size_t line = 1;

  while (i < source.size()) {
    char ch = source[i];

    // Newline.
    if (ch == '\r' || ch == '\n') {
      if (ch == '\r' && i + 1 < source.size() && source[i + 1] == '\n') ++i;
      tokens.push_back({JassTokenType::kNewline, "\n", line});
      ++line;
      ++i;
      continue;
    }

    // Whitespace.
    if (ch == ' ' || ch == '\t') { ++i; continue; }

    // Line comment.
    if (ch == '/' && i + 1 < source.size() && source[i + 1] == '/') {
      std::size_t start = i;
      while (i < source.size() && source[i] != '\r' && source[i] != '\n') ++i;
      tokens.push_back({JassTokenType::kComment,
                         std::string(source.substr(start, i - start)), line});
      continue;
    }

    // String literal.
    if (ch == '"') {
      ++i;
      std::string text("\"");
      while (i < source.size() && source[i] != '"') {
        if (source[i] == '\\' && i + 1 < source.size()) {
          text += source[i++];
          text += source[i++];
        } else {
          text += source[i++];
        }
      }
      if (i < source.size()) { text += '"'; ++i; }
      tokens.push_back({JassTokenType::kStringLiteral, std::move(text), line});
      continue;
    }

    // Raw-code literal 'xxxx'.
    if (ch == '\'') {
      std::size_t start = i++;
      while (i < source.size() && source[i] != '\'') ++i;
      if (i < source.size()) ++i;
      tokens.push_back({JassTokenType::kRawCodeLiteral,
                         std::string(source.substr(start, i - start)), line});
      continue;
    }

    // Numbers.
    if (std::isdigit(static_cast<unsigned char>(ch)) || ch == '.') {
      if (ch == '.' &&
          (i + 1 >= source.size() ||
           !std::isdigit(static_cast<unsigned char>(source[i + 1])))) {
        tokens.push_back({JassTokenType::kPunctuation, ".", line});
        ++i;
        continue;
      }
      std::size_t start = i;
      bool has_dot = (ch == '.');
      if (ch == '0' && i + 1 < source.size() &&
          (source[i + 1] == 'x' || source[i + 1] == 'X')) {
        i += 2;
        while (i < source.size() &&
               std::isxdigit(static_cast<unsigned char>(source[i]))) ++i;
      } else {
        ++i;
        while (i < source.size()) {
          if (source[i] == '.' && !has_dot) { has_dot = true; ++i; }
          else if (std::isdigit(static_cast<unsigned char>(source[i]))) ++i;
          else break;
        }
      }
      tokens.push_back({has_dot ? JassTokenType::kRealLiteral
                                : JassTokenType::kIntegerLiteral,
                         std::string(source.substr(start, i - start)), line});
      continue;
    }

    // Dollar-prefixed hex ($FF).
    if (ch == '$') {
      ++i;
      std::size_t hex_start = i;
      while (i < source.size() &&
             std::isxdigit(static_cast<unsigned char>(source[i]))) ++i;
      tokens.push_back({JassTokenType::kIntegerLiteral,
                         "0x" + std::string(source.substr(hex_start,
                                                          i - hex_start)),
                         line});
      continue;
    }

    // Identifiers / keywords.
    if (std::isalpha(static_cast<unsigned char>(ch)) || ch == '_') {
      std::size_t start = i++;
      while (i < source.size() &&
             (std::isalnum(static_cast<unsigned char>(source[i])) ||
              source[i] == '_')) ++i;
      std::string word(source.substr(start, i - start));
      bool is_kw = std::find(keywords.begin(), keywords.end(), word) !=
                   keywords.end();
      tokens.push_back({is_kw ? JassTokenType::kKeyword
                               : JassTokenType::kIdentifier,
                         std::move(word), line});
      continue;
    }

    // Two-character operators.
    if (i + 1 < source.size()) {
      std::string two{ch, source[i + 1]};
      if (two == "==" || two == "!=" || two == "<=" || two == ">=") {
        tokens.push_back({JassTokenType::kOperator, std::move(two), line});
        i += 2;
        continue;
      }
    }

    // Single-character operators.
    if (std::string_view("+-*/<>=").find(ch) != std::string_view::npos) {
      tokens.push_back({JassTokenType::kOperator, std::string(1, ch), line});
      ++i;
      continue;
    }

    // Punctuation.
    if (std::string_view("()[],.").find(ch) != std::string_view::npos) {
      tokens.push_back(
          {JassTokenType::kPunctuation, std::string(1, ch), line});
      ++i;
      continue;
    }

    ++i;  // skip unknown
  }

  tokens.push_back({JassTokenType::kEndOfFile, "", line});
  return tokens;
}

}  // namespace w3x_toolkit::converter
