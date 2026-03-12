// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/map_input_utils.h"

#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::cli {

core::Result<std::filesystem::path> ResolveMapInputDirectory(
    const std::string& input_path) {
  std::filesystem::path input(input_path);

  if (!core::FilesystemUtils::Exists(input)) {
    return std::unexpected(
        core::Error::FileNotFound("Input path not found: " + input_path));
  }

  if (core::FilesystemUtils::IsDirectory(input)) {
    auto absolute = core::FilesystemUtils::GetAbsolutePath(input);
    if (!absolute.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to resolve map directory: " + absolute.error()));
    }
    return absolute.value();
  }

  if (core::FilesystemUtils::IsFile(input)) {
    return std::unexpected(core::Error::ConvertError(
        "Packed .w3x/.w3m archives are not yet supported in the pure C++ "
        "v1 build. Unpack the map to a directory and retry: " + input_path));
  }

  return std::unexpected(core::Error::InvalidFormat(
      "Input path is neither a file nor a directory: " + input_path));
}

}  // namespace w3x_toolkit::cli
