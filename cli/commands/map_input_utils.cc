// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/map_input_utils.h"

#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::cli {

core::Result<std::filesystem::path> ResolveMapInputPath(
    const std::string& input_path) {
  std::filesystem::path input(input_path);

  if (!core::FilesystemUtils::Exists(input)) {
    return std::unexpected(
        core::Error::FileNotFound("Input path not found: " + input_path));
  }

  if (!core::FilesystemUtils::IsDirectory(input) &&
      !core::FilesystemUtils::IsFile(input)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input path is neither a file nor a directory: " + input_path));
  }

  auto absolute = core::FilesystemUtils::GetAbsolutePath(input);
  if (!absolute.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to resolve input path: " + absolute.error()));
  }
  return absolute.value();
}

core::Result<std::filesystem::path> ResolveMapInputDirectory(
    const std::string& input_path) {
  W3X_ASSIGN_OR_RETURN(auto resolved, ResolveMapInputPath(input_path));
  if (!core::FilesystemUtils::IsDirectory(resolved)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input must be an unpacked map directory: " + resolved.string()));
  }
  return resolved;
}

}  // namespace w3x_toolkit::cli
