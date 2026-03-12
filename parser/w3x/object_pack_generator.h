#ifndef W3X_TOOLKIT_PARSER_W3X_OBJECT_PACK_GENERATOR_H_
#define W3X_TOOLKIT_PARSER_W3X_OBJECT_PACK_GENERATOR_H_

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

struct GeneratedMapFile {
  std::string relative_path;
  std::vector<std::uint8_t> content;
};

// Generates missing obj-side files (war3map.w3a/w3u/w3t/w3q/w3h and
// war3mapmisc.txt) from the unpacked map directory using the same external
// metadata that the original Lua implementation relies on.
core::Result<std::vector<GeneratedMapFile>> GenerateSyntheticMapFiles(
    const std::filesystem::path& input_dir);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_OBJECT_PACK_GENERATOR_H_
