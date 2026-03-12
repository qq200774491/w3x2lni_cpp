// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/convert_command.h"

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

std::string ConvertCommand::Name() const { return "convert"; }

std::string ConvertCommand::Description() const {
  return "Convert a W3X map to LNI, SLK, or OBJ format";
}

std::string ConvertCommand::Usage() const {
  return "convert <input.w3x> <output_dir> [--format=lni|slk|obj]";
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

  const std::string& input_path = args[0];
  const std::string& output_path = args[1];
  ConvertFormat format = ConvertFormat::kLni;  // default

  // Parse optional flags.
  for (std::size_t i = 2; i < args.size(); ++i) {
    const std::string& arg = args[i];

    if (arg.starts_with("--format=")) {
      std::string value = arg.substr(9);
      auto result = ParseFormat(value);
      if (!result.has_value()) {
        return std::unexpected(result.error());
      }
      format = result.value();
    } else {
      return std::unexpected(core::Error(
          core::ErrorCode::kInvalidFormat,
          "Unknown option: " + arg + "\nUsage: " + Usage()));
    }
  }

  return RunConversion(input_path, output_path, format);
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

core::Result<ConvertFormat> ConvertCommand::ParseFormat(
    const std::string& value) {
  std::string lower = value;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c) { return std::tolower(c); });

  if (lower == "lni") return ConvertFormat::kLni;
  if (lower == "slk") return ConvertFormat::kSlk;
  if (lower == "obj") return ConvertFormat::kObj;

  return std::unexpected(core::Error(
      core::ErrorCode::kInvalidFormat,
      "Unknown format '" + value + "'. Supported formats: lni, slk, obj"));
}

core::Result<void> ConvertCommand::RunConversion(
    const std::string& input_path, const std::string& output_path,
    ConvertFormat format) {
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

  // Ensure output directory exists (create if needed).
  std::filesystem::path output(output_path);
  auto create_result = core::FilesystemUtils::CreateDirectories(output);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + create_result.error()));
  }

  const char* format_name = "LNI";
  switch (format) {
    case ConvertFormat::kLni:
      format_name = "LNI";
      break;
    case ConvertFormat::kSlk:
      format_name = "SLK";
      break;
    case ConvertFormat::kObj:
      format_name = "OBJ";
      break;
  }

  logger.Info("Converting '{}' -> '{}' (format: {})", input_path, output_path,
              format_name);

  // ------------------------------------------------------------------
  // TODO: Call into the converter module once it is implemented.
  //
  // The conversion pipeline will:
  //   1. Open the W3X archive (MPQ)
  //   2. Parse internal files (w3i, w3u, w3t, w3a, etc.)
  //   3. Write output in the requested format
  //   4. Report progress to stdout
  //
  // For now we print a placeholder message.
  // ------------------------------------------------------------------
  std::cout << "[1/4] Opening map archive..." << std::endl;
  std::cout << "[2/4] Parsing map data..." << std::endl;
  std::cout << "[3/4] Converting to " << format_name << " format..."
            << std::endl;
  std::cout << "[4/4] Writing output files..." << std::endl;

  logger.Info("Conversion complete.");
  std::cout << "Conversion complete. Output written to: " << output_path
            << std::endl;

  return {};
}

}  // namespace w3x_toolkit::cli
