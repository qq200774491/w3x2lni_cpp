#ifndef W3X_TOOLKIT_CORE_FILESYSTEM_FILESYSTEM_UTILS_H_
#define W3X_TOOLKIT_CORE_FILESYSTEM_FILESYSTEM_UTILS_H_

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace w3x_toolkit::core {

// Utility class providing static methods for common filesystem operations.
// All methods return std::expected<T, std::string> to propagate errors without
// exceptions.
class FilesystemUtils {
 public:
  FilesystemUtils() = delete;
  ~FilesystemUtils() = delete;
  FilesystemUtils(const FilesystemUtils&) = delete;
  FilesystemUtils& operator=(const FilesystemUtils&) = delete;

  // ---------------------------------------------------------------------------
  // File read operations
  // ---------------------------------------------------------------------------

  // Reads the entire contents of a text file and returns it as a string.
  static std::expected<std::string, std::string> ReadTextFile(
      const std::filesystem::path& file_path);

  // Reads the entire contents of a binary file and returns it as a byte vector.
  static std::expected<std::vector<uint8_t>, std::string> ReadBinaryFile(
      const std::filesystem::path& file_path);

  // ---------------------------------------------------------------------------
  // File write operations
  // ---------------------------------------------------------------------------

  // Writes a string to a text file, creating or overwriting as needed.
  // Parent directories are created automatically.
  static std::expected<void, std::string> WriteTextFile(
      const std::filesystem::path& file_path, const std::string& content);

  // Writes a byte vector to a binary file, creating or overwriting as needed.
  // Parent directories are created automatically.
  static std::expected<void, std::string> WriteBinaryFile(
      const std::filesystem::path& file_path,
      const std::vector<uint8_t>& content);

  // Appends a string to an existing (or new) text file.
  static std::expected<void, std::string> AppendTextFile(
      const std::filesystem::path& file_path, const std::string& content);

  // ---------------------------------------------------------------------------
  // Directory operations
  // ---------------------------------------------------------------------------

  // Creates a directory and all necessary parent directories.
  static std::expected<bool, std::string> CreateDirectories(
      const std::filesystem::path& dir_path);

  // Lists the immediate entries (files and directories) inside a directory.
  static std::expected<std::vector<std::filesystem::path>, std::string>
  ListDirectory(const std::filesystem::path& dir_path);

  // Lists entries inside a directory recursively.
  static std::expected<std::vector<std::filesystem::path>, std::string>
  ListDirectoryRecursive(const std::filesystem::path& dir_path);

  // Returns true if the path exists on disk.
  static bool Exists(const std::filesystem::path& path);

  // Returns true if the path refers to a regular file.
  static bool IsFile(const std::filesystem::path& path);

  // Returns true if the path refers to a directory.
  static bool IsDirectory(const std::filesystem::path& path);

  // Removes a file or an empty directory.
  static std::expected<bool, std::string> Remove(
      const std::filesystem::path& path);

  // Removes a file or directory and all of its contents recursively.
  static std::expected<std::uintmax_t, std::string> RemoveAll(
      const std::filesystem::path& path);

  // Copies a file or directory. For directories, use CopyRecursive instead.
  static std::expected<void, std::string> CopyFile(
      const std::filesystem::path& source,
      const std::filesystem::path& destination);

  // Renames / moves a file or directory.
  static std::expected<void, std::string> Rename(
      const std::filesystem::path& old_path,
      const std::filesystem::path& new_path);

  // ---------------------------------------------------------------------------
  // Path manipulation utilities
  // ---------------------------------------------------------------------------

  // Returns the file extension (including the dot, e.g. ".txt").
  static std::string GetExtension(const std::filesystem::path& path);

  // Returns the file stem (filename without extension).
  static std::string GetStem(const std::filesystem::path& path);

  // Returns the filename component (stem + extension).
  static std::string GetFilename(const std::filesystem::path& path);

  // Returns the parent directory of the given path.
  static std::filesystem::path GetParentPath(
      const std::filesystem::path& path);

  // Returns the absolute, canonical form of a path.
  static std::expected<std::filesystem::path, std::string> GetAbsolutePath(
      const std::filesystem::path& path);

  // ---------------------------------------------------------------------------
  // File metadata helpers
  // ---------------------------------------------------------------------------

  // Returns the size of a file in bytes.
  static std::expected<std::uintmax_t, std::string> GetFileSize(
      const std::filesystem::path& path);

  // Returns true if the file is empty (size == 0) or the directory has no
  // entries.
  static std::expected<bool, std::string> IsEmpty(
      const std::filesystem::path& path);
};

}  // namespace w3x_toolkit::core

#endif  // W3X_TOOLKIT_CORE_FILESYSTEM_FILESYSTEM_UTILS_H_
