#include "converter/resource_extract/resource_extractor.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::converter
{

  // ---------------------------------------------------------------------------
  // ResourceType utilities
  // ---------------------------------------------------------------------------

  std::string_view ResourceTypeToString(ResourceType type)
  {
    switch (type)
    {
    case ResourceType::kModel:
      return "Model";
    case ResourceType::kTexture:
      return "Texture";
    case ResourceType::kSound:
      return "Sound";
    case ResourceType::kScript:
      return "Script";
    case ResourceType::kUi:
      return "UI";
    case ResourceType::kData:
      return "Data";
    case ResourceType::kMap:
      return "Map";
    case ResourceType::kUnknown:
      return "Unknown";
    }
    return "Unknown";
  }

  ResourceType ClassifyExtension(std::string_view extension)
  {
    // Normalize to lowercase.
    std::string ext(extension);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c)
                   { return std::tolower(c); });

    // Models.
    if (ext == ".mdx" || ext == ".mdl")
      return ResourceType::kModel;

    // Textures.
    if (ext == ".blp" || ext == ".tga" || ext == ".dds")
      return ResourceType::kTexture;

    // Sounds.
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
      return ResourceType::kSound;

    // Scripts.
    if (ext == ".j" || ext == ".lua" || ext == ".ai")
      return ResourceType::kScript;

    // UI.
    if (ext == ".fdf" || ext == ".toc")
      return ResourceType::kUi;

    // Data.
    if (ext == ".slk" || ext == ".txt" || ext == ".ini" || ext == ".w3u" ||
        ext == ".w3t" || ext == ".w3a" || ext == ".w3b" || ext == ".w3d" ||
        ext == ".w3q" || ext == ".w3h")
    {
      return ResourceType::kData;
    }

    // Map-specific binaries.
    if (ext == ".w3e" || ext == ".wpm" || ext == ".shd" || ext == ".doo" ||
        ext == ".mmp" || ext == ".wts" || ext == ".w3i" || ext == ".w3r" ||
        ext == ".w3c" || ext == ".w3s")
    {
      return ResourceType::kMap;
    }

    return ResourceType::kUnknown;
  }

  // ---------------------------------------------------------------------------
  // Construction
  // ---------------------------------------------------------------------------

  ResourceExtractor::ResourceExtractor(std::filesystem::path archive_path)
      : archive_path_(std::move(archive_path)) {}

  void ResourceExtractor::SetOutputPath(
      const std::filesystem::path &output_path)
  {
    output_path_ = output_path;
  }

  const std::filesystem::path &ResourceExtractor::GetOutputPath() const
  {
    return output_path_;
  }

  void ResourceExtractor::SetProgressCallback(
      ExtractionProgressCallback callback)
  {
    progress_callback_ = std::move(callback);
  }

  // ---------------------------------------------------------------------------
  // Progress
  // ---------------------------------------------------------------------------

  bool ResourceExtractor::ReportProgress(std::size_t current, std::size_t total,
                                         std::string_view file)
  {
    if (progress_callback_)
    {
      return progress_callback_(current, total, file);
    }
    return true;
  }

  // ---------------------------------------------------------------------------
  // Resource list
  // ---------------------------------------------------------------------------

  core::Result<void> ResourceExtractor::EnsureResourceListLoaded()
  {
    if (resource_list_loaded_)
    {
      return {};
    }

    if (!core::FilesystemUtils::Exists(archive_path_))
    {
      return std::unexpected(core::Error::FileNotFound(
          "Archive path does not exist: " + archive_path_.string()));
    }

    if (core::FilesystemUtils::IsDirectory(archive_path_))
    {
      // Scan directory recursively.
      auto files =
          core::FilesystemUtils::ListDirectoryRecursive(archive_path_);
      if (!files.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to list archive directory: " + files.error()));
      }

      resource_list_.clear();
      resource_list_.reserve(files.value().size());

      for (const auto &file : files.value())
      {
        if (!core::FilesystemUtils::IsFile(file))
        {
          continue;
        }

        ResourceInfo info;
        auto relative = std::filesystem::relative(file, archive_path_);
        // Use forward slashes for the archive-relative path.
        info.path = relative.generic_string();

        auto ext = core::FilesystemUtils::GetExtension(file);
        info.type = ClassifyExtension(ext);

        auto size = core::FilesystemUtils::GetFileSize(file);
        if (size.has_value())
        {
          info.size = size.value();
        }

        resource_list_.push_back(std::move(info));
      }
    }
    else
    {
      // The input is an archive file (.w3x / .w3m).
      // Full MPQ reading is not yet implemented.  For now, report the
      // archive itself as a single resource.
      core::Logger::Instance().Warn(
          "ResourceExtractor: MPQ archive reading not yet implemented; "
          "only directory-based extraction is supported");

      ResourceInfo info;
      info.path = archive_path_.filename().string();
      info.type = ResourceType::kUnknown;
      auto size = core::FilesystemUtils::GetFileSize(archive_path_);
      if (size.has_value())
      {
        info.size = size.value();
      }
      resource_list_.push_back(std::move(info));
    }

    resource_list_loaded_ = true;
    core::Logger::Instance().Info(
        "ResourceExtractor: loaded {} resources from {}",
        resource_list_.size(), archive_path_.string());
    return {};
  }

  core::Result<std::vector<ResourceInfo>> ResourceExtractor::ListResources()
  {
    W3X_RETURN_IF_ERROR(EnsureResourceListLoaded());
    return resource_list_;
  }

  core::Result<std::vector<ResourceInfo>> ResourceExtractor::ListResourcesByType(
      ResourceType type)
  {
    W3X_RETURN_IF_ERROR(EnsureResourceListLoaded());

    std::vector<ResourceInfo> filtered;
    for (const auto &info : resource_list_)
    {
      if (info.type == type)
      {
        filtered.push_back(info);
      }
    }
    return filtered;
  }

  // ---------------------------------------------------------------------------
  // File I/O helpers
  // ---------------------------------------------------------------------------

  core::Result<std::vector<std::uint8_t>> ResourceExtractor::ReadArchiveFile(
      std::string_view path)
  {
    if (core::FilesystemUtils::IsDirectory(archive_path_))
    {
      auto full_path = archive_path_ / std::filesystem::path(path);
      auto data = core::FilesystemUtils::ReadBinaryFile(full_path);
      if (!data.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to read file: " + std::string(path) + ": " + data.error()));
      }
      return std::move(data.value());
    }

    // MPQ archive reading not yet implemented.
    return std::unexpected(core::Error::ConvertError(
        "MPQ archive reading not yet implemented for file: " +
        std::string(path)));
  }

  core::Result<void> ResourceExtractor::WriteFile(
      const std::filesystem::path &dest_path,
      const std::vector<std::uint8_t> &data)
  {
    auto parent = dest_path.parent_path();
    if (!parent.empty())
    {
      auto mkdir_result = core::FilesystemUtils::CreateDirectories(parent);
      if (!mkdir_result.has_value())
      {
        return std::unexpected(core::Error::IOError(
            "Failed to create directory: " + parent.string() + ": " +
            mkdir_result.error()));
      }
    }

    auto write_result = core::FilesystemUtils::WriteBinaryFile(dest_path, data);
    if (!write_result.has_value())
    {
      return std::unexpected(core::Error::IOError(
          "Failed to write file: " + dest_path.string() + ": " +
          write_result.error()));
    }
    return {};
  }

  // ---------------------------------------------------------------------------
  // Extraction
  // ---------------------------------------------------------------------------

  core::Result<std::size_t> ResourceExtractor::ExtractAll()
  {
    if (output_path_.empty())
    {
      return std::unexpected(
          core::Error::ConvertError("Output path not set"));
    }

    W3X_RETURN_IF_ERROR(EnsureResourceListLoaded());

    std::size_t extracted = 0;
    const std::size_t total = resource_list_.size();

    for (std::size_t i = 0; i < total; ++i)
    {
      const auto &info = resource_list_[i];

      if (!ReportProgress(i + 1, total, info.path))
      {
        core::Logger::Instance().Info(
            "ResourceExtractor: extraction cancelled at {}/{}", i + 1, total);
        return extracted;
      }

      auto data = ReadArchiveFile(info.path);
      if (!data.has_value())
      {
        core::Logger::Instance().Warn(
            "ResourceExtractor: skipping {}: {}", info.path,
            data.error().message());
        continue;
      }

      auto dest = output_path_ / std::filesystem::path(info.path);
      auto write_result = WriteFile(dest, data.value());
      if (!write_result.has_value())
      {
        core::Logger::Instance().Warn(
            "ResourceExtractor: failed to write {}: {}", dest.string(),
            write_result.error().message());
        continue;
      }

      ++extracted;
    }

    core::Logger::Instance().Info(
        "ResourceExtractor: extracted {}/{} files", extracted, total);
    return extracted;
  }

  core::Result<std::size_t> ResourceExtractor::ExtractByType(
      ResourceType type)
  {
    if (output_path_.empty())
    {
      return std::unexpected(
          core::Error::ConvertError("Output path not set"));
    }

    W3X_RETURN_IF_ERROR(EnsureResourceListLoaded());

    // Collect matching resources.
    std::vector<const ResourceInfo *> matching;
    for (const auto &info : resource_list_)
    {
      if (info.type == type)
      {
        matching.push_back(&info);
      }
    }

    std::size_t extracted = 0;
    const std::size_t total = matching.size();

    for (std::size_t i = 0; i < total; ++i)
    {
      const auto &info = *matching[i];

      if (!ReportProgress(i + 1, total, info.path))
      {
        return extracted;
      }

      auto data = ReadArchiveFile(info.path);
      if (!data.has_value())
      {
        core::Logger::Instance().Warn(
            "ResourceExtractor: skipping {}: {}", info.path,
            data.error().message());
        continue;
      }

      auto dest = output_path_ / std::filesystem::path(info.path);
      auto write_result = WriteFile(dest, data.value());
      if (!write_result.has_value())
      {
        core::Logger::Instance().Warn(
            "ResourceExtractor: failed to write {}: {}", dest.string(),
            write_result.error().message());
        continue;
      }

      ++extracted;
    }

    core::Logger::Instance().Info(
        "ResourceExtractor: extracted {}/{} {} files", extracted, total,
        ResourceTypeToString(type));
    return extracted;
  }

  core::Result<void> ResourceExtractor::ExtractFile(
      std::string_view path, const std::filesystem::path &destination)
  {
    auto data = ReadArchiveFile(path);
    if (!data.has_value())
    {
      return std::unexpected(data.error());
    }

    std::filesystem::path dest;
    if (destination.empty())
    {
      if (output_path_.empty())
      {
        return std::unexpected(
            core::Error::ConvertError("No output path or destination set"));
      }
      dest = output_path_ / std::filesystem::path(path);
    }
    else
    {
      dest = destination;
    }

    return WriteFile(dest, data.value());
  }

} // namespace w3x_toolkit::converter
