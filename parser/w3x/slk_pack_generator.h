#ifndef W3X_TOOLKIT_PARSER_W3X_SLK_PACK_GENERATOR_H_
#define W3X_TOOLKIT_PARSER_W3X_SLK_PACK_GENERATOR_H_

#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

#include "core/error/error.h"
#include "parser/w3x/map_archive_io.h"
#include "parser/w3x/object_pack_generator.h"

namespace w3x_toolkit::parser::w3x {

struct SlkPackResult {
  std::vector<GeneratedMapFile> generated_files;
  std::unordered_set<std::string> covered_object_files;
};

core::Result<SlkPackResult> GenerateSyntheticSlkFiles(
    const std::filesystem::path& input_dir, const PackOptions& options);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_SLK_PACK_GENERATOR_H_
