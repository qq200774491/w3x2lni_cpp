// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_
#define W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_

#include <filesystem>
#include <string>

#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Resolves a CLI map input path to an unpacked directory map.
//
// The current pure C++ v1 build supports unpacked map directories only.
// Packed .w3x/.w3m MPQ archives are intentionally rejected with a clear
// actionable error until the MPQ reader is implemented.
core::Result<std::filesystem::path> ResolveMapInputDirectory(
    const std::string& input_path);

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_MAP_INPUT_UTILS_H_
