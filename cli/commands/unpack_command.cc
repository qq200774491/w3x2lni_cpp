// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/unpack_command.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"
#include "parser/w3x/map_archive_io.h"
#include "parser/w3x/workspace_layout.h"

namespace w3x_toolkit::cli {

namespace {

std::filesystem::path MakeTemporaryDirectoryPath() {
  const auto ticks =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  return std::filesystem::temp_directory_path() /
         ("w3x_toolkit_unpack_" + ticks);
}

}  // namespace

std::string UnpackCommand::Name() const { return "unpack"; }

std::string UnpackCommand::Description() const {
  return "Unpack a .w3x/.w3m archive to an external-style workspace";
}

std::string UnpackCommand::Usage() const {
  return "unpack <input_map.w3x|input_map.w3m> <output_dir>";
}

core::Result<void> UnpackCommand::Execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing required arguments.\nUsage: " + Usage()));
  }
  if (args.size() < 2) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing output directory.\nUsage: " + Usage()));
  }
  if (args.size() > 2) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Unknown extra arguments.\nUsage: " + Usage()));
  }

  const std::filesystem::path input_path(args[0]);
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::unexpected(
        core::Error::FileNotFound("Input archive not found: " + args[0]));
  }
  if (!core::FilesystemUtils::IsFile(input_path)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input must be a packed .w3x/.w3m file: " + args[0]));
  }

  auto& logger = core::Logger::Instance();
  logger.Info("Unpacking '{}' -> '{}'", input_path.string(), args[1]);

  const std::filesystem::path workspace_output(args[1]);
  if (core::FilesystemUtils::Exists(workspace_output)) {
    auto remove_result = core::FilesystemUtils::RemoveAll(workspace_output);
    if (!remove_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to clear output directory: " + workspace_output.string() +
          ": " + remove_result.error()));
    }
  }

  const std::filesystem::path raw_output = MakeTemporaryDirectoryPath();
  auto create_result = core::FilesystemUtils::CreateDirectories(raw_output);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create temporary unpack directory: " + raw_output.string() +
        ": " + create_result.error()));
  }

  W3X_ASSIGN_OR_RETURN(
      auto manifest,
      parser::w3x::UnpackMapArchive(input_path, raw_output));
  auto convert_result = parser::w3x::ConvertFlatDirectoryToWorkspace(
      raw_output, workspace_output);
  std::error_code cleanup_error;
  std::filesystem::remove_all(raw_output, cleanup_error);
  if (!convert_result.has_value()) {
    return std::unexpected(std::move(convert_result.error()));
  }

  std::cout << "Unpacked " << manifest.written_files.size() << " file(s) to: "
            << args[1] << std::endl;
  std::cout << "Recovered names: "
            << std::count_if(
                   manifest.written_files.begin(), manifest.written_files.end(),
                   [](const parser::w3x::ManifestEntry& entry) {
                     return entry.status ==
                            parser::w3x::ArchiveNameStatus::kRecovered;
                   })
            << std::endl;
  std::cout << "Anonymous leftovers: "
            << std::count_if(
                   manifest.written_files.begin(), manifest.written_files.end(),
                   [](const parser::w3x::ManifestEntry& entry) {
                     return entry.status ==
                            parser::w3x::ArchiveNameStatus::kAnonymous;
                   })
            << std::endl;
  std::cout << "Skipped duplicate aliases: "
            << manifest.skipped_duplicates.size() << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
