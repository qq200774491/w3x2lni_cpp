#include "parser/w3x/w3x_parser.h"

#include <StormLib.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>
#include <utility>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

namespace fs = std::filesystem;

namespace {

std::string NormalizeArchivePath(std::string path) {
  std::replace(path.begin(), path.end(), '\\', '/');
  while (path.starts_with("./")) {
    path.erase(0, 2);
  }
  return path;
}

std::string ToStormPath(const std::string& path) {
  std::string storm_path = NormalizeArchivePath(path);
  std::replace(storm_path.begin(), storm_path.end(), '/', '\\');
  return storm_path;
}

core::Error StormIoError(const std::string& action, const std::string& target) {
  return core::Error::IOError(action + " '" + target +
                              "' failed with Win32 error " +
                              std::to_string(GetLastError()));
}

}  // namespace

// ============================================================================
// DirectoryArchive
// ============================================================================

DirectoryArchive::~DirectoryArchive() { Close(); }

core::Result<void> DirectoryArchive::Open(const fs::path& path) {
  if (!fs::exists(path)) {
    return std::unexpected(
        core::Error::FileNotFound("Directory not found: " + path.string()));
  }
  if (!fs::is_directory(path)) {
    return std::unexpected(
        core::Error::InvalidFormat("Path is not a directory: " + path.string()));
  }
  root_path_ = path;
  is_open_ = true;
  return {};
}

void DirectoryArchive::Close() {
  is_open_ = false;
  root_path_.clear();
}

bool DirectoryArchive::IsOpen() const { return is_open_; }

core::Result<std::vector<std::string>> DirectoryArchive::ListFiles() const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("Archive is not open"));
  }
  std::vector<std::string> files;
  std::error_code ec;
  for (const auto& entry : fs::recursive_directory_iterator(root_path_, ec)) {
    if (ec) {
      return std::unexpected(
          core::Error::IOError("Error iterating directory: " + ec.message()));
    }
    if (entry.is_regular_file()) {
      auto relative = fs::relative(entry.path(), root_path_, ec);
      if (!ec) {
        files.push_back(relative.generic_string());
      }
    }
  }
  return files;
}

bool DirectoryArchive::FileExists(const std::string& filename) const {
  if (!is_open_) {
    return false;
  }
  return fs::exists(root_path_ / fs::path(NormalizeArchivePath(filename)));
}

core::Result<std::vector<std::uint8_t>> DirectoryArchive::ExtractFile(
    const std::string& filename) const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("Archive is not open"));
  }

  const fs::path file_path = root_path_ / fs::path(NormalizeArchivePath(filename));
  if (!fs::exists(file_path)) {
    return std::unexpected(
        core::Error::FileNotFound("File not found in archive: " + filename));
  }

  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return std::unexpected(
        core::Error::IOError("Failed to open file: " + file_path.string()));
  }

  const auto file_size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<std::uint8_t> buffer(static_cast<std::size_t>(file_size));
  if (file_size > 0) {
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(file_size));
  }
  return buffer;
}

const fs::path& DirectoryArchive::GetPath() const { return root_path_; }

// ============================================================================
// MpqArchive
// ============================================================================

MpqArchive::~MpqArchive() { Close(); }

core::Result<void> MpqArchive::Open(const fs::path& path) {
  if (!fs::exists(path)) {
    return std::unexpected(
        core::Error::FileNotFound("Archive not found: " + path.string()));
  }
  if (!fs::is_regular_file(path)) {
    return std::unexpected(
        core::Error::InvalidFormat("Path is not a file: " + path.string()));
  }

  HANDLE archive = nullptr;
  if (!SFileOpenArchive(path.c_str(), 0, MPQ_OPEN_READ_ONLY, &archive)) {
    return std::unexpected(
        StormIoError("Opening archive", path.string()));
  }

  archive_path_ = path;
  archive_handle_ = archive;
  is_open_ = true;
  file_list_loaded_ = false;
  file_list_.clear();
  file_lookup_.clear();
  return {};
}

void MpqArchive::Close() {
  if (archive_handle_ != nullptr) {
    SFileCloseArchive(static_cast<HANDLE>(archive_handle_));
  }
  archive_handle_ = nullptr;
  archive_path_.clear();
  is_open_ = false;
  file_list_loaded_ = false;
  file_list_.clear();
  file_lookup_.clear();
}

bool MpqArchive::IsOpen() const { return is_open_; }

core::Result<std::vector<std::string>> MpqArchive::ListFiles() const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("MPQ archive is not open"));
  }
  if (file_list_loaded_) {
    return file_list_;
  }

  std::vector<std::string> files;
  std::unordered_set<std::string> dedupe;
  SFILE_FIND_DATA find_data{};
  HANDLE find = SFileFindFirstFile(static_cast<HANDLE>(archive_handle_), "*",
                                   &find_data, nullptr);
  if (find != nullptr) {
    do {
      const std::string normalized = NormalizeArchivePath(find_data.cFileName);
      if (dedupe.insert(normalized).second) {
        files.push_back(normalized);
      }
    } while (SFileFindNextFile(find, &find_data));
    SFileFindClose(find);
  }

  std::sort(files.begin(), files.end());
  file_list_ = files;
  file_lookup_.clear();
  for (const auto& file : file_list_) {
    file_lookup_.insert(file);
  }
  file_list_loaded_ = true;
  return file_list_;
}

bool MpqArchive::FileExists(const std::string& filename) const {
  if (!is_open_) {
    return false;
  }
  const std::string normalized = NormalizeArchivePath(filename);
  if (file_list_loaded_ && file_lookup_.contains(normalized)) {
    return true;
  }
  const std::string storm_path = ToStormPath(normalized);
  return SFileHasFile(static_cast<HANDLE>(archive_handle_), storm_path.c_str());
}

core::Result<std::vector<std::uint8_t>> MpqArchive::ExtractFile(
    const std::string& filename) const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("MPQ archive is not open"));
  }

  const std::string normalized = NormalizeArchivePath(filename);
  const std::string storm_path = ToStormPath(normalized);

  HANDLE file = nullptr;
  if (!SFileOpenFileEx(static_cast<HANDLE>(archive_handle_), storm_path.c_str(),
                       SFILE_OPEN_FROM_MPQ, &file)) {
    return std::unexpected(StormIoError("Opening archive member", normalized));
  }

  DWORD high_size = 0;
  const DWORD low_size = SFileGetFileSize(file, &high_size);
  if (low_size == SFILE_INVALID_SIZE && GetLastError() != ERROR_SUCCESS) {
    SFileCloseFile(file);
    return std::unexpected(
        StormIoError("Querying archive member size", normalized));
  }

  const std::uint64_t full_size =
      (static_cast<std::uint64_t>(high_size) << 32) | low_size;
  if (full_size >
      static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
    SFileCloseFile(file);
    return std::unexpected(core::Error::IOError(
        "Archive member is too large to fit in memory: " + normalized));
  }

  std::vector<std::uint8_t> buffer(static_cast<std::size_t>(full_size));
  DWORD bytes_read = 0;
  if (!buffer.empty() &&
      !SFileReadFile(file, buffer.data(), low_size, &bytes_read, nullptr)) {
    SFileCloseFile(file);
    return std::unexpected(StormIoError("Reading archive member", normalized));
  }
  SFileCloseFile(file);

  buffer.resize(bytes_read);
  return buffer;
}

const fs::path& MpqArchive::GetPath() const { return archive_path_; }

// ============================================================================
// Factory
// ============================================================================

std::unique_ptr<W3xArchive> OpenArchive(const fs::path& path) {
  if (fs::is_directory(path)) {
    auto archive = std::make_unique<DirectoryArchive>();
    auto result = archive->Open(path);
    if (result.has_value()) {
      return archive;
    }
    return nullptr;
  }

  auto archive = std::make_unique<MpqArchive>();
  auto result = archive->Open(path);
  if (result.has_value()) {
    return archive;
  }
  return nullptr;
}

}  // namespace w3x_toolkit::parser::w3x
