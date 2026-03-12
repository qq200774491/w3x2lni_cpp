#include "converter/jass2lua/jass_to_lua.h"

#include <cstdint>
#include <sstream>
#include <string>
#include <utility>

#include "core/logger/logger.h"

namespace w3x_toolkit::converter
{

  // ---------------------------------------------------------------------------
  // Static data
  // ---------------------------------------------------------------------------

  const std::unordered_map<std::string, std::string> JassToLuaConverter::kTypeMap{
      {"integer", "number"},
      {"real", "number"},
      {"boolean", "boolean"},
      {"string", "string"},
      {"handle", "userdata"},
      {"code", "function"},
      {"nothing", "nil"},
  };

  JassToLuaConverter::JassToLuaConverter() = default;

  void JassToLuaConverter::SetOptions(const JassToLuaOptions &options)
  {
    options_ = options;
  }

  const JassToLuaOptions &JassToLuaConverter::GetOptions() const
  {
    return options_;
  }

  // ---------------------------------------------------------------------------
  // Helpers
  // ---------------------------------------------------------------------------

  std::string JassToLuaConverter::Indent(int level) const
  {
    std::string r;
    for (int i = 0; i < level; ++i)
      r += options_.indent;
    return r;
  }

  bool JassToLuaConverter::IsKw(const std::vector<Token> &t, std::size_t p,
                                std::string_view kw)
  {
    return p < t.size() && t[p].type == TT::kKeyword && t[p].text == kw;
  }

  void JassToLuaConverter::SkipWs(const std::vector<Token> &t,
                                  std::size_t &p)
  {
    while (p < t.size() &&
           (t[p].type == TT::kNewline || t[p].type == TT::kComment))
      ++p;
  }

  std::string JassToLuaConverter::TranslateOperator(std::string_view op)
  {
    if (op == "!=")
      return "~=";
    return std::string(op);
  }

  // ---------------------------------------------------------------------------
  // Expression
  // ---------------------------------------------------------------------------

  std::string JassToLuaConverter::TranslateExpression(
      const std::vector<Token> &tokens, std::size_t &pos)
  {
    std::ostringstream out;
    while (pos < tokens.size())
    {
      const auto &tok = tokens[pos];
      if (tok.type == TT::kNewline || tok.type == TT::kEndOfFile ||
          tok.type == TT::kComment)
        break;

      if (tok.type == TT::kKeyword && tok.text == "null")
      {
        out << "nil";
        ++pos;
        continue;
      }
      if (tok.type == TT::kKeyword && tok.text == "not")
      {
        out << "not ";
        ++pos;
        continue;
      }
      if (tok.type == TT::kKeyword &&
          (tok.text == "and" || tok.text == "or"))
      {
        out << " " << tok.text << " ";
        ++pos;
        continue;
      }
      if (tok.type == TT::kKeyword &&
          (tok.text == "true" || tok.text == "false"))
      {
        out << tok.text;
        ++pos;
        continue;
      }
      if (tok.type == TT::kOperator && tok.text == "!=")
      {
        out << " ~= ";
        ++pos;
        continue;
      }
      if (tok.type == TT::kOperator && tok.text == "+")
      {
        bool next_str = pos + 1 < tokens.size() &&
                        tokens[pos + 1].type == TT::kStringLiteral;
        std::string s = out.str();
        bool prev_str = !s.empty() && s.back() == '"';
        out << (next_str || prev_str ? " .. " : " + ");
        ++pos;
        continue;
      }
      if (tok.type == TT::kOperator)
      {
        out << " " << TranslateOperator(tok.text) << " ";
        ++pos;
        continue;
      }
      if (tok.type == TT::kRawCodeLiteral)
      {
        std::string raw = tok.text;
        if (raw.size() >= 2 && raw.front() == '\'' && raw.back() == '\'')
        {
          std::string chars = raw.substr(1, raw.size() - 2);
          uint32_t val = 0;
          for (char c : chars)
            val = (val << 8) | static_cast<uint8_t>(c);
          out << val;
        }
        else
        {
          out << raw;
        }
        ++pos;
        continue;
      }
      out << tok.text;
      ++pos;
    }
    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Condition (shared by if / elseif)
  // ---------------------------------------------------------------------------

  std::string JassToLuaConverter::TranslateCondition(
      const std::vector<Token> &tokens, std::size_t &pos)
  {
    std::ostringstream cond;
    while (pos < tokens.size() && !IsKw(tokens, pos, "then"))
    {
      if (tokens[pos].type == TT::kNewline)
      {
        ++pos;
        continue;
      }
      if (tokens[pos].type == TT::kKeyword && tokens[pos].text == "null")
        cond << "nil";
      else if (tokens[pos].type == TT::kOperator && tokens[pos].text == "!=")
        cond << " ~= ";
      else if (tokens[pos].type == TT::kOperator)
        cond << " " << TranslateOperator(tokens[pos].text) << " ";
      else if (tokens[pos].type == TT::kKeyword &&
               (tokens[pos].text == "and" || tokens[pos].text == "or"))
        cond << " " << tokens[pos].text << " ";
      else if (tokens[pos].type == TT::kKeyword && tokens[pos].text == "not")
        cond << "not ";
      else
        cond << tokens[pos].text;
      ++pos;
    }
    if (IsKw(tokens, pos, "then"))
      ++pos;
    return cond.str();
  }

  // ---------------------------------------------------------------------------
  // Globals block
  // ---------------------------------------------------------------------------

  std::string JassToLuaConverter::TranslateGlobalsBlock(
      const std::vector<Token> &tokens, std::size_t &pos)
  {
    ++pos; // skip "globals"
    SkipWs(tokens, pos);
    std::ostringstream out;
    out << "-- globals\n";

    while (pos < tokens.size() && !IsKw(tokens, pos, "endglobals"))
    {
      SkipWs(tokens, pos);
      if (pos >= tokens.size() || IsKw(tokens, pos, "endglobals"))
        break;
      if (tokens[pos].type == TT::kComment)
      {
        out << tokens[pos].text << "\n";
        ++pos;
        continue;
      }

      if (IsKw(tokens, pos, "constant"))
        ++pos;
      if (pos < tokens.size() &&
          (tokens[pos].type == TT::kKeyword ||
           tokens[pos].type == TT::kIdentifier))
        ++pos; // skip type

      bool is_array = IsKw(tokens, pos, "array");
      if (is_array)
        ++pos;
      if (pos >= tokens.size())
        break;

      std::string name = options_.global_prefix + tokens[pos].text;
      ++pos;

      if (pos < tokens.size() && tokens[pos].type == TT::kOperator &&
          tokens[pos].text == "=")
      {
        ++pos;
        std::string expr = TranslateExpression(tokens, pos);
        out << name << " = " << (is_array ? "{}" : expr) << "\n";
      }
      else
      {
        out << name << " = " << (is_array ? "{}" : "nil") << "\n";
      }
      SkipWs(tokens, pos);
    }
    if (IsKw(tokens, pos, "endglobals"))
      ++pos;
    out << "-- end globals\n";
    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Function body
  // ---------------------------------------------------------------------------

  std::string JassToLuaConverter::TranslateFunctionBody(
      const std::vector<Token> &tokens, std::size_t &pos, int indent_level)
  {
    std::ostringstream out;
    std::string ind = Indent(indent_level);

    while (pos < tokens.size())
    {
      SkipWs(tokens, pos);
      if (pos >= tokens.size() || tokens[pos].type == TT::kEndOfFile)
        break;
      if (IsKw(tokens, pos, "endfunction") || IsKw(tokens, pos, "endif") ||
          IsKw(tokens, pos, "endloop") || IsKw(tokens, pos, "else") ||
          IsKw(tokens, pos, "elseif"))
        break;

      if (tokens[pos].type == TT::kComment)
      {
        out << ind << tokens[pos].text << "\n";
        ++pos;
        continue;
      }

      // local
      if (IsKw(tokens, pos, "local"))
      {
        ++pos;
        if (pos < tokens.size())
          ++pos; // skip type
        bool arr = IsKw(tokens, pos, "array");
        if (arr)
          ++pos;
        if (pos >= tokens.size())
          break;
        std::string vn = tokens[pos].text;
        ++pos;
        if (pos < tokens.size() && tokens[pos].type == TT::kOperator &&
            tokens[pos].text == "=")
        {
          ++pos;
          std::string expr = TranslateExpression(tokens, pos);
          out << ind << "local " << vn << " = " << (arr ? "{}" : expr) << "\n";
        }
        else
        {
          out << ind << "local " << vn << (arr ? " = {}" : "") << "\n";
        }
        continue;
      }

      // set
      if (IsKw(tokens, pos, "set"))
      {
        ++pos;
        std::string lhs;
        while (pos < tokens.size() &&
               !(tokens[pos].type == TT::kOperator && tokens[pos].text == "="))
          lhs += tokens[pos++].text;
        if (pos < tokens.size())
          ++pos;
        out << ind << lhs << " = " << TranslateExpression(tokens, pos) << "\n";
        continue;
      }

      // call
      if (IsKw(tokens, pos, "call"))
      {
        ++pos;
        out << ind << TranslateExpression(tokens, pos) << "\n";
        continue;
      }

      // return
      if (IsKw(tokens, pos, "return"))
      {
        ++pos;
        std::string expr = TranslateExpression(tokens, pos);
        out << ind << "return" << (expr.empty() ? "" : " " + expr) << "\n";
        continue;
      }

      // if
      if (IsKw(tokens, pos, "if"))
      {
        ++pos;
        out << ind << "if " << TranslateCondition(tokens, pos) << " then\n";
        out << TranslateFunctionBody(tokens, pos, indent_level + 1);
        while (IsKw(tokens, pos, "elseif"))
        {
          ++pos;
          out << ind << "elseif " << TranslateCondition(tokens, pos)
              << " then\n";
          out << TranslateFunctionBody(tokens, pos, indent_level + 1);
        }
        if (IsKw(tokens, pos, "else"))
        {
          ++pos;
          out << ind << "else\n";
          out << TranslateFunctionBody(tokens, pos, indent_level + 1);
        }
        if (IsKw(tokens, pos, "endif"))
          ++pos;
        out << ind << "end\n";
        continue;
      }

      // loop
      if (IsKw(tokens, pos, "loop"))
      {
        ++pos;
        out << ind << "while true do\n";
        out << TranslateFunctionBody(tokens, pos, indent_level + 1);
        if (IsKw(tokens, pos, "endloop"))
          ++pos;
        out << ind << "end\n";
        continue;
      }

      // exitwhen
      if (IsKw(tokens, pos, "exitwhen"))
      {
        ++pos;
        out << ind << "if " << TranslateExpression(tokens, pos)
            << " then break end\n";
        continue;
      }

      // native / type / constant -- skip to end of line
      if (IsKw(tokens, pos, "native") || IsKw(tokens, pos, "type") ||
          IsKw(tokens, pos, "constant"))
      {
        while (pos < tokens.size() && tokens[pos].type != TT::kNewline &&
               tokens[pos].type != TT::kEndOfFile)
          ++pos;
        continue;
      }

      // fallback
      out << ind << tokens[pos].text;
      ++pos;
      std::string rest = TranslateExpression(tokens, pos);
      if (!rest.empty())
        out << " " << rest;
      out << "\n";
    }
    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Top-level translation
  // ---------------------------------------------------------------------------

  core::Result<std::string> JassToLuaConverter::TranslateTokens(
      const std::vector<Token> &tokens)
  {
    std::ostringstream out;
    out << "-- Converted from JASS to Lua by w3x_toolkit\n\n";
    std::size_t pos = 0;

    while (pos < tokens.size())
    {
      SkipWs(tokens, pos);
      if (pos >= tokens.size() || tokens[pos].type == TT::kEndOfFile)
        break;

      if (tokens[pos].type == TT::kComment)
      {
        out << tokens[pos].text << "\n";
        ++pos;
        continue;
      }

      if (IsKw(tokens, pos, "globals"))
      {
        out << TranslateGlobalsBlock(tokens, pos) << "\n";
        continue;
      }

      if (IsKw(tokens, pos, "function"))
      {
        ++pos;
        std::string name = pos < tokens.size() ? tokens[pos++].text : "";
        std::vector<std::string> params;
        if (IsKw(tokens, pos, "takes"))
        {
          ++pos;
          if (IsKw(tokens, pos, "nothing"))
          {
            ++pos;
          }
          else
          {
            while (pos < tokens.size() && !IsKw(tokens, pos, "returns"))
            {
              if (tokens[pos].type == TT::kNewline)
              {
                ++pos;
                continue;
              }
              if (tokens[pos].type == TT::kKeyword ||
                  tokens[pos].type == TT::kIdentifier)
                ++pos; // skip type
              if (pos < tokens.size() && tokens[pos].type == TT::kIdentifier)
                params.push_back(tokens[pos++].text);
              if (pos < tokens.size() && tokens[pos].type == TT::kPunctuation &&
                  tokens[pos].text == ",")
                ++pos;
            }
          }
        }
        if (IsKw(tokens, pos, "returns"))
        {
          ++pos;
          if (pos < tokens.size())
            ++pos;
        }

        out << "function " << name << "(";
        for (std::size_t p = 0; p < params.size(); ++p)
        {
          if (p > 0)
            out << ", ";
          out << params[p];
        }
        out << ")\n";
        out << TranslateFunctionBody(tokens, pos, 1);
        if (IsKw(tokens, pos, "endfunction"))
          ++pos;
        out << "end\n\n";
        continue;
      }

      // Skip native / constant / type declarations.
      if (IsKw(tokens, pos, "native") || IsKw(tokens, pos, "constant") ||
          IsKw(tokens, pos, "type"))
      {
        while (pos < tokens.size() && tokens[pos].type != TT::kNewline &&
               tokens[pos].type != TT::kEndOfFile)
          ++pos;
        continue;
      }

      ++pos; // skip unrecognized
    }
    return out.str();
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  core::Result<std::string> JassToLuaConverter::Convert(
      std::string_view jass_source)
  {
    if (jass_source.empty())
    {
      return std::string("-- Converted from JASS to Lua by w3x_toolkit\n\n");
    }

    W3X_ASSIGN_OR_RETURN(auto tokens, TokenizeJass(jass_source));

    core::Logger::Instance().Debug("JassToLua: tokenized {} tokens",
                                   tokens.size());
    return TranslateTokens(tokens);
  }

} // namespace w3x_toolkit::converter
