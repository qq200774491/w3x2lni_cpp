// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/analyze_command.h"

#include <filesystem>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::cli
{

  // ---------------------------------------------------------------------------
  // Command interface
  // ---------------------------------------------------------------------------

  std::string AnalyzeCommand::Name() const { return "analyze"; }

  std::string AnalyzeCommand::Description() const
  {
    return "Analyze a W3X map structure and print statistics";
  }

  std::string AnalyzeCommand::Usage() const
  {
    return "analyze <input.w3x>";
  }

  core::Result<void> AnalyzeCommand::Execute(
      const std::vector<std::string> &args)
  {
    if (args.empty())
    {
      return std::unexpected(core::Error(
          core::ErrorCode::kInvalidFormat,
          "Missing required argument.\nUsage: " + Usage()));
    }

    const std::string &input_path = args[0];

    // Warn about extra arguments but don't fail.
    if (args.size() > 1)
    {
      core::Logger::Instance().Warn(
          "Ignoring extra arguments after input path");
    }

    return RunAnalysis(input_path);
  }

  // ---------------------------------------------------------------------------
  // Private helpers
  // ---------------------------------------------------------------------------

  core::Result<void> AnalyzeCommand::RunAnalysis(
      const std::string &input_path)
  {
    auto &logger = core::Logger::Instance();

    // Validate input file exists.
    std::filesystem::path input(input_path);
    if (!core::FilesystemUtils::Exists(input))
    {
      return std::unexpected(
          core::Error::FileNotFound("Input file not found: " + input_path));
    }
    if (!core::FilesystemUtils::IsFile(input))
    {
      return std::unexpected(core::Error(
          core::ErrorCode::kInvalidFormat,
          "Input path is not a file: " + input_path));
    }

    // Get file size for the summary.
    auto file_size_result = core::FilesystemUtils::GetFileSize(input);
    std::uintmax_t file_size = 0;
    if (file_size_result.has_value())
    {
      file_size = file_size_result.value();
    }

    logger.Info("Analyzing map: {}", input_path);

    // ------------------------------------------------------------------
    // TODO: Call into the parser modules (w3x, w3i, etc.) once implemented.
    //
    // A full analysis would:
    //   1. Open the W3X archive (MPQ)
    //   2. Parse war3map.w3i for map metadata
    //   3. Enumerate all files and categorize them
    //   4. Compute statistics
    //
    // For now we output what we can determine from the filesystem alone
    // and show a placeholder for archive contents.
    // ------------------------------------------------------------------

    // Print header.
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "  W3X Map Analysis" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::endl;

    // File info.
    std::cout << "File Information:" << std::endl;
    std::cout << "  Path:      " << input_path << std::endl;
    std::cout << "  Filename:  "
              << core::FilesystemUtils::GetFilename(input) << std::endl;
    std::cout << "  Size:      " << file_size << " bytes";
    if (file_size >= 1024 * 1024)
    {
      std::cout << " (" << std::fixed << std::setprecision(2)
                << static_cast<double>(file_size) / (1024.0 * 1024.0) << " MB)";
    }
    else if (file_size >= 1024)
    {
      std::cout << " (" << std::fixed << std::setprecision(2)
                << static_cast<double>(file_size) / 1024.0 << " KB)";
    }
    std::cout << std::endl;
    std::cout << std::endl;

    // Map info placeholder.
    std::cout << "Map Info (war3map.w3i):" << std::endl;
    std::cout << "  (Requires W3I parser -- not yet implemented)" << std::endl;
    std::cout << std::endl;

    // File listing placeholder.
    std::cout << "Archive Contents:" << std::endl;
    std::cout << "  (Requires MPQ reader -- not yet implemented)" << std::endl;
    std::cout << std::endl;

    // Statistics placeholder.
    std::cout << "Statistics:" << std::endl;
    std::cout << "  (Requires archive enumeration -- not yet implemented)"
              << std::endl;
    std::cout << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    logger.Info("Analysis complete.");
    return {};
  }

} // namespace w3x_toolkit::cli
