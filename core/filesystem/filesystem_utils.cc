#include "core/filesystem/filesystem_utils.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <string>
#include <vector>

namespace w3x_toolkit::core {

namespace fs = std::filesystem;

// =============================================================================
// File read operations
// =============================================================================

std::expected<std::string, std::string> FilesystemUtils::ReadTextFile(
    const fs::path& file_path) {
  std::error_code ec;
  if (!fs::exists(file_path, ec)) {
    return std::unexpected("File does not exist: " + file_path.string());
  }
  if (!fs::is_regular_file(file_path, ec)) {
    return std::unexpected("Path is not a regular file: " +
                           file_path.string());
  }

  std::ifstream stream(file_path, std::ios::in);
  if (!stream.is_open()) {
    return std::unexpected("Failed to open file for reading: " +
                           file_path.string());
  }

  std::ostringstream buffer;
  buffer << stream.rdbuf();
  if (stream.bad()) {
    return std::unexpected("I/O error while reading file: " +
                           file_path.string());
  }

  return buffer.str();
}

std::expected<std::vector<uint8_t>, std::string>
FilesystemUtils::ReadBinaryFile(const fs::path& file_path) {
  std::error_code ec;
  if (!fs::exists(file_path, ec)) {
    return std::unexpected("File does not exist: " + file_path.string());
  }
  if (!fs::is_regular_file(file_path, ec)) {
    return std::unexpected("Path is not a regular file: " +
                           file_path.string());
  }

  const auto file_size = fs::file_size(file_path, ec);
  if (ec) {
    return std::unexpected("Failed to get file size: " + ec.message());
  }

  std::ifstream stream(file_path, std::ios::in | std::ios::binary);
  if (!stream.is_open()) {
    return std::unexpected("Failed to open file for reading: " +
                           file_path.string());
  }

  std::vector<uint8_t> data(static_cast<size_t>(file_size));
  stream.read(reinterpret_cast<char*>(data.data()),
              static_cast<std::streamsize>(file_size));
  if (stream.bad()) {
    return std::unexpected("I/O error while reading binary file: " +
                           file_path.string());
  }

  return data;
}

// =============================================================================
// File write operations
// =============================================================================

// Ensures parent directories exist before writing.
static std::expected<void, std::string> EnsureParentDirectoryExists(
    const fs::path& file_path) {
  const auto parent = file_path.parent_path();
  if (parent.empty()) {
    return {};
  }
  std::error_code ec;
  if (!fs::exists(parent, ec)) {
    fs::create_directories(parent, ec);
    if (ec) {
      return std::unexpected("Failed to create parent directories: " +
                             ec.message());
    }
  }
  return {};
}

std::expected<void, std::string> FilesystemUtils::WriteTextFile(
    const fs::path& file_path, const std::string& content) {
  auto dir_result = EnsureParentDirectoryExists(file_path);
  if (!dir_result) {
    return dir_result;
  }

  std::ofstream stream(file_path, std::ios::out | std::ios::trunc);
  if (!stream.is_open()) {
    return std::unexpected("Failed to open file for writing: " +
                           file_path.string());
  }

  stream << content;
  if (stream.bad()) {
    return std::unexpected("I/O error while writing file: " +
                           file_path.string());
  }

  return {};
}

std::expected<void, std::string> FilesystemUtils::WriteBinaryFile(
    const fs::path& file_path, const std::vector<uint8_t>& content) {
  auto dir_result = EnsureParentDirectoryExists(file_path);
  if (!dir_result) {
    return dir_result;
  }

  std::ofstream stream(file_path,
                       std::ios::out | std::ios::binary | std::ios::trunc);
  if (!stream.is_open()) {
    return std::unexpected("Failed to open file for writing: " +
                           file_path.string());
  }

  stream.write(reinterpret_cast<const char*>(content.data()),
               static_cast<std::streamsize>(content.size()));
  if (stream.bad()) {
    return std::unexpected("I/O error while writing binary file: " +
                           file_path.string());
  }

  return {};
}

std::expected<void, std::string> FilesystemUtils::AppendTextFile(
    const fs::path& file_path, const std::string& content) {
  auto dir_result = EnsureParentDirectoryExists(file_path);
  if (!dir_result) {
    return dir_result;
  }

  std::ofstream stream(file_path, std::ios::out | std::ios::app);
  if (!stream.is_open()) {
    return std::unexpected("Failed to open file for appending: " +
                           file_path.string());
  }

  stream << content;
  if (stream.bad()) {
    return std::unexpected("I/O error while appending to file: " +
                           file_path.string());
  }

  return {};
}

// =============================================================================
// Directory operations
// =============================================================================

std::expected<bool, std::string> FilesystemUtils::CreateDirectories(
    const fs::path& dir_path) {
  std::error_code ec;
  const bool created = fs::create_directories(dir_path, ec);
  if (ec) {
    return std::unexpected("Failed to create directories: " + ec.message());
  }
  return created;
}

std::expected<std::vector<fs::path>, std::string>
FilesystemUtils::ListDirectory(const fs::path& dir_path) {
  std::error_code ec;
  if (!fs::is_directory(dir_path, ec)) {
    return std::unexpected("Path is not a directory: " + dir_path.string());
  }

  std::vector<fs::path> entries;
  for (const auto& entry : fs::directory_iterator(dir_path, ec)) {
    if (ec) {
      return std::unexpected("Error iterating directory: " + ec.message());
    }
    entries.push_back(entry.path());
  }
  if (ec) {
    return std::unexpected("Error iterating directory: " + ec.message());
  }

  return entries;
}

std::expected<std::vector<fs::path>, std::string>
FilesystemUtils::ListDirectoryRecursive(const fs::path& dir_path) {
  std::error_code ec;
  if (!fs::is_directory(dir_path, ec)) {
    return std::unexpected("Path is not a directory: " + dir_path.string());
  }

  std::vector<fs::path> entries;
  for (const auto& entry : fs::recursive_directory_iterator(dir_path, ec)) {
    if (ec) {
      return std::unexpected("Error iterating directory recursively: " +
                             ec.message());
    }
    entries.push_back(entry.path());
  }
  if (ec) {
    return std::unexpected("Error iterating directory recursively: " +
                           ec.message());
  }

  return entries;
}

bool FilesystemUtils::Exists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}

bool FilesystemUtils::IsFile(const fs::path& path) {
  std::error_code ec;
  return fs::is_regular_file(path, ec);
}

bool FilesystemUtils::IsDirectory(const fs::path& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

std::expected<bool, std::string> FilesystemUtils::Remove(
    const fs::path& path) {
  std::error_code ec;
  const bool removed = fs::remove(path, ec);
  if (ec) {
    return std::unexpected("Failed to remove: " + ec.message());
  }
  return removed;
}

std::expected<std::uintmax_t, std::string> FilesystemUtils::RemoveAll(
    const fs::path& path) {
  std::error_code ec;
  const auto count = fs::remove_all(path, ec);
  if (ec) {
    return std::unexpected("Failed to remove recursively: " + ec.message());
  }
  return count;
}

std::expected<void, std::string> FilesystemUtils::CopyFile(
    const fs::path& source, const fs::path& destination) {
  std::error_code ec;
  fs::copy_file(source, destination,
                fs::copy_options::overwrite_existing, ec);
  if (ec) {
    return std::unexpected("Failed to copy file: " + ec.message());
  }
  return {};
}

std::expected<void, std::string> FilesystemUtils::Rename(
    const fs::path& old_path, const fs::path& new_path) {
  std::error_code ec;
  fs::rename(old_path, new_path, ec);
  if (ec) {
    return std::unexpected("Failed to rename: " + ec.message());
  }
  return {};
}

// =============================================================================
// Path manipulation utilities
// =============================================================================

std::string FilesystemUtils::GetExtension(const fs::path& path) {
  return path.extension().string();
}

std::string FilesystemUtils::GetStem(const fs::path& path) {
  return path.stem().string();
}

std::string FilesystemUtils::GetFilename(const fs::path& path) {
  return path.filename().string();
}

fs::path FilesystemUtils::GetParentPath(const fs::path& path) {
  return path.parent_path();
}

std::expected<fs::path, std::string> FilesystemUtils::GetAbsolutePath(
    const fs::path& path) {
  std::error_code ec;
  auto canonical = fs::canonical(path, ec);
  if (ec) {
    // Fall back to absolute() when the path does not yet exist on disk.
    auto absolute = fs::absolute(path, ec);
    if (ec) {
      return std::unexpected("Failed to resolve absolute path: " +
                             ec.message());
    }
    return absolute;
  }
  return canonical;
}

// =============================================================================
// File metadata helpers
// =============================================================================

std::expected<std::uintmax_t, std::string> FilesystemUtils::GetFileSize(
    const fs::path& path) {
  std::error_code ec;
  const auto size = fs::file_size(path, ec);
  if (ec) {
    return std::unexpected("Failed to get file size: " + ec.message());
  }
  return size;
}

std::expected<bool, std::string> FilesystemUtils::IsEmpty(
    const fs::path& path) {
  std::error_code ec;
  const bool empty = fs::is_empty(path, ec);
  if (ec) {
    return std::unexpected("Failed to check if path is empty: " +
                           ec.message());
  }
  return empty;
}

}  // namespace w3x_toolkit::core
