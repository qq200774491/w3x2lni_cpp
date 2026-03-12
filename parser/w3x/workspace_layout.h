#ifndef W3X_TOOLKIT_PARSER_W3X_WORKSPACE_LAYOUT_H_
#define W3X_TOOLKIT_PARSER_W3X_WORKSPACE_LAYOUT_H_

#include <filesystem>
#include <string>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

// Returns true when the directory looks like an external-style workspace.
bool IsWorkspaceLayout(const std::filesystem::path& input_dir);

// Converts a flat unpacked map directory into an external-style workspace.
core::Result<void> ConvertFlatDirectoryToWorkspace(
    const std::filesystem::path& flat_dir,
    const std::filesystem::path& workspace_dir);

// Converts an external-style workspace back into a flat map directory.
core::Result<void> ConvertWorkspaceToFlatDirectory(
    const std::filesystem::path& workspace_dir,
    const std::filesystem::path& flat_dir);

// Maps a flat unpacked relative path to workspace layout.
std::string ToWorkspacePath(const std::string& flat_relative_path);

// Maps a workspace relative path to flat unpacked layout.
std::string ToFlatPath(const std::string& workspace_relative_path);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_WORKSPACE_LAYOUT_H_
