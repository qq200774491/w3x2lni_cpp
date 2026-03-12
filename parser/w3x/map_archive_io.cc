#include "parser/w3x/map_archive_io.h"

#include <StormLib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "core/filesystem/filesystem_utils.h"
#include "parser/w3x/object_pack_generator.h"
#include "parser/w3x/workspace_layout.h"
#include "parser/w3x/w3x_parser.h"

namespace w3x_toolkit::parser::w3x {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr std::array<std::string_view, 3> kInternalFiles = {
    "(listfile)",
    "(attributes)",
    "(signature)",
};

const std::vector<std::string>& KnownArchivePaths() {
  static const std::vector<std::string> kPaths = {
      "war3map.w3i",
      "war3map.wts",
      "war3map.j",
      "war3map.lua",
      "war3map.wtg",
      "war3map.wct",
      "war3map.w3u",
      "war3map.w3t",
      "war3map.w3a",
      "war3map.w3b",
      "war3map.w3d",
      "war3map.w3q",
      "war3map.w3h",
      "war3map.doo",
      "war3map.w3e",
      "war3map.wpm",
      "war3map.shd",
      "war3map.mmp",
      "war3map.w3r",
      "war3map.imp",
      "war3map.w3c",
      "war3map.w3s",
      "war3mapmisc.txt",
      "war3mapextra.txt",
      "war3mapskin.txt",
      "war3mapunits.doo",
      "war3mapMap.blp",
      "war3mapPreview.tga",
      "war3campaign.w3f",
      "ability.ini",
      "destructable.ini",
      "doodad.ini",
      "buff.ini",
      "upgrade.ini",
      "item.ini",
      "unit.ini",
      "misc.ini",
      "txt.ini",
      "units/abilitydata.slk",
      "units/destructabledata.slk",
      "units/abilitybuffdata.slk",
      "units/upgradedata.slk",
      "units/itemdata.slk",
      "units/unitui.slk",
      "units/unitdata.slk",
      "units/unitbalance.slk",
      "units/unitabilities.slk",
      "units/unitweapons.slk",
      "doodads/doodads.slk",
  };
  return kPaths;
}

std::string NormalizeArchivePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.starts_with("./")) {
    path.erase(0, 2);
  }
  return path;
}

std::string ToArchiveStoragePath(const std::string& path) {
  std::string archive_path = NormalizeArchivePath(path);
  std::replace(archive_path.begin(), archive_path.end(), '/', '\\');
  return archive_path;
}

bool IsInternalFile(const std::string& path) {
  return std::find(kInternalFiles.begin(), kInternalFiles.end(), path) !=
         kInternalFiles.end();
}

bool IsAnonymousEntryName(const std::string& path) {
  const std::string filename = fs::path(path).filename().string();
  if (filename.size() != 16 || !filename.starts_with("File")) {
    return false;
  }
  for (std::size_t i = 4; i < 12; ++i) {
    if (!std::isdigit(static_cast<unsigned char>(filename[i]))) {
      return false;
    }
  }
  return filename[12] == '.';
}

std::uint64_t HashBytes(const std::vector<std::uint8_t>& data) {
  constexpr std::uint64_t kOffset = 1469598103934665603ull;
  constexpr std::uint64_t kPrime = 1099511628211ull;
  std::uint64_t hash = kOffset;
  for (std::uint8_t byte : data) {
    hash ^= byte;
    hash *= kPrime;
  }
  return hash;
}

std::string StatusToString(ArchiveNameStatus status) {
  switch (status) {
    case ArchiveNameStatus::kOriginal:
      return "original";
    case ArchiveNameStatus::kRecovered:
      return "recovered";
    case ArchiveNameStatus::kAnonymous:
      return "anonymous";
  }
  return "original";
}

std::string EncodeManifestString(const std::string& value) {
  std::ostringstream stream;
  stream << std::hex << std::uppercase << std::setfill('0');
  for (unsigned char ch : value) {
    if (ch >= 0x20 && ch <= 0x7E && ch != '%') {
      stream << static_cast<char>(ch);
    } else {
      stream << '%' << std::setw(2) << static_cast<int>(ch);
    }
  }
  return stream.str();
}

core::Result<std::string> DecodeManifestString(const std::string& value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    const unsigned char ch = static_cast<unsigned char>(value[i]);
    if (ch != '%') {
      decoded.push_back(static_cast<char>(ch));
      continue;
    }
    if (i + 2 >= value.size()) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid percent-encoding in unpack manifest"));
    }
    const std::string hex = value.substr(i + 1, 2);
    unsigned int byte = 0;
    std::istringstream stream(hex);
    stream >> std::hex >> byte;
    if (stream.fail()) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid percent-encoding in unpack manifest"));
    }
    decoded.push_back(static_cast<char>(byte));
    i += 2;
  }
  return decoded;
}

core::Result<ArchiveNameStatus> StatusFromString(const std::string& value) {
  if (value == "original") {
    return ArchiveNameStatus::kOriginal;
  }
  if (value == "recovered") {
    return ArchiveNameStatus::kRecovered;
  }
  if (value == "anonymous") {
    return ArchiveNameStatus::kAnonymous;
  }
  return std::unexpected(core::Error::InvalidFormat(
      "Unknown manifest status value: " + value));
}

core::Result<void> WriteManifestFile(const fs::path& output_dir,
                                     const UnpackManifest& manifest) {
  json root;
  root["archive_file"] = EncodeManifestString(manifest.archive_file);
  root["internal_files"] = manifest.internal_files;
  root["written_files"] = json::array();
  for (const auto& entry : manifest.written_files) {
    root["written_files"].push_back({
        {"archive_path", EncodeManifestString(entry.archive_path)},
        {"disk_path", EncodeManifestString(entry.disk_path)},
        {"source_name", EncodeManifestString(entry.source_name)},
        {"status", StatusToString(entry.status)},
        {"content_hash", entry.content_hash},
        {"size", entry.size},
    });
  }
  root["skipped_duplicates"] = json::array();
  for (const auto& entry : manifest.skipped_duplicates) {
    root["skipped_duplicates"].push_back({
        {"archive_path", EncodeManifestString(entry.archive_path)},
        {"disk_path", EncodeManifestString(entry.disk_path)},
        {"source_name", EncodeManifestString(entry.source_name)},
        {"status", StatusToString(entry.status)},
        {"content_hash", entry.content_hash},
        {"size", entry.size},
    });
  }

  return core::FilesystemUtils::WriteTextFile(
             output_dir / std::string(kUnpackManifestFileName), root.dump(2))
             .transform_error([](const std::string& error) {
               return core::Error::IOError(
                   "Failed to write unpack manifest: " + error);
             });
}

core::Result<std::unordered_map<std::string, std::string>> LoadManifestMappings(
    const fs::path& input_dir) {
  const fs::path manifest_path =
      input_dir / std::string(kUnpackManifestFileName);
  std::unordered_map<std::string, std::string> mappings;
  if (!core::FilesystemUtils::Exists(manifest_path)) {
    return mappings;
  }

  W3X_ASSIGN_OR_RETURN(auto manifest_text,
                       core::FilesystemUtils::ReadTextFile(manifest_path)
                           .transform_error([](const std::string& error) {
                             return core::Error::IOError(
                                 "Failed to read unpack manifest: " + error);
                           }));

  json root;
  try {
    root = json::parse(manifest_text);
  } catch (const std::exception& ex) {
    return std::unexpected(core::Error::InvalidFormat(
        "Failed to parse unpack manifest: " + std::string(ex.what())));
  }

  if (!root.contains("written_files") || !root["written_files"].is_array()) {
    return std::unexpected(core::Error::InvalidFormat(
        "Unpack manifest is missing 'written_files'"));
  }

  for (const auto& entry : root["written_files"]) {
    if (!entry.contains("disk_path") || !entry.contains("archive_path")) {
      return std::unexpected(core::Error::InvalidFormat(
          "Unpack manifest entry is missing disk_path/archive_path"));
    }
    W3X_ASSIGN_OR_RETURN(
        auto disk_path,
        DecodeManifestString(entry["disk_path"].get<std::string>()));
    W3X_ASSIGN_OR_RETURN(
        auto archive_path,
        DecodeManifestString(entry["archive_path"].get<std::string>()));
    mappings.emplace(NormalizeArchivePath(disk_path),
                     NormalizeArchivePath(archive_path));
  }
  return mappings;
}

core::Result<void> WriteEntryFile(const fs::path& output_dir,
                                  const ManifestEntry& entry,
                                  const std::vector<std::uint8_t>& data) {
  return core::FilesystemUtils::WriteBinaryFile(output_dir / entry.disk_path,
                                                data)
      .transform_error([&entry](const std::string& error) {
        return core::Error::IOError("Failed to write '" + entry.disk_path +
                                    "': " + error);
      });
}

std::string FormatStormError(const std::string& action, const fs::path& path) {
  std::ostringstream stream;
  stream << action << " '" << path.string() << "' failed with Win32 error "
         << GetLastError();
  return stream.str();
}

std::string FormatStormErrorForArchiveName(const std::string& action,
                                           const std::string& archive_name) {
  std::ostringstream stream;
  stream << action << " '" << archive_name << "' failed with Win32 error "
         << GetLastError();
  return stream.str();
}

std::optional<std::string> TryRecoverAnonymousTextPath(
    const std::string& archive_name, const std::vector<std::uint8_t>& data) {
  if (!IsAnonymousEntryName(archive_name) || data.empty()) {
    return std::nullopt;
  }
  if (data.front() != static_cast<std::uint8_t>('[')) {
    return std::nullopt;
  }

  std::size_t end = 1;
  while (end < data.size() && end < 32 &&
         data[end] != static_cast<std::uint8_t>(']') &&
         data[end] != static_cast<std::uint8_t>('\r') &&
         data[end] != static_cast<std::uint8_t>('\n')) {
    ++end;
  }
  if (end >= data.size() || data[end] != static_cast<std::uint8_t>(']')) {
    return std::nullopt;
  }

  std::string section(reinterpret_cast<const char*>(data.data() + 1), end - 1);
  if (section == "A000") {
    return std::string("ability.ini");
  }
  if (section == "Ecen") {
    return std::string("unit.ini");
  }
  if (section == "Recb") {
    return std::string("upgrade.ini");
  }
  if (section == "AEsd") {
    return std::string("buff.ini");
  }
  if (section == "afac") {
    return std::string("item.ini");
  }
  if (section == "aami") {
    return std::string("txt.ini");
  }
  if (section == "HERO") {
    return std::string("misc.ini");
  }
  return std::nullopt;
}

fs::path MakeTemporaryDirectoryPath(std::string_view prefix) {
  const auto ticks =
      std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
  return fs::temp_directory_path() / (std::string(prefix) + "_" + ticks);
}

}  // namespace

core::Result<UnpackManifest> UnpackMapArchive(const fs::path& archive_path,
                                              const fs::path& output_dir) {
  if (!core::FilesystemUtils::Exists(archive_path)) {
    return std::unexpected(core::Error::FileNotFound(
        "Archive not found: " + archive_path.string()));
  }
  if (!core::FilesystemUtils::IsFile(archive_path)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input is not a packed archive: " + archive_path.string()));
  }

  auto create_result = core::FilesystemUtils::CreateDirectories(output_dir);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create output directory: " + output_dir.string() + ": " +
        create_result.error()));
  }

  MpqArchive archive;
  W3X_RETURN_IF_ERROR(archive.Open(archive_path));
  W3X_ASSIGN_OR_RETURN(auto listed_files, archive.ListFiles());

  std::unordered_set<std::string> enumerated_names;
  for (std::string& file : listed_files) {
    file = NormalizeArchivePath(file);
    enumerated_names.insert(file);
  }

  UnpackManifest manifest;
  manifest.archive_file = archive_path.filename().string();

  std::unordered_set<std::string> written_archive_paths;
  std::unordered_map<std::uint64_t, std::size_t> written_hashes;

  const auto add_written_entry =
      [&](ManifestEntry entry, const std::vector<std::uint8_t>& data)
      -> core::Result<void> {
    W3X_RETURN_IF_ERROR(WriteEntryFile(output_dir, entry, data));
    written_archive_paths.insert(entry.archive_path);
    written_hashes.emplace(entry.content_hash, manifest.written_files.size());
    manifest.written_files.push_back(std::move(entry));
    return {};
  };

  for (const std::string& known_path : KnownArchivePaths()) {
    if (!archive.FileExists(known_path)) {
      continue;
    }

    W3X_ASSIGN_OR_RETURN(auto data, archive.ExtractFile(known_path));
    ManifestEntry entry;
    entry.archive_path = NormalizeArchivePath(known_path);
    entry.disk_path = entry.archive_path;
    entry.source_name = entry.archive_path;
    entry.status = enumerated_names.contains(entry.archive_path)
                       ? ArchiveNameStatus::kOriginal
                       : ArchiveNameStatus::kRecovered;
    entry.content_hash = HashBytes(data);
    entry.size = static_cast<std::uint64_t>(data.size());
    W3X_RETURN_IF_ERROR(add_written_entry(std::move(entry), data));
  }

  std::sort(listed_files.begin(), listed_files.end());
  for (const std::string& listed_name : listed_files) {
    const std::string normalized_name = NormalizeArchivePath(listed_name);
    if (written_archive_paths.contains(normalized_name)) {
      continue;
    }
    if (IsInternalFile(normalized_name)) {
      manifest.internal_files.push_back(normalized_name);
      continue;
    }

    W3X_ASSIGN_OR_RETURN(auto data, archive.ExtractFile(normalized_name));
    if (IsAnonymousEntryName(normalized_name) && data.empty()) {
      ManifestEntry duplicate;
      duplicate.archive_path = normalized_name;
      duplicate.source_name = normalized_name;
      duplicate.status = ArchiveNameStatus::kAnonymous;
      duplicate.content_hash = HashBytes(data);
      duplicate.size = 0;
      manifest.skipped_duplicates.push_back(std::move(duplicate));
      continue;
    }
    const std::uint64_t content_hash = HashBytes(data);
    if (const auto recovered_path =
            TryRecoverAnonymousTextPath(normalized_name, data);
        recovered_path.has_value() &&
        !written_archive_paths.contains(*recovered_path)) {
      ManifestEntry entry;
      entry.archive_path = *recovered_path;
      entry.disk_path = *recovered_path;
      entry.source_name = normalized_name;
      entry.status = ArchiveNameStatus::kRecovered;
      entry.content_hash = content_hash;
      entry.size = static_cast<std::uint64_t>(data.size());
      W3X_RETURN_IF_ERROR(add_written_entry(std::move(entry), data));
      continue;
    }

    if (const auto it = written_hashes.find(content_hash);
        it != written_hashes.end()) {
      ManifestEntry duplicate;
      duplicate.archive_path = normalized_name;
      duplicate.source_name = normalized_name;
      duplicate.status = IsAnonymousEntryName(normalized_name)
                             ? ArchiveNameStatus::kAnonymous
                             : ArchiveNameStatus::kOriginal;
      duplicate.content_hash = content_hash;
      duplicate.size = static_cast<std::uint64_t>(data.size());
      manifest.skipped_duplicates.push_back(std::move(duplicate));

      ManifestEntry& written = manifest.written_files[it->second];
      if (written.status == ArchiveNameStatus::kRecovered &&
          written.source_name == written.archive_path &&
          IsAnonymousEntryName(normalized_name)) {
        written.source_name = normalized_name;
      }
      continue;
    }

    ManifestEntry entry;
    entry.archive_path = normalized_name;
    entry.source_name = normalized_name;
    entry.status = IsAnonymousEntryName(normalized_name)
                       ? ArchiveNameStatus::kAnonymous
                       : ArchiveNameStatus::kOriginal;
    entry.disk_path = entry.status == ArchiveNameStatus::kAnonymous
                          ? NormalizeArchivePath("anonymous/" + normalized_name)
                          : normalized_name;
    entry.content_hash = content_hash;
    entry.size = static_cast<std::uint64_t>(data.size());
    W3X_RETURN_IF_ERROR(add_written_entry(std::move(entry), data));
  }

  W3X_RETURN_IF_ERROR(WriteManifestFile(output_dir, manifest));
  return manifest;
}

namespace {

core::Result<std::size_t> PackFlatMapDirectory(const fs::path& input_dir,
                                               const fs::path& archive_path) {
  if (!core::FilesystemUtils::Exists(input_dir)) {
    return std::unexpected(core::Error::FileNotFound(
        "Input directory not found: " + input_dir.string()));
  }
  if (!core::FilesystemUtils::IsDirectory(input_dir)) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input is not a directory: " + input_dir.string()));
  }

  auto absolute_result = core::FilesystemUtils::GetAbsolutePath(input_dir);
  if (!absolute_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to resolve input directory: " + absolute_result.error()));
  }
  const fs::path resolved_input_dir = absolute_result.value();

  W3X_ASSIGN_OR_RETURN(auto synthetic_files,
                       GenerateSyntheticMapFiles(resolved_input_dir));
  std::unordered_set<std::string> synthetic_relative_paths;
  for (const auto& generated : synthetic_files) {
    synthetic_relative_paths.insert(
        NormalizeArchivePath(generated.relative_path));
  }
  W3X_ASSIGN_OR_RETURN(auto manifest_mappings,
                       LoadManifestMappings(resolved_input_dir));
  W3X_ASSIGN_OR_RETURN(auto files,
                       core::FilesystemUtils::ListDirectoryRecursive(
                           resolved_input_dir)
                           .transform_error([](const std::string& error) {
                             return core::Error::IOError(
                                 "Failed to enumerate input directory: " +
                                 error);
                           }));

  std::vector<fs::path> regular_files;
  regular_files.reserve(files.size());
  std::unordered_set<std::string> regular_relative_paths;
  const std::string manifest_name(kUnpackManifestFileName);
  for (const auto& file : files) {
    if (!core::FilesystemUtils::IsFile(file)) {
      continue;
    }
    const auto relative = NormalizeArchivePath(
        fs::relative(file, resolved_input_dir).generic_string());
    if (relative == manifest_name) {
      continue;
    }
    if (synthetic_relative_paths.contains(relative)) {
      continue;
    }
    regular_files.push_back(file);
    regular_relative_paths.insert(relative);
  }

  if (regular_files.empty()) {
    return std::unexpected(core::Error::InvalidFormat(
        "Input directory does not contain any files to pack"));
  }

  const fs::path archive_parent = archive_path.parent_path();
  if (!archive_parent.empty()) {
    auto create_result = core::FilesystemUtils::CreateDirectories(archive_parent);
    if (!create_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to create output archive directory: " +
          archive_parent.string() + ": " + create_result.error()));
    }
  }

  if (core::FilesystemUtils::Exists(archive_path)) {
    auto remove_result = core::FilesystemUtils::Remove(archive_path);
    if (!remove_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to remove existing archive: " + archive_path.string() +
          ": " + remove_result.error()));
    }
  }

  HANDLE archive_handle = nullptr;
  const DWORD create_flags =
      MPQ_CREATE_ARCHIVE_V1 | MPQ_CREATE_LISTFILE | MPQ_CREATE_ATTRIBUTES;
  const DWORD max_file_count = static_cast<DWORD>(
      std::max<std::size_t>(64,
                            (regular_files.size() + synthetic_files.size()) *
                                2));
  if (!SFileCreateArchive(archive_path.c_str(), create_flags, max_file_count,
                          &archive_handle)) {
    return std::unexpected(
        core::Error::IOError(FormatStormError("Creating archive", archive_path)));
  }

  std::size_t packed_files = 0;
  for (const auto& file : regular_files) {
    const std::string relative = NormalizeArchivePath(
        fs::relative(file, resolved_input_dir).generic_string());
    std::string archive_name = relative;
    if (const auto it = manifest_mappings.find(relative);
        it != manifest_mappings.end()) {
      archive_name = it->second;
    } else if (relative.starts_with("anonymous/")) {
      archive_name = fs::path(relative).filename().string();
    }

    const std::string mpq_path = ToArchiveStoragePath(archive_name);
    W3X_ASSIGN_OR_RETURN(
        auto file_size,
        core::FilesystemUtils::GetFileSize(file).transform_error(
            [&file](const std::string& error) {
              return core::Error::IOError("Failed to stat '" + file.string() +
                                          "': " + error);
            }));
    if (file_size == 0 && relative.starts_with("anonymous/")) {
      continue;
    }
    if (file_size == 0) {
      HANDLE mpq_file = nullptr;
      if (!SFileCreateFile(archive_handle, mpq_path.c_str(), 0, 0, 0, 0,
                           &mpq_file)) {
        const auto error = FormatStormErrorForArchiveName("Creating file",
                                                          mpq_path);
        SFileCloseArchive(archive_handle);
        return std::unexpected(core::Error::IOError(error));
      }
      if (!SFileFinishFile(mpq_file)) {
        const auto error = FormatStormErrorForArchiveName(
            "Finishing empty file", mpq_path);
        SFileCloseArchive(archive_handle);
        return std::unexpected(core::Error::IOError(error));
      }
    } else if (!SFileAddFileEx(archive_handle, file.c_str(), mpq_path.c_str(),
                               MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING,
                               MPQ_COMPRESSION_ZLIB,
                               MPQ_COMPRESSION_NEXT_SAME)) {
      const auto error = FormatStormErrorForArchiveName("Adding file",
                                                        mpq_path);
      SFileCloseArchive(archive_handle);
      return std::unexpected(core::Error::IOError(error));
    }
    ++packed_files;
  }

  for (const auto& generated : synthetic_files) {
    const std::string relative = NormalizeArchivePath(generated.relative_path);
    if (generated.content.empty()) {
      continue;
    }

    const std::string mpq_path = ToArchiveStoragePath(relative);
    HANDLE mpq_file = nullptr;
    if (!SFileCreateFile(archive_handle, mpq_path.c_str(), 0,
                         static_cast<DWORD>(generated.content.size()), 0,
                         MPQ_FILE_COMPRESS | MPQ_FILE_REPLACEEXISTING,
                         &mpq_file)) {
      const auto error = FormatStormErrorForArchiveName("Creating file",
                                                        mpq_path);
      SFileCloseArchive(archive_handle);
      return std::unexpected(core::Error::IOError(error));
    }
    if (!SFileWriteFile(mpq_file, generated.content.data(),
                        static_cast<DWORD>(generated.content.size()),
                        MPQ_COMPRESSION_ZLIB)) {
      const auto error = FormatStormErrorForArchiveName("Writing file",
                                                        mpq_path);
      SFileCloseFile(mpq_file);
      SFileCloseArchive(archive_handle);
      return std::unexpected(core::Error::IOError(error));
    }
    if (!SFileFinishFile(mpq_file)) {
      const auto error = FormatStormErrorForArchiveName("Finishing file",
                                                        mpq_path);
      SFileCloseArchive(archive_handle);
      return std::unexpected(core::Error::IOError(error));
    }
    ++packed_files;
  }

  if (!SFileFlushArchive(archive_handle)) {
    const auto error = FormatStormError("Flushing archive", archive_path);
    SFileCloseArchive(archive_handle);
    return std::unexpected(core::Error::IOError(error));
  }
  if (!SFileCloseArchive(archive_handle)) {
    return std::unexpected(
        core::Error::IOError(FormatStormError("Closing archive", archive_path)));
  }
  return packed_files;
}

}  // namespace

core::Result<std::size_t> PackMapDirectory(const fs::path& input_dir,
                                           const fs::path& archive_path) {
  if (!IsWorkspaceLayout(input_dir)) {
    return PackFlatMapDirectory(input_dir, archive_path);
  }

  const fs::path staging_dir = MakeTemporaryDirectoryPath("w3x_toolkit_pack");
  auto create_result = core::FilesystemUtils::CreateDirectories(staging_dir);
  if (!create_result.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to create staging directory: " + staging_dir.string() + ": " +
        create_result.error()));
  }

  auto cleanup = [&staging_dir]() {
    std::error_code ec;
    fs::remove_all(staging_dir, ec);
  };

  auto convert_result = ConvertWorkspaceToFlatDirectory(input_dir, staging_dir);
  if (!convert_result.has_value()) {
    cleanup();
    return std::unexpected(std::move(convert_result.error()));
  }

  auto pack_result = PackFlatMapDirectory(staging_dir, archive_path);
  cleanup();
  if (!pack_result.has_value()) {
    return std::unexpected(std::move(pack_result.error()));
  }
  return pack_result;
}

}  // namespace w3x_toolkit::parser::w3x
