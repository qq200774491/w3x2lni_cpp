#include "parser/jass/jass_parser.h"

#include <cctype>
#include <string>
#include <unordered_map>

namespace w3x_toolkit::parser::jass {

namespace {

const std::unordered_map<std::string, TokenType>& Keywords() {
  static const std::unordered_map<std::string, TokenType> kw = {
      {"function", TokenType::kFunction},
      {"endfunction", TokenType::kEndfunction},
      {"if", TokenType::kIf},
      {"then", TokenType::kThen},
      {"else", TokenType::kElse},
      {"elseif", TokenType::kElseif},
      {"endif", TokenType::kEndif},
      {"loop", TokenType::kLoop},
      {"endloop", TokenType::kEndloop},
      {"exitwhen", TokenType::kExitwhen},
      {"return", TokenType::kReturn},
      {"call", TokenType::kCall},
      {"set", TokenType::kSet},
      {"local", TokenType::kLocal},
      {"type", TokenType::kType},
      {"extends", TokenType::kExtends},
      {"globals", TokenType::kGlobals},
      {"endglobals", TokenType::kEndglobals},
      {"native", TokenType::kNative},
      {"constant", TokenType::kConstant},
      {"takes", TokenType::kTakes},
      {"returns", TokenType::kReturns},
      {"nothing", TokenType::kNothing},
      {"array", TokenType::kArray},
      {"not", TokenType::kNot},
      {"and", TokenType::kAnd},
      {"or", TokenType::kOr},
      {"null", TokenType::kNull},
      {"true", TokenType::kTrue},
      {"false", TokenType::kFalse},
      {"debug", TokenType::kDebug},
  };
  return kw;
}

class Lexer {
 public:
  explicit Lexer(std::string_view source) : source_(source) {}

  core::Result<std::vector<Token>> Tokenize() {
    std::vector<Token> tokens;
    while (pos_ < source_.size()) {
      SkipWhitespace();
      if (pos_ >= source_.size()) break;

      char c = source_[pos_];

      // Newline.
      if (c == '\r' || c == '\n') {
        Token tok{TokenType::kNewline, "", line_, column_};
        if (c == '\r' && Peek(1) == '\n') {
          Advance();
        }
        Advance();
        tokens.push_back(std::move(tok));
        continue;
      }

      // Comment.
      if (c == '/' && Peek(1) == '/') {
        SkipLineComment();
        continue;
      }

      // String literal.
      if (c == '"') {
        auto tok = ReadString();
        if (!tok.has_value()) return std::unexpected(tok.error());
        tokens.push_back(std::move(tok.value()));
        continue;
      }

      // Character literal (integer-256).
      if (c == '\'') {
        auto tok = ReadCharLiteral();
        if (!tok.has_value()) return std::unexpected(tok.error());
        tokens.push_back(std::move(tok.value()));
        continue;
      }

      // Number.
      if (std::isdigit(static_cast<unsigned char>(c)) ||
          (c == '.' && pos_ + 1 < source_.size() &&
           std::isdigit(static_cast<unsigned char>(source_[pos_ + 1])))) {
        tokens.push_back(ReadNumber());
        continue;
      }

      // Hex with $ prefix.
      if (c == '$') {
        tokens.push_back(ReadHexDollar());
        continue;
      }

      // Identifier or keyword.
      if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
        tokens.push_back(ReadIdentifier());
        continue;
      }

      // Operators and punctuation.
      Token tok;
      tok.line = line_;
      tok.column = column_;
      switch (c) {
        case '+': tok.type = TokenType::kPlus; tok.lexeme = "+"; Advance(); break;
        case '-': tok.type = TokenType::kMinus; tok.lexeme = "-"; Advance(); break;
        case '*': tok.type = TokenType::kStar; tok.lexeme = "*"; Advance(); break;
        case '/': tok.type = TokenType::kSlash; tok.lexeme = "/"; Advance(); break;
        case '%': tok.type = TokenType::kPercent; tok.lexeme = "%"; Advance(); break;
        case '(': tok.type = TokenType::kLeftParen; tok.lexeme = "("; Advance(); break;
        case ')': tok.type = TokenType::kRightParen; tok.lexeme = ")"; Advance(); break;
        case '[': tok.type = TokenType::kLeftBracket; tok.lexeme = "["; Advance(); break;
        case ']': tok.type = TokenType::kRightBracket; tok.lexeme = "]"; Advance(); break;
        case ',': tok.type = TokenType::kComma; tok.lexeme = ","; Advance(); break;
        case '=':
          if (Peek(1) == '=') {
            tok.type = TokenType::kEqual; tok.lexeme = "=="; Advance(); Advance();
          } else {
            tok.type = TokenType::kAssign; tok.lexeme = "="; Advance();
          }
          break;
        case '!':
          if (Peek(1) == '=') {
            tok.type = TokenType::kNotEqual; tok.lexeme = "!="; Advance(); Advance();
          } else {
            return std::unexpected(core::Error::ParseError(
                "Unexpected character '!' at line " + std::to_string(line_)));
          }
          break;
        case '<':
          if (Peek(1) == '=') {
            tok.type = TokenType::kLessEqual; tok.lexeme = "<="; Advance(); Advance();
          } else {
            tok.type = TokenType::kLess; tok.lexeme = "<"; Advance();
          }
          break;
        case '>':
          if (Peek(1) == '=') {
            tok.type = TokenType::kGreaterEqual; tok.lexeme = ">="; Advance(); Advance();
          } else {
            tok.type = TokenType::kGreater; tok.lexeme = ">"; Advance();
          }
          break;
        default:
          // Skip unknown characters (BOM bytes, etc.).
          Advance();
          continue;
      }
      tokens.push_back(std::move(tok));
    }

    tokens.push_back(Token{TokenType::kEof, "", line_, column_});
    return tokens;
  }

 private:
  char Peek(size_t offset = 0) const {
    size_t idx = pos_ + offset;
    return (idx < source_.size()) ? source_[idx] : '\0';
  }

  void Advance() {
    if (pos_ < source_.size()) {
      if (source_[pos_] == '\n') {
        ++line_;
        column_ = 1;
      } else {
        ++column_;
      }
      ++pos_;
    }
  }

  void SkipWhitespace() {
    while (pos_ < source_.size()) {
      char c = source_[pos_];
      if (c == ' ' || c == '\t') {
        Advance();
      } else {
        break;
      }
    }
  }

  void SkipLineComment() {
    // Skip "//"
    Advance();
    Advance();
    while (pos_ < source_.size() && source_[pos_] != '\r' &&
           source_[pos_] != '\n') {
      Advance();
    }
  }

  core::Result<Token> ReadString() {
    Token tok;
    tok.type = TokenType::kStringLiteral;
    tok.line = line_;
    tok.column = column_;
    Advance();  // skip opening quote
    std::string value;
    while (pos_ < source_.size() && source_[pos_] != '"') {
      if (source_[pos_] == '\\') {
        Advance();
        if (pos_ >= source_.size()) break;
        char esc = source_[pos_];
        switch (esc) {
          case 'n': value += '\n'; break;
          case 'r': value += '\r'; break;
          case 't': value += '\t'; break;
          case 'b': value += '\b'; break;
          case 'f': value += '\f'; break;
          case '"': value += '"'; break;
          case '\\': value += '\\'; break;
          default: value += esc; break;
        }
        Advance();
      } else if (source_[pos_] == '\r' || source_[pos_] == '\n') {
        // Newlines inside strings are allowed in JASS.
        value += source_[pos_];
        Advance();
      } else {
        value += source_[pos_];
        Advance();
      }
    }
    if (pos_ < source_.size()) {
      Advance();  // skip closing quote
    }
    tok.lexeme = std::move(value);
    return tok;
  }

  core::Result<Token> ReadCharLiteral() {
    Token tok;
    tok.type = TokenType::kIntegerLiteral;
    tok.line = line_;
    tok.column = column_;
    Advance();  // skip opening '
    std::string chars;
    while (pos_ < source_.size() && source_[pos_] != '\'') {
      chars += source_[pos_];
      Advance();
    }
    if (pos_ < source_.size()) {
      Advance();  // skip closing '
    }
    // Convert to integer (big-endian multi-char constant).
    int32_t value = 0;
    if (chars.size() == 1) {
      value = static_cast<unsigned char>(chars[0]);
    } else if (chars.size() == 4) {
      value = (static_cast<uint32_t>(static_cast<unsigned char>(chars[0])) << 24) |
              (static_cast<uint32_t>(static_cast<unsigned char>(chars[1])) << 16) |
              (static_cast<uint32_t>(static_cast<unsigned char>(chars[2])) << 8) |
              static_cast<uint32_t>(static_cast<unsigned char>(chars[3]));
    }
    tok.lexeme = std::to_string(value);
    return tok;
  }

  Token ReadNumber() {
    Token tok;
    tok.line = line_;
    tok.column = column_;
    std::string num;
    bool is_real = false;
    bool is_hex = false;

    if (source_[pos_] == '0' && pos_ + 1 < source_.size() &&
        (source_[pos_ + 1] == 'x' || source_[pos_ + 1] == 'X')) {
      // Hex literal.
      is_hex = true;
      num += source_[pos_]; Advance();
      num += source_[pos_]; Advance();
      while (pos_ < source_.size() &&
             std::isxdigit(static_cast<unsigned char>(source_[pos_]))) {
        num += source_[pos_];
        Advance();
      }
    } else {
      // Decimal or octal or real.
      while (pos_ < source_.size() &&
             std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
        num += source_[pos_];
        Advance();
      }
      if (pos_ < source_.size() && source_[pos_] == '.') {
        is_real = true;
        num += '.';
        Advance();
        while (pos_ < source_.size() &&
               std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
          num += source_[pos_];
          Advance();
        }
      } else if (num.empty() && pos_ < source_.size() &&
                 source_[pos_] == '.') {
        is_real = true;
        num += '.';
        Advance();
        while (pos_ < source_.size() &&
               std::isdigit(static_cast<unsigned char>(source_[pos_]))) {
          num += source_[pos_];
          Advance();
        }
      }
    }

    tok.type = is_real ? TokenType::kRealLiteral : TokenType::kIntegerLiteral;
    tok.lexeme = std::move(num);
    return tok;
  }

  Token ReadHexDollar() {
    Token tok;
    tok.type = TokenType::kIntegerLiteral;
    tok.line = line_;
    tok.column = column_;
    Advance();  // skip '$'
    std::string num = "0x";
    while (pos_ < source_.size() &&
           std::isxdigit(static_cast<unsigned char>(source_[pos_]))) {
      num += source_[pos_];
      Advance();
    }
    tok.lexeme = std::move(num);
    return tok;
  }

  Token ReadIdentifier() {
    Token tok;
    tok.line = line_;
    tok.column = column_;
    std::string id;
    while (pos_ < source_.size() &&
           (std::isalnum(static_cast<unsigned char>(source_[pos_])) ||
            source_[pos_] == '_')) {
      id += source_[pos_];
      Advance();
    }
    auto& kw = Keywords();
    auto it = kw.find(id);
    tok.type = (it != kw.end()) ? it->second : TokenType::kIdentifier;
    tok.lexeme = std::move(id);
    return tok;
  }

  std::string_view source_;
  size_t pos_ = 0;
  int32_t line_ = 1;
  int32_t column_ = 1;
};

}  // namespace

core::Result<std::vector<Token>> Tokenize(std::string_view source) {
  Lexer lexer(source);
  return lexer.Tokenize();
}

}  // namespace w3x_toolkit::parser::jass
