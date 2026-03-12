#ifndef W3X_TOOLKIT_PARSER_W3X_TXT_PACK_GENERATOR_H_
#define W3X_TOOLKIT_PARSER_W3X_TXT_PACK_GENERATOR_H_

#include <filesystem>
#include <vector>

#include "core/error/error.h"
#include "parser/w3x/map_archive_io.h"
#include "parser/w3x/object_pack_generator.h"

namespace w3x_toolkit::parser::w3x {

struct TxtPackResult {
  std::vector<GeneratedMapFile> generated_files;
};

core::Result<TxtPackResult> GenerateSyntheticTxtFiles(
    const std::filesystem::path& input_dir, const PackOptions& options);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_TXT_PACK_GENERATOR_H_
