// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/convert_command.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "cli/commands/map_input_utils.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::cli {

// ---------------------------------------------------------------------------
// Command interface
// ---------------------------------------------------------------------------

std::string ConvertCommand::Name() const { return "convert"; }

std::string ConvertCommand::Description() const {
  return "Convert an unpacked map directory to the LNI workspace layout";
}

std::string ConvertCommand::Usage() const {
  return "convert <input_map_dir> <output_dir> "
         "[--no-map-files] [--no-table-data] [--no-triggers] [--no-config]";
}

core::Result<void> ConvertCommand::Execute(
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

  auto options = ParseOptions(args, 2);
  if (!options.has_value()) {
    return std::unexpected(options.error());
  }

  W3X_ASSIGN_OR_RETURN(auto input_dir, ResolveMapInputDirectory(args[0]));
  return RunConversion(input_dir, std::filesystem::path(args[1]),
                       options.value());
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

core::Result<converter::W3xToLniOptions> ConvertCommand::ParseOptions(
    const std::vector<std::string>& args, std::size_t first_option_index) {
  converter::W3xToLniOptions options;

  for (std::size_t i = first_option_index; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg == "--no-map-files") {
      options.extract_map_files = false;
    } else if (arg == "--no-table-data") {
      options.extract_table_data = false;
    } else if (arg == "--no-triggers") {
      options.extract_triggers = false;
    } else if (arg == "--no-config") {
      options.generate_config = false;
    } else {
      return std::unexpected(core::Error(
          core::ErrorCode::kInvalidFormat,
          "Unknown option: " + arg + "\nUsage: " + Usage()));
    }
  }

  if (!options.extract_map_files && !options.extract_table_data &&
      !options.extract_triggers && !options.generate_config) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "All conversion outputs are disabled. Enable at least one output "
        "group or remove the --no-* flags."));
  }

  return options;
}

core::Result<void> ConvertCommand::RunConversion(
    const std::filesystem::path& input_path,
    const std::filesystem::path& output_path,
    const converter::W3xToLniOptions& options) {
  auto& logger = core::Logger::Instance();

  // Ensure output directory exists (create if needed).
  auto create_result = core::FilesystemUtils::CreateDirectories(output_path);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + create_result.error()));
  }

  logger.Info("Converting '{}' -> '{}' as LNI workspace", input_path.string(),
              output_path.string());

  converter::W3xToLniConverter converter(input_path, output_path);
  converter.SetOptions(options);
  converter.SetProgressCallback([](const converter::ConversionProgress& p) {
    std::cout << "[" << p.items_done << "/" << p.items_total << "] "
              << p.phase;
    std::cout << " (" << std::fixed << std::setprecision(0)
              << p.overall_fraction * 100.0 << "%)" << std::endl;
    return true;
  });

  auto result = converter.Convert();
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }

  logger.Info("Conversion complete.");
  std::cout << "LNI workspace written to: " << output_path.string()
            << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
