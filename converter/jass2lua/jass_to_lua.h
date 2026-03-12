#ifndef W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_
#define W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::converter {

// Options controlling JASS-to-Lua translation behavior.
struct JassToLuaOptions {
  // Prefix to add to global variable names to avoid Lua keyword collisions.
  std::string global_prefix;

  // When true, emit comments marking the original JASS line numbers.
  bool emit_line_comments = false;

  // Indentation string used in the generated Lua code.
  std::string indent = "    ";

  // When true, convert integer division (a / b where both are integers) to
  // Lua's integer division operator (a // b).
  bool use_integer_division = true;
};

// Converts Warcraft III JASS scripts to Lua scripts.
//
// Translation rules implemented:
//   - function/endfunction     -> function ... end
//   - local <type> <name>      -> local <name>
//   - if/then/else/elseif/endif-> if/then/else/elseif/end
//   - loop/endloop             -> while true do ... end
//   - exitwhen <cond>          -> if <cond> then break end
//   - set <var> = <expr>       -> <var> = <expr>
//   - call <func>(...)         -> <func>(...)
//   - globals/endglobals       -> module-level assignments
//   - array[idx]               -> table[idx]  (1-indexed adjustment)
//   - Type mapping: integer/real -> number, boolean -> boolean,
//                   string -> string, handle subtypes -> userdata
//   - Operators: and/or/not preserved, != -> ~=
//   - null -> nil
//   - String concatenation: + on strings -> ..
//
// Usage:
//   JassToLuaConverter converter;
//   converter.SetOptions(options);
//   auto result = converter.Convert(jass_source);
//   if (result.has_value()) {
//     std::string lua_code = result.value();
//   }
//
class JassToLuaConverter {
 public:
  JassToLuaConverter();
  ~JassToLuaConverter() = default;

  // Non-copyable, movable.
  JassToLuaConverter(const JassToLuaConverter&) = delete;
  JassToLuaConverter& operator=(const JassToLuaConverter&) = delete;
  JassToLuaConverter(JassToLuaConverter&&) noexcept = default;
  JassToLuaConverter& operator=(JassToLuaConverter&&) noexcept = default;

  // Sets the conversion options.
  void SetOptions(const JassToLuaOptions& options);

  // Returns the current options.
  const JassToLuaOptions& GetOptions() const;

  // Converts a complete JASS source string to its Lua equivalent.
  // Returns the translated Lua source on success, or an error describing
  // what went wrong (e.g. unexpected syntax).
  core::Result<std::string> Convert(std::string_view jass_source);

 private:
  // Token types recognized by the simple JASS scanner.
  enum class TokenType {
    kKeyword,
    kIdentifier,
    kStringLiteral,
    kIntegerLiteral,
    kRealLiteral,
    kRawCodeLiteral,  // FourCC 'xxxx'
    kOperator,
    kPunctuation,
    kNewline,
    kComment,
    kEndOfFile,
  };

  struct Token {
    TokenType type;
    std::string text;
    std::size_t line = 0;
  };

  // Tokenizes the JASS source into a flat list of tokens.
  core::Result<std::vector<Token>> Tokenize(std::string_view source);

  // Translates the token stream into Lua source.
  core::Result<std::string> TranslateTokens(const std::vector<Token>& tokens);

  // Translation helpers for specific constructs.
  std::string TranslateType(std::string_view jass_type);
  std::string TranslateOperator(std::string_view op);
  std::string TranslateExpression(const std::vector<Token>& tokens,
                                  std::size_t& pos);
  std::string TranslateFunctionBody(const std::vector<Token>& tokens,
                                    std::size_t& pos, int indent_level);
  std::string TranslateGlobalsBlock(const std::vector<Token>& tokens,
                                    std::size_t& pos);

  // Returns true if the token at |pos| is a keyword matching |keyword|.
  static bool IsKeyword(const std::vector<Token>& tokens, std::size_t pos,
                        std::string_view keyword);

  // Skips newline and comment tokens starting at |pos|.
  static void SkipWhitespace(const std::vector<Token>& tokens,
                             std::size_t& pos);

  // Builds the indent string for the given nesting level.
  std::string MakeIndent(int level) const;

  JassToLuaOptions options_;

  // JASS keywords that need special handling.
  static const std::unordered_map<std::string, std::string> kTypeMap;
};

}  // namespace w3x_toolkit::converter

#endif  // W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_
