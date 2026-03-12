// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/pack_command.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"
#include "parser/w3x/map_archive_io.h"

namespace w3x_toolkit::cli {

std::string PackCommand::Name() const { return "pack"; }

std::string PackCommand::Description() const {
  return "Pack an unpacked map directory back into a .w3x/.w3m archive";
}

std::string PackCommand::Usage() const {
  return "pack <input_dir> <output_map.w3x|output_map.w3m>";
}

core::Result<void> PackCommand::Execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing required arguments.\nUsage: " + Usage()));
  }
  if (args.size() < 2) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing output archive path.\nUsage: " + Usage()));
  }
  if (args.size() > 2) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Unknown extra arguments.\nUsage: " + Usage()));
  }

  const std::filesystem::path input_dir(args[0]);
  if (!core::FilesystemUtils::Exists(input_dir)) {
    return std::unexpected(
        core::Error::FileNotFound("Input directory not found: " + args[0]));
  }
  if (!core::FilesystemUtils::IsDirectory(input_dir)) {
    return std::unexpected(
        core::Error::InvalidFormat("Input must be a directory: " + args[0]));
  }

  auto& logger = core::Logger::Instance();
  logger.Info("Packing '{}' -> '{}'", input_dir.string(), args[1]);

  W3X_ASSIGN_OR_RETURN(auto packed_files,
                       parser::w3x::PackMapDirectory(
                           input_dir, std::filesystem::path(args[1])));
  std::cout << "Packed " << packed_files << " file(s) into: " << args[1]
            << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
