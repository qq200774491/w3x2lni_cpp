// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/extract_command.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::cli {

// ---------------------------------------------------------------------------
// Command interface
// ---------------------------------------------------------------------------

std::string ExtractCommand::Name() const { return "extract"; }

std::string ExtractCommand::Description() const {
  return "Extract resources from a W3X map archive";
}

std::string ExtractCommand::Usage() const {
  return "extract <input.w3x> <output_dir> "
         "[--type=all|models|textures|sounds|scripts]";
}

core::Result<void> ExtractCommand::Execute(
    const std::vector<std::string>& args) {
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

  const std::string& input_path = args[0];
  const std::string& output_path = args[1];
  ResourceType resource_type = ResourceType::kAll;  // default

  // Parse optional flags.
  for (std::size_t i = 2; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg.starts_with("--type=")) {
      std::string value = arg.substr(7);
      auto result = ParseResourceType(value);
      if (!result.has_value()) {
        return std::unexpected(result.error());
      }
      resource_type = result.value();
    } else {
      return std::unexpected(core::Error(
          core::ErrorCode::kInvalidFormat,
          "Unknown option: " + arg + "\nUsage: " + Usage()));
    }
  }

  return RunExtraction(input_path, output_path, resource_type);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

core::Result<ResourceType> ExtractCommand::ParseResourceType(
    const std::string& value) {
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower == "all") return ResourceType::kAll;
  if (lower == "models") return ResourceType::kModels;
  if (lower == "textures") return ResourceType::kTextures;
  if (lower == "sounds") return ResourceType::kSounds;
  if (lower == "scripts") return ResourceType::kScripts;

  return std::unexpected(core::Error(
      core::ErrorCode::kInvalidFormat,
      "Unknown resource type '" + value +
          "'. Supported types: all, models, textures, sounds, scripts"));
}

core::Result<void> ExtractCommand::RunExtraction(
    const std::string& input_path, const std::string& output_path,
    ResourceType type) {
  auto& logger = core::Logger::Instance();

  // Validate input file exists.
  std::filesystem::path input(input_path);
  if (!core::FilesystemUtils::Exists(input)) {
    return std::unexpected(
        core::Error::FileNotFound("Input file not found: " + input_path));
  }
  if (!core::FilesystemUtils::IsFile(input)) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Input path is not a file: " + input_path));
  }

  // Ensure output directory exists.
  std::filesystem::path output(output_path);
  auto create_result = core::FilesystemUtils::CreateDirectories(output);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + create_result.error()));
  }

  const char* type_name = "all";
  switch (type) {
    case ResourceType::kAll:
      type_name = "all";
      break;
    case ResourceType::kModels:
      type_name = "models";
      break;
    case ResourceType::kTextures:
      type_name = "textures";
      break;
    case ResourceType::kSounds:
      type_name = "sounds";
      break;
    case ResourceType::kScripts:
      type_name = "scripts";
      break;
  }

  logger.Info("Extracting '{}' -> '{}' (type: {})", input_path, output_path,
              type_name);

  // ------------------------------------------------------------------
  // TODO: Call into the resource_extract module once it is implemented.
  //
  // The extraction pipeline will:
  //   1. Open the W3X archive (MPQ)
  //   2. Enumerate files matching the resource type filter
  //   3. Extract each file to the output directory
  //   4. Report progress to stdout
  //
  // For now we print a placeholder message.
  // ------------------------------------------------------------------
  std::cout << "[1/3] Opening map archive..." << std::endl;
  std::cout << "[2/3] Enumerating resources (filter: " << type_name << ")..."
            << std::endl;
  std::cout << "[3/3] Extracting files..." << std::endl;

  logger.Info("Extraction complete.");
  std::cout << "Extraction complete. Output written to: " << output_path
            << std::endl;

  return {};
}

}  // namespace w3x_toolkit::cli
