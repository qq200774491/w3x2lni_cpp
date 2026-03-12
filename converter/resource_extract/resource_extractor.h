#ifndef W3X_TOOLKIT_CONVERTER_RESOURCE_EXTRACTOR_H_
#define W3X_TOOLKIT_CONVERTER_RESOURCE_EXTRACTOR_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {
class W3xArchive;
}

namespace w3x_toolkit::converter
{

  // Classification of resources found inside a W3X map archive.
  enum class ResourceType : std::uint8_t
  {
    kModel,   // .mdx, .mdl
    kTexture, // .blp, .tga, .dds
    kSound,   // .wav, .mp3, .ogg, .flac
    kScript,  // .j, .lua, .ai
    kUi,      // .fdf, .toc
    kData,    // .slk, .txt, .ini, .w3u, .w3t, .w3a, .w3b, .w3d, .w3q, .w3h
    kMap,     // .w3e, .wpm, .shd, .doo, .mmp, .wts, .w3i, .w3r, .w3c, .w3s
    kUnknown, // anything else
  };

  // Returns a human-readable name for a ResourceType value.
  std::string_view ResourceTypeToString(ResourceType type);

  // Determines the ResourceType from a file extension (with leading dot).
  ResourceType ClassifyExtension(std::string_view extension);

  // Metadata for a single resource inside a W3X archive.
  struct ResourceInfo
  {
    // Archive-relative path (using forward slashes).
    std::string path;

    // Classified resource type.
    ResourceType type = ResourceType::kUnknown;

    // Uncompressed size in bytes (0 if unknown).
    std::uint64_t size = 0;
  };

  // Progress callback for extraction operations.
  // |current| is the 1-based index of the file being extracted, |total| is the
  // total number of files.  Return false to cancel.
  using ExtractionProgressCallback =
      std::function<bool(std::size_t current, std::size_t total,
                         std::string_view current_file)>;

  // Extracts resources from Warcraft III map archives (.w3x / .w3m).
  //
  // The extractor reads the archive file list (from the MPQ listfile or a
  // directory listing when the map is already unpacked) and can extract
  // individual files, files of a specific type, or everything at once.
  //
  // Usage:
  //   ResourceExtractor extractor(map_path);
  //   auto resources = extractor.ListResources();
  //   extractor.SetOutputPath(output_dir);
  //   extractor.ExtractByType(ResourceType::kModel);
  //
  class ResourceExtractor
  {
  public:
    // Constructs an extractor for the archive (or directory) at |archive_path|.
    explicit ResourceExtractor(std::filesystem::path archive_path);

    ~ResourceExtractor();

    // Non-copyable, movable.
    ResourceExtractor(const ResourceExtractor &) = delete;
    ResourceExtractor &operator=(const ResourceExtractor &) = delete;
    ResourceExtractor(ResourceExtractor &&) noexcept = default;
    ResourceExtractor &operator=(ResourceExtractor &&) noexcept = default;

    // ---------------------------------------------------------------------------
    // Configuration
    // ---------------------------------------------------------------------------

    // Sets the output directory where extracted files will be written.
    // Directory structure inside the archive is preserved.
    void SetOutputPath(const std::filesystem::path &output_path);

    // Returns the current output path.
    const std::filesystem::path &GetOutputPath() const;

    // Installs a progress callback invoked during extraction.
    void SetProgressCallback(ExtractionProgressCallback callback);

    // ---------------------------------------------------------------------------
    // Resource listing
    // ---------------------------------------------------------------------------

    // Returns information about every resource in the archive.
    core::Result<std::vector<ResourceInfo>> ListResources();

    // Returns information about resources matching |type|.
    core::Result<std::vector<ResourceInfo>> ListResourcesByType(
        ResourceType type);

    // ---------------------------------------------------------------------------
    // Extraction
    // ---------------------------------------------------------------------------

    // Extracts all files from the archive to the output directory.
    // Returns the number of files successfully extracted.
    core::Result<std::size_t> ExtractAll();

    // Extracts only files matching |type| to the output directory.
    // Returns the number of files successfully extracted.
    core::Result<std::size_t> ExtractByType(ResourceType type);

    // Extracts a single file identified by its archive-relative |path|.
    // The file is written under the output directory preserving its relative
    // path, or to |destination| if provided.
    core::Result<void> ExtractFile(
        std::string_view path,
        const std::filesystem::path &destination = {});

  private:
    // Populates resource_list_ by scanning the archive.  Called lazily on the
    // first operation that needs the file list.
    core::Result<void> EnsureResourceListLoaded();

    // Reads the raw bytes of a single file from the archive.
    core::Result<std::vector<std::uint8_t>> ReadArchiveFile(
        std::string_view path);

    // Writes |data| to |dest_path|, creating parent directories as needed.
    core::Result<void> WriteFile(const std::filesystem::path &dest_path,
                                 const std::vector<std::uint8_t> &data);

    // Reports progress and checks for cancellation.
    bool ReportProgress(std::size_t current, std::size_t total,
                        std::string_view file);

    std::filesystem::path archive_path_;
    std::filesystem::path output_path_;
    ExtractionProgressCallback progress_callback_;
    std::unique_ptr<parser::w3x::W3xArchive> archive_;
    std::vector<ResourceInfo> resource_list_;
    bool resource_list_loaded_ = false;
  };

} // namespace w3x_toolkit::converter

#endif // W3X_TOOLKIT_CONVERTER_RESOURCE_EXTRACTOR_H_
