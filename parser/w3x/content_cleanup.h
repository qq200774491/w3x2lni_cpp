#ifndef W3X_TOOLKIT_PARSER_W3X_CONTENT_CLEANUP_H_
#define W3X_TOOLKIT_PARSER_W3X_CONTENT_CLEANUP_H_

#include <filesystem>

#include "core/error/error.h"
#include "parser/w3x/map_archive_io.h"

namespace w3x_toolkit::parser::w3x {

core::Result<void> ApplyContentCleanupPasses(
    const std::filesystem::path& input_dir, const PackOptions& options);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_CONTENT_CLEANUP_H_
