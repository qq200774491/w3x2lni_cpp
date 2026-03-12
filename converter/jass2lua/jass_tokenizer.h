#ifndef W3X_TOOLKIT_CONVERTER_JASS_TOKENIZER_H_
#define W3X_TOOLKIT_CONVERTER_JASS_TOKENIZER_H_

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::converter
{

  // Token types recognized by the JASS scanner.
  enum class JassTokenType
  {
    kKeyword,
    kIdentifier,
    kStringLiteral,
    kIntegerLiteral,
    kRealLiteral,
    kRawCodeLiteral, // FourCC 'xxxx'
    kOperator,
    kPunctuation,
    kNewline,
    kComment,
    kEndOfFile,
  };

  // A single token produced by the JASS tokenizer.
  struct JassToken
  {
    JassTokenType type;
    std::string text;
    std::size_t line = 0;
  };

  // Tokenizes JASS source code into a flat list of tokens.
  // Returns an error if the source contains malformed constructs that
  // prevent tokenization.
  core::Result<std::vector<JassToken>> TokenizeJass(std::string_view source);

} // namespace w3x_toolkit::converter

#endif // W3X_TOOLKIT_CONVERTER_JASS_TOKENIZER_H_
