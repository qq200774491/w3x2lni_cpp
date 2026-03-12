// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/extract_command.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "cli/commands/map_input_utils.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::cli {

std::string ExtractCommand::Name() const { return "extract"; }

std::string ExtractCommand::Description() const {
  return "Extract resources from an unpacked map directory";
}

std::string ExtractCommand::Usage() const {
  return "extract <input_map_dir> <output_dir> "
         "[--type=all|models|textures|sounds|scripts|ui|data|map]";
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

  std::optional<converter::ResourceType> resource_type;
  for (std::size_t i = 2; i < args.size(); ++i) {
    const std::string& arg = args[i];
    if (arg.starts_with("--type=")) {
      auto result = ParseResourceType(arg.substr(7));
      if (!result.has_value()) {
        return std::unexpected(result.error());
      }
      resource_type = result.value();
      continue;
    }

    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Unknown option: " + arg + "\nUsage: " + Usage()));
  }

  W3X_ASSIGN_OR_RETURN(auto input_dir, ResolveMapInputDirectory(args[0]));
  return RunExtraction(input_dir, std::filesystem::path(args[1]),
                       resource_type);
}

core::Result<std::optional<converter::ResourceType>>
ExtractCommand::ParseResourceType(const std::string& value) {
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower == "all") return std::nullopt;
  if (lower == "models") return converter::ResourceType::kModel;
  if (lower == "textures") return converter::ResourceType::kTexture;
  if (lower == "sounds") return converter::ResourceType::kSound;
  if (lower == "scripts") return converter::ResourceType::kScript;
  if (lower == "ui") return converter::ResourceType::kUi;
  if (lower == "data") return converter::ResourceType::kData;
  if (lower == "map") return converter::ResourceType::kMap;

  return std::unexpected(core::Error(
      core::ErrorCode::kInvalidFormat,
      "Unknown resource type '" + value +
          "'. Supported types: all, models, textures, sounds, scripts, ui, "
          "data, map"));
}

core::Result<void> ExtractCommand::RunExtraction(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const std::optional<converter::ResourceType>& type) {
  auto& logger = core::Logger::Instance();

  auto create_result = core::FilesystemUtils::CreateDirectories(output_path);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + create_result.error()));
  }

  const std::string type_name =
      type.has_value() ? std::string(converter::ResourceTypeToString(*type))
                       : std::string("All");
  logger.Info("Extracting '{}' -> '{}' (type: {})", input_path.string(),
              output_path.string(), type_name);

  converter::ResourceExtractor extractor(input_path);
  extractor.SetOutputPath(output_path);
  extractor.SetProgressCallback(
      [](std::size_t current, std::size_t total, std::string_view file) {
        std::cout << "[" << current << "/" << total << "] " << file
                  << std::endl;
        return true;
      });

  core::Result<std::size_t> extracted = type.has_value()
      ? extractor.ExtractByType(*type)
      : extractor.ExtractAll();
  if (!extracted.has_value()) {
    return std::unexpected(extracted.error());
  }

  logger.Info("Extraction complete.");
  std::cout << "Extracted " << extracted.value() << " file(s) to: "
            << output_path.string() << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
