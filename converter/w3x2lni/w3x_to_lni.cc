#include "converter/w3x2lni/w3x_to_lni.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <utility>

#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::converter {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

W3xToLniConverter::W3xToLniConverter(std::filesystem::path input_path,
                                     std::filesystem::path output_path)
    : input_path_(std::move(input_path)),
      output_path_(std::move(output_path)) {}

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------

void W3xToLniConverter::SetOptions(const W3xToLniOptions& options) {
  options_ = options;
}

const W3xToLniOptions& W3xToLniConverter::GetOptions() const {
  return options_;
}

void W3xToLniConverter::SetProgressCallback(ProgressCallback callback) {
  progress_callback_ = std::move(callback);
}

// ---------------------------------------------------------------------------
// Progress
// ---------------------------------------------------------------------------

bool W3xToLniConverter::ReportProgress(std::string_view phase,
                                       std::size_t done, std::size_t total,
                                       double overall) {
  if (cancelled_) {
    return false;
  }
  if (progress_callback_) {
    ConversionProgress progress;
    progress.phase = std::string(phase);
    progress.items_done = done;
    progress.items_total = total;
    progress.overall_fraction = overall;
    if (!progress_callback_(progress)) {
      cancelled_ = true;
      return false;
    }
  }
  return true;
}

// ---------------------------------------------------------------------------
// Conversion entry point
// ---------------------------------------------------------------------------

core::Result<void> W3xToLniConverter::Convert() {
  cancelled_ = false;

  core::Logger::Instance().Info("W3xToLni: converting {} -> {}",
                                input_path_.string(),
                                output_path_.string());

  if (!core::FilesystemUtils::Exists(input_path_)) {
    return std::unexpected(core::Error::FileNotFound(
        "Input path does not exist: " + input_path_.string()));
  }

  W3X_RETURN_IF_ERROR(PrepareOutputDirectory());

  if (!ReportProgress("Preparing", 0, 4, 0.0)) {
    return std::unexpected(
        core::Error::ConvertError("Conversion cancelled by user"));
  }

  if (options_.extract_map_files) {
    W3X_RETURN_IF_ERROR(ExtractMapFiles());
  }
  if (!ReportProgress("Map files extracted", 1, 4, 0.25)) {
    return std::unexpected(
        core::Error::ConvertError("Conversion cancelled by user"));
  }

  if (options_.extract_table_data) {
    W3X_RETURN_IF_ERROR(ConvertTableData());
  }
  if (!ReportProgress("Table data converted", 2, 4, 0.50)) {
    return std::unexpected(
        core::Error::ConvertError("Conversion cancelled by user"));
  }

  if (options_.extract_triggers) {
    W3X_RETURN_IF_ERROR(ExtractTriggers());
  }
  if (!ReportProgress("Triggers extracted", 3, 4, 0.75)) {
    return std::unexpected(
        core::Error::ConvertError("Conversion cancelled by user"));
  }

  if (options_.generate_config) {
    W3X_RETURN_IF_ERROR(WriteConfig());
  }
  ReportProgress("Complete", 4, 4, 1.0);

  core::Logger::Instance().Info("W3xToLni: conversion complete");
  return {};
}

// ---------------------------------------------------------------------------
// Conversion phases
// ---------------------------------------------------------------------------

core::Result<void> W3xToLniConverter::PrepareOutputDirectory() {
  auto map_dir = output_path_ / "map";
  auto table_dir = output_path_ / "table";
  auto trigger_dir = output_path_ / "trigger";

  for (const auto& dir : {output_path_, map_dir, table_dir, trigger_dir}) {
    auto result = core::FilesystemUtils::CreateDirectories(dir);
    if (!result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to create directory: " + dir.string() + ": " +
          result.error()));
    }
  }
  return {};
}

core::Result<void> W3xToLniConverter::ExtractMapFiles() {
  core::Logger::Instance().Debug("W3xToLni: extracting map files");

  auto map_output = output_path_ / "map";

  // When the input is a directory, copy recognized map files.
  if (core::FilesystemUtils::IsDirectory(input_path_)) {
    auto files_result =
        core::FilesystemUtils::ListDirectoryRecursive(input_path_);
    if (!files_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to list input directory: " + files_result.error()));
    }

    const auto& files = files_result.value();
    std::size_t index = 0;
    for (const auto& file : files) {
      if (!core::FilesystemUtils::IsFile(file)) {
        continue;
      }
      auto ext = core::FilesystemUtils::GetExtension(file);
      // Copy map-specific binary files.
      static const std::vector<std::string> kMapExtensions = {
          ".w3e", ".wpm", ".shd", ".doo", ".mmp", ".wts",
          ".w3i", ".w3r", ".w3c", ".w3s", ".blp", ".tga",
          ".mdx", ".mdl", ".wav", ".mp3",
      };
      bool is_map_file =
          std::find(kMapExtensions.begin(), kMapExtensions.end(), ext) !=
          kMapExtensions.end();
      if (!is_map_file) {
        continue;
      }

      auto relative = std::filesystem::relative(file, input_path_);
      auto dest = map_output / relative;

      auto parent_result =
          core::FilesystemUtils::CreateDirectories(dest.parent_path());
      if (!parent_result.has_value()) {
        return std::unexpected(core::Error::IOError(
            "Failed to create directory: " + parent_result.error()));
      }

      auto copy_result = core::FilesystemUtils::CopyFile(file, dest);
      if (!copy_result.has_value()) {
        core::Logger::Instance().Warn("W3xToLni: failed to copy {}: {}",
                                       file.string(), copy_result.error());
      }
      ++index;
    }
    core::Logger::Instance().Info("W3xToLni: extracted {} map files", index);
  } else {
    // Input is a .w3x archive file -- archive reading would be handled by
    // the MPQ reader which is not yet implemented.  Log a placeholder note.
    core::Logger::Instance().Warn(
        "W3xToLni: archive extraction requires MPQ reader (not yet "
        "available); skipping map file extraction from archive");
  }

  return {};
}

core::Result<void> W3xToLniConverter::ConvertTableData() {
  core::Logger::Instance().Debug("W3xToLni: converting table data");

  auto table_output = output_path_ / "table";

  if (core::FilesystemUtils::IsDirectory(input_path_)) {
    auto files_result =
        core::FilesystemUtils::ListDirectoryRecursive(input_path_);
    if (!files_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to list input directory: " + files_result.error()));
    }

    const auto& files = files_result.value();
    std::size_t index = 0;
    for (const auto& file : files) {
      if (!core::FilesystemUtils::IsFile(file)) {
        continue;
      }
      auto ext = core::FilesystemUtils::GetExtension(file);
      // SLK / object data files.
      static const std::vector<std::string> kTableExtensions = {
          ".slk", ".txt", ".w3u", ".w3t", ".w3a", ".w3b",
          ".w3d", ".w3q", ".w3h",
      };
      bool is_table_file =
          std::find(kTableExtensions.begin(), kTableExtensions.end(), ext) !=
          kTableExtensions.end();
      if (!is_table_file) {
        continue;
      }

      // Read the file and write it to table/ as-is for now.
      // Full SLK -> INI conversion requires the SLK parser.
      auto relative = std::filesystem::relative(file, input_path_);
      auto dest = table_output / relative;
      dest.replace_extension(".ini");

      auto parent_result =
          core::FilesystemUtils::CreateDirectories(dest.parent_path());
      if (!parent_result.has_value()) {
        return std::unexpected(core::Error::IOError(
            "Failed to create directory: " + parent_result.error()));
      }

      auto content = core::FilesystemUtils::ReadTextFile(file);
      if (!content.has_value()) {
        core::Logger::Instance().Warn("W3xToLni: failed to read {}: {}",
                                       file.string(), content.error());
        continue;
      }

      // Write the raw content -- when the SLK parser is available this
      // will be replaced with proper INI serialization.
      auto write_result =
          core::FilesystemUtils::WriteTextFile(dest, content.value());
      if (!write_result.has_value()) {
        core::Logger::Instance().Warn("W3xToLni: failed to write {}: {}",
                                       dest.string(), write_result.error());
      }
      ++index;
    }
    core::Logger::Instance().Info("W3xToLni: converted {} table files",
                                  index);
  } else {
    core::Logger::Instance().Warn(
        "W3xToLni: archive table extraction requires MPQ reader");
  }

  return {};
}

core::Result<void> W3xToLniConverter::ExtractTriggers() {
  core::Logger::Instance().Debug("W3xToLni: extracting triggers");

  auto trigger_output = output_path_ / "trigger";

  if (core::FilesystemUtils::IsDirectory(input_path_)) {
    auto files_result =
        core::FilesystemUtils::ListDirectoryRecursive(input_path_);
    if (!files_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to list input directory: " + files_result.error()));
    }

    const auto& files = files_result.value();
    std::size_t index = 0;
    for (const auto& file : files) {
      if (!core::FilesystemUtils::IsFile(file)) {
        continue;
      }
      auto ext = core::FilesystemUtils::GetExtension(file);
      if (ext != ".j" && ext != ".lua" && ext != ".ai") {
        continue;
      }

      auto relative = std::filesystem::relative(file, input_path_);
      auto dest = trigger_output / relative;

      auto parent_result =
          core::FilesystemUtils::CreateDirectories(dest.parent_path());
      if (!parent_result.has_value()) {
        return std::unexpected(core::Error::IOError(
            "Failed to create directory: " + parent_result.error()));
      }

      auto copy_result = core::FilesystemUtils::CopyFile(file, dest);
      if (!copy_result.has_value()) {
        core::Logger::Instance().Warn("W3xToLni: failed to copy {}: {}",
                                       file.string(), copy_result.error());
      }
      ++index;
    }
    core::Logger::Instance().Info("W3xToLni: extracted {} trigger files",
                                  index);
  } else {
    core::Logger::Instance().Warn(
        "W3xToLni: archive trigger extraction requires MPQ reader");
  }

  return {};
}

core::Result<void> W3xToLniConverter::WriteConfig() {
  core::Logger::Instance().Debug("W3xToLni: writing config");

  auto config_path = output_path_ / "w3x2lni.ini";

  std::ostringstream oss;
  oss << "[w3x2lni]\n";
  oss << "version = 1\n";
  oss << "input = " << input_path_.filename().string() << "\n";
  oss << "\n";
  oss << "[options]\n";
  oss << "remove_unused_objects = "
      << (options_.remove_unused_objects ? "true" : "false") << "\n";
  oss << "inline_wts_strings = "
      << (options_.inline_wts_strings ? "true" : "false") << "\n";

  auto result =
      core::FilesystemUtils::WriteTextFile(config_path, oss.str());
  if (!result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to write config: " + result.error()));
  }

  return {};
}

}  // namespace w3x_toolkit::converter
