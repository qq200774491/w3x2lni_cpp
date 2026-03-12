#ifndef W3X_TOOLKIT_PARSER_W3X_MAP_ARCHIVE_IO_H_
#define W3X_TOOLKIT_PARSER_W3X_MAP_ARCHIVE_IO_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

inline constexpr std::string_view kUnpackManifestFileName =
    ".w3x_manifest.json";

enum class PackProfile : std::uint8_t {
  kDefault = 0,
  kObj = 1,
  kSlk = 2,
};

struct PackOptions {
  PackProfile profile = PackProfile::kDefault;
  bool slk_doodad = true;
  bool read_slk = false;
  bool remove_unused_objects = false;
  bool remove_we_only = false;
  bool remove_same = false;
  bool computed_text = false;
};

enum class ArchiveNameStatus : std::uint8_t {
  kOriginal = 0,
  kRecovered = 1,
  kAnonymous = 2,
};

struct ManifestEntry {
  std::string archive_path;
  std::string disk_path;
  std::string source_name;
  ArchiveNameStatus status = ArchiveNameStatus::kOriginal;
  std::uint64_t content_hash = 0;
  std::uint64_t size = 0;
};

struct UnpackManifest {
  std::string archive_file;
  std::vector<ManifestEntry> written_files;
  std::vector<ManifestEntry> skipped_duplicates;
  std::vector<std::string> internal_files;
};

core::Result<UnpackManifest> UnpackMapArchive(
    const std::filesystem::path& archive_path,
    const std::filesystem::path& output_dir);

core::Result<std::size_t> PackMapDirectory(
    const std::filesystem::path& input_dir,
    const std::filesystem::path& archive_path,
    const PackOptions& options = {});

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_MAP_ARCHIVE_IO_H_
