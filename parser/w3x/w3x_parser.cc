#include "parser/w3x/w3x_parser.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

namespace fs = std::filesystem;

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
  for (const auto& entry :
       fs::recursive_directory_iterator(root_path_, ec)) {
    if (ec) {
      return std::unexpected(
          core::Error::IOError("Error iterating directory: " + ec.message()));
    }
    if (entry.is_regular_file()) {
      auto relative = fs::relative(entry.path(), root_path_, ec);
      if (!ec) {
        // Normalize to forward slashes for consistency.
        std::string rel_str = relative.generic_string();
        files.push_back(std::move(rel_str));
      }
    }
  }
  return files;
}

bool DirectoryArchive::FileExists(const std::string& filename) const {
  if (!is_open_) return false;
  return fs::exists(root_path_ / filename);
}

core::Result<std::vector<uint8_t>> DirectoryArchive::ExtractFile(
    const std::string& filename) const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("Archive is not open"));
  }
  auto file_path = root_path_ / filename;
  if (!fs::exists(file_path)) {
    return std::unexpected(
        core::Error::FileNotFound("File not found in archive: " + filename));
  }
  std::ifstream file(file_path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    return std::unexpected(
        core::Error::IOError("Failed to open file: " + file_path.string()));
  }
  auto file_size = file.tellg();
  file.seekg(0, std::ios::beg);
  std::vector<uint8_t> buffer(static_cast<size_t>(file_size));
  if (file_size > 0) {
    file.read(reinterpret_cast<char*>(buffer.data()),
              static_cast<std::streamsize>(file_size));
  }
  return buffer;
}

const fs::path& DirectoryArchive::GetPath() const { return root_path_; }

// ============================================================================
// MpqArchive (placeholder)
// ============================================================================

MpqArchive::~MpqArchive() { Close(); }

core::Result<void> MpqArchive::Open(const fs::path& path) {
  if (!fs::exists(path)) {
    return std::unexpected(
        core::Error::FileNotFound("Archive not found: " + path.string()));
  }
  // TODO: Implement MPQ opening via StormLib.
  // SFileOpenArchive(path.string().c_str(), 0, 0, &mpq_handle_);
  archive_path_ = path;
  is_open_ = false;
  return std::unexpected(core::Error::Unknown(
      "MPQ archive support requires StormLib (not yet linked)"));
}

void MpqArchive::Close() {
  // TODO: SFileCloseArchive(mpq_handle_);
  is_open_ = false;
  archive_path_.clear();
}

bool MpqArchive::IsOpen() const { return is_open_; }

core::Result<std::vector<std::string>> MpqArchive::ListFiles() const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("MPQ archive is not open"));
  }
  // TODO: Use SFileFindFirstFile / SFileFindNextFile.
  return std::vector<std::string>{};
}

bool MpqArchive::FileExists(const std::string& filename) const {
  if (!is_open_) return false;
  // TODO: SFileHasFile(mpq_handle_, filename.c_str());
  return false;
}

core::Result<std::vector<uint8_t>> MpqArchive::ExtractFile(
    const std::string& filename) const {
  if (!is_open_) {
    return std::unexpected(core::Error::IOError("MPQ archive is not open"));
  }
  // TODO: SFileOpenFileEx, SFileGetFileSize, SFileReadFile, SFileCloseFile.
  return std::unexpected(
      core::Error::Unknown("MPQ extraction not yet implemented"));
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
  // Assume it's an MPQ file.
  auto archive = std::make_unique<MpqArchive>();
  auto result = archive->Open(path);
  if (result.has_value()) {
    return archive;
  }
  return nullptr;
}

}  // namespace w3x_toolkit::parser::w3x
