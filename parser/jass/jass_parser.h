#ifndef W3X_TOOLKIT_PARSER_JASS_JASS_PARSER_H_
#define W3X_TOOLKIT_PARSER_JASS_JASS_PARSER_H_

#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"
#include "parser/jass/jass_types.h"

namespace w3x_toolkit::parser::jass {

// Tokenizes JASS source code into a sequence of tokens.
core::Result<std::vector<Token>> Tokenize(std::string_view source);

// Parses JASS source code into an abstract syntax tree.
// This is a recursive descent parser that handles the full JASS language.
core::Result<JassAst> ParseJass(std::string_view source);

}  // namespace w3x_toolkit::parser::jass

#endif  // W3X_TOOLKIT_PARSER_JASS_JASS_PARSER_H_
