#ifndef W3X_TOOLKIT_PARSER_W3U_W3U_WRITER_H_
#define W3X_TOOLKIT_PARSER_W3U_W3U_WRITER_H_

#include <vector>

#include "core/error/error.h"
#include "parser/w3u/w3u_parser.h"

namespace w3x_toolkit::parser::w3u {

// Serializes a Warcraft III object data file (W3A/W3U/W3T/W3B/W3D/W3H/W3Q).
core::Result<std::vector<std::uint8_t>> SerializeObjectFile(
    const ObjectData& data, ObjectFileKind kind);

}  // namespace w3x_toolkit::parser::w3u

#endif  // W3X_TOOLKIT_PARSER_W3U_W3U_WRITER_H_
