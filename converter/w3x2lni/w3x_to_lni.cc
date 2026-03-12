#include "converter/w3x2lni/w3x_to_lni.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <sstream>
#include <string>
#include <utility>

#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"
#include "parser/w3x/object_pack_generator.h"
#include "parser/w3x/workspace_layout.h"

namespace w3x_toolkit::converter
{

  namespace
  {

    std::string Lower(std::string value)
    {
      std::transform(value.begin(), value.end(), value.begin(),
                     [](unsigned char ch)
                     { return static_cast<char>(std::tolower(ch)); });
      return value;
    }

    bool HasExtension(const std::filesystem::path &path,
                      std::string_view extension)
    {
      return Lower(path.extension().string()) == Lower(std::string(extension));
    }

    bool IsResourceFile(const std::filesystem::path &path)
    {
      return HasExtension(path, ".mdx") || HasExtension(path, ".mdl") ||
             HasExtension(path, ".blp") || HasExtension(path, ".tga") ||
             HasExtension(path, ".dds") || HasExtension(path, ".tif");
    }

    bool IsSoundFile(const std::filesystem::path &path)
    {
      return HasExtension(path, ".wav") || HasExtension(path, ".mp3") ||
             HasExtension(path, ".ogg") || HasExtension(path, ".flac");
    }

    bool IsMapBinaryFile(const std::filesystem::path &path)
    {
      static const std::array<std::string_view, 10> kMapExtensions = {
          ".w3e", ".wpm", ".shd", ".doo", ".mmp",
          ".wts", ".w3i", ".w3r", ".w3c", ".w3s"};
      const std::string ext = Lower(path.extension().string());
      return std::ranges::find(kMapExtensions, ext) != kMapExtensions.end();
    }

    bool StartsWith(std::string_view value, std::string_view prefix)
    {
      return value.substr(0, prefix.size()) == prefix;
    }

  } // namespace

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

  void W3xToLniConverter::SetOptions(const W3xToLniOptions &options)
  {
    options_ = options;
  }

  const W3xToLniOptions &W3xToLniConverter::GetOptions() const
  {
    return options_;
  }

  void W3xToLniConverter::SetProgressCallback(ProgressCallback callback)
  {
    progress_callback_ = std::move(callback);
  }

  // ---------------------------------------------------------------------------
  // Progress
  // ---------------------------------------------------------------------------

  bool W3xToLniConverter::ReportProgress(std::string_view phase, std::size_t done, std::size_t total, double overall)
  {
    if (cancelled_)
    {
      return false;
    }
    if (progress_callback_)
    {
      ConversionProgress progress;
      progress.phase = std::string(phase);
      progress.items_done = done;
      progress.items_total = total;
      progress.overall_fraction = overall;
      if (!progress_callback_(progress))
      {
        cancelled_ = true;
        return false;
      }
    }
    return true;
  }

  // ---------------------------------------------------------------------------
  // Conversion entry point
  // ---------------------------------------------------------------------------

  core::Result<void> W3xToLniConverter::Convert()
  {
    cancelled_ = false;

    core::Logger::Instance().Info("W3xToLni: converting {} -> {}",
                                  input_path_.string(),
                                  output_path_.string());

    if (!core::FilesystemUtils::Exists(input_path_))
    {
      return std::unexpected(core::Error::FileNotFound(
          "Input path does not exist: " + input_path_.string()));
    }

    W3X_RETURN_IF_ERROR(PrepareOutputDirectory());

    if (!ReportProgress("Preparing", 0, 4, 0.0))
    {
      return std::unexpected(
          core::Error::ConvertError("Conversion cancelled by user"));
    }

    if (options_.extract_map_files)
    {
      W3X_RETURN_IF_ERROR(ExtractMapFiles());
    }
    if (!ReportProgress("Map files extracted", 1, 4, 0.25))
    {
      return std::unexpected(
          core::Error::ConvertError("Conversion cancelled by user"));
    }

    if (options_.extract_table_data)
    {
      W3X_RETURN_IF_ERROR(ConvertTableData());
    }
    if (!ReportProgress("Table data converted", 2, 4, 0.50))
    {
      return std::unexpected(
          core::Error::ConvertError("Conversion cancelled by user"));
    }

    if (options_.extract_triggers)
    {
      W3X_RETURN_IF_ERROR(ExtractTriggers());
    }
    if (!ReportProgress("Triggers extracted", 3, 4, 0.75))
    {
      return std::unexpected(
          core::Error::ConvertError("Conversion cancelled by user"));
    }

    if (options_.generate_config)
    {
      W3X_RETURN_IF_ERROR(WriteConfig());
    }
    W3X_RETURN_IF_ERROR(
        parser::w3x::FinalizeWorkspaceDirectory(input_path_, output_path_));
    ReportProgress("Complete", 4, 4, 1.0);

    core::Logger::Instance().Info("W3xToLni: conversion complete");
    return {};
  }

  // ---------------------------------------------------------------------------
  // Conversion phases
  // ---------------------------------------------------------------------------

  core::Result<void> W3xToLniConverter::PrepareOutputDirectory()
  {
    auto map_dir = output_path_ / "map";
    auto resource_dir = output_path_ / "resource";
    auto sound_dir = output_path_ / "sound";
    auto table_dir = output_path_ / "table";
    auto trigger_dir = output_path_ / "trigger";
    auto toolkit_dir = output_path_ / "w3x2lni";

    for (const auto &dir :
         {output_path_, map_dir, resource_dir, sound_dir, table_dir, trigger_dir,
          toolkit_dir})
    {
      auto result = core::FilesystemUtils::CreateDirectories(dir);
      if (!result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to create directory: " + dir.string() + ": " +
            result.error()));
      }
    }
    return {};
  }

  core::Result<void> W3xToLniConverter::ExtractMapFiles()
  {
    core::Logger::Instance().Debug("W3xToLni: extracting map files");

    auto map_output = output_path_ / "map";
    auto resource_output = output_path_ / "resource";
    auto sound_output = output_path_ / "sound";

    // When the input is a directory, copy recognized map files.
    if (core::FilesystemUtils::IsDirectory(input_path_))
    {
      auto files_result =
          core::FilesystemUtils::ListDirectoryRecursive(input_path_);
      if (!files_result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to list input directory: " + files_result.error()));
      }

      const auto &files = files_result.value();
      std::size_t index = 0;
      for (const auto &file : files)
      {
        if (!core::FilesystemUtils::IsFile(file))
        {
          continue;
        }
        if (!IsMapBinaryFile(file) && !IsResourceFile(file) && !IsSoundFile(file))
        {
          continue;
        }

        auto relative = std::filesystem::relative(file, input_path_);
        std::filesystem::path dest_root = map_output;
        if (IsResourceFile(file))
        {
          dest_root = resource_output;
        }
        else if (IsSoundFile(file))
        {
          dest_root = sound_output;
        }
        auto dest = dest_root / relative;

        auto parent_result =
            core::FilesystemUtils::CreateDirectories(dest.parent_path());
        if (!parent_result.has_value())
        {
          return std::unexpected(core::Error::IOError(
              "Failed to create directory: " + parent_result.error()));
        }

        auto copy_result = core::FilesystemUtils::CopyFile(file, dest);
        if (!copy_result.has_value())
        {
          core::Logger::Instance().Warn("W3xToLni: failed to copy {}: {}",
                                        file.string(), copy_result.error());
        }
        ++index;
      }
      core::Logger::Instance().Info("W3xToLni: extracted {} map files", index);
    }
    return {};
  }

  core::Result<void> W3xToLniConverter::ConvertTableData()
  {
    core::Logger::Instance().Debug("W3xToLni: converting table data");

    auto table_output = output_path_ / "table";

    W3X_ASSIGN_OR_RETURN(
        auto derived_files,
        parser::w3x::GenerateDerivedTableFiles(input_path_)
            .transform_error([](const core::Error& error) {
              return core::Error::ConvertError(
                  "Failed to derive table files from object data: " +
                  error.message());
            }));
    for (const auto& generated : derived_files)
    {
      auto write_result = core::FilesystemUtils::WriteBinaryFile(
          table_output / generated.relative_path, generated.content);
      if (!write_result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to write derived table file '" +
            (table_output / generated.relative_path).string() + "': " +
            write_result.error()));
      }
    }

    if (core::FilesystemUtils::IsDirectory(input_path_))
    {
      auto files_result =
          core::FilesystemUtils::ListDirectoryRecursive(input_path_);
      if (!files_result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to list input directory: " + files_result.error()));
      }

      const auto &files = files_result.value();
      std::size_t index = 0;
      for (const auto &file : files)
      {
        if (!core::FilesystemUtils::IsFile(file))
        {
          continue;
        }
        const auto relative = std::filesystem::relative(file, input_path_);
        const std::string workspace_path =
            parser::w3x::ToWorkspacePath(relative.generic_string());
        if (!StartsWith(workspace_path, "table/"))
        {
          continue;
        }

        const auto dest = output_path_ / workspace_path;
        auto parent_result = core::FilesystemUtils::CreateDirectories(dest.parent_path());
        if (!parent_result.has_value())
        {
          return std::unexpected(core::Error::IOError(
              "Failed to create directory: " + parent_result.error()));
        }

        auto copy_result = core::FilesystemUtils::CopyFile(file, dest);
        if (!copy_result.has_value())
        {
          core::Logger::Instance().Warn("W3xToLni: failed to copy {}: {}",
                                        file.string(), copy_result.error());
        }
        ++index;
      }
      core::Logger::Instance().Info("W3xToLni: converted {} table files",
                                    index);
    }
    return {};
  }

  core::Result<void> W3xToLniConverter::ExtractTriggers()
  {
    core::Logger::Instance().Debug("W3xToLni: extracting triggers");

    auto trigger_output = output_path_ / "trigger";

    if (core::FilesystemUtils::IsDirectory(input_path_))
    {
      auto files_result =
          core::FilesystemUtils::ListDirectoryRecursive(input_path_);
      if (!files_result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to list input directory: " + files_result.error()));
      }

      const auto &files = files_result.value();
      std::size_t index = 0;
      for (const auto &file : files)
      {
        if (!core::FilesystemUtils::IsFile(file))
        {
          continue;
        }
        auto ext = core::FilesystemUtils::GetExtension(file);
        if (ext != ".j" && ext != ".lua" && ext != ".ai")
        {
          continue;
        }

        auto relative = std::filesystem::relative(file, input_path_);
        auto dest = trigger_output / relative;

        auto parent_result =
            core::FilesystemUtils::CreateDirectories(dest.parent_path());
        if (!parent_result.has_value())
        {
          return std::unexpected(core::Error::IOError(
              "Failed to create directory: " + parent_result.error()));
        }

        auto copy_result = core::FilesystemUtils::CopyFile(file, dest);
        if (!copy_result.has_value())
        {
          core::Logger::Instance().Warn("W3xToLni: failed to copy {}: {}",
                                        file.string(), copy_result.error());
        }
        ++index;
      }
      core::Logger::Instance().Info("W3xToLni: extracted {} trigger files",
                                    index);
    }
    return {};
  }

  core::Result<void> W3xToLniConverter::WriteConfig()
  {
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
    if (!result.has_value())
    {
      return std::unexpected(core::Error::IOError(
          "Failed to write config: " + result.error()));
    }

    return {};
  }

} // namespace w3x_toolkit::converter
