#ifndef W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_
#define W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "converter/jass2lua/jass_tokenizer.h"
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
//   - array[idx]               -> table[idx]
//   - Type mapping: integer/real -> number, boolean -> boolean,
//                   string -> string, handle subtypes -> userdata
//   - Operators: and/or/not preserved, != -> ~=
//   - null -> nil
//   - String concatenation: + on strings -> ..
//
class JassToLuaConverter {
 public:
  JassToLuaConverter();
  ~JassToLuaConverter() = default;

  JassToLuaConverter(const JassToLuaConverter&) = delete;
  JassToLuaConverter& operator=(const JassToLuaConverter&) = delete;
  JassToLuaConverter(JassToLuaConverter&&) noexcept = default;
  JassToLuaConverter& operator=(JassToLuaConverter&&) noexcept = default;

  void SetOptions(const JassToLuaOptions& options);
  const JassToLuaOptions& GetOptions() const;

  // Converts a complete JASS source string to its Lua equivalent.
  core::Result<std::string> Convert(std::string_view jass_source);

 private:
  using Token = JassToken;
  using TT = JassTokenType;

  core::Result<std::string> TranslateTokens(const std::vector<Token>& tokens);

  std::string TranslateOperator(std::string_view op);
  std::string TranslateExpression(const std::vector<Token>& tokens,
                                  std::size_t& pos);
  std::string TranslateFunctionBody(const std::vector<Token>& tokens,
                                    std::size_t& pos, int indent_level);
  std::string TranslateGlobalsBlock(const std::vector<Token>& tokens,
                                    std::size_t& pos);
  std::string TranslateCondition(const std::vector<Token>& tokens,
                                 std::size_t& pos);

  static bool IsKw(const std::vector<Token>& t, std::size_t p,
                   std::string_view kw);
  static void SkipWs(const std::vector<Token>& t, std::size_t& p);
  std::string Indent(int level) const;

  JassToLuaOptions options_;
  static const std::unordered_map<std::string, std::string> kTypeMap;
};

}  // namespace w3x_toolkit::converter

#endif  // W3X_TOOLKIT_CONVERTER_JASS_TO_LUA_H_
