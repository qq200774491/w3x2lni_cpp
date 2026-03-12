// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_
#define W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_

#include <filesystem>
#include <string>

#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Resolves a CLI map input path to an existing directory or packed archive.
core::Result<std::filesystem::path> ResolveMapInputPath(
    const std::string& input_path);

// Resolves a CLI map input path to an unpacked directory map.
core::Result<std::filesystem::path> ResolveMapInputDirectory(
    const std::string& input_path);

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_
