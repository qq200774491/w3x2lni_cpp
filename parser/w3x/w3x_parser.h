#ifndef W3X_TOOLKIT_PARSER_W3X_W3X_PARSER_H_
#define W3X_TOOLKIT_PARSER_W3X_W3X_PARSER_H_

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::parser::w3x {

// Abstract interface for reading a Warcraft III map archive.
// Concrete implementations handle MPQ archives (via StormLib) or unpacked
// directory-based maps.
class W3xArchive {
 public:
  virtual ~W3xArchive() = default;

  // Opens the archive at the given path.
  virtual core::Result<void> Open(const std::filesystem::path& path) = 0;

  // Closes the archive and releases resources.
  virtual void Close() = 0;

  // Returns true if the archive is currently open.
  virtual bool IsOpen() const = 0;

  // Lists all files contained in the archive.
  virtual core::Result<std::vector<std::string>> ListFiles() const = 0;

  // Returns true if the named file exists within the archive.
  virtual bool FileExists(const std::string& filename) const = 0;

  // Extracts the contents of the named file.
  virtual core::Result<std::vector<uint8_t>> ExtractFile(
      const std::string& filename) const = 0;

  // Returns the path this archive was opened from.
  virtual const std::filesystem::path& GetPath() const = 0;
};

// Implementation that treats an unpacked directory as a map archive.
// Each file in the directory tree is accessible by its relative path.
class DirectoryArchive final : public W3xArchive {
 public:
  DirectoryArchive() = default;
  ~DirectoryArchive() override;

  core::Result<void> Open(const std::filesystem::path& path) override;
  void Close() override;
  bool IsOpen() const override;
  core::Result<std::vector<std::string>> ListFiles() const override;
  bool FileExists(const std::string& filename) const override;
  core::Result<std::vector<uint8_t>> ExtractFile(
      const std::string& filename) const override;
  const std::filesystem::path& GetPath() const override;

 private:
  std::filesystem::path root_path_;
  bool is_open_ = false;
};

// Placeholder for future MPQ-based archive reader (requires StormLib).
class MpqArchive final : public W3xArchive {
 public:
  MpqArchive() = default;
  ~MpqArchive() override;

  core::Result<void> Open(const std::filesystem::path& path) override;
  void Close() override;
  bool IsOpen() const override;
  core::Result<std::vector<std::string>> ListFiles() const override;
  bool FileExists(const std::string& filename) const override;
  core::Result<std::vector<uint8_t>> ExtractFile(
      const std::string& filename) const override;
  const std::filesystem::path& GetPath() const override;

 private:
  std::filesystem::path archive_path_;
  bool is_open_ = false;
  // Future: HANDLE mpq_handle_ = nullptr;
};

// Factory: opens either a directory archive or an MPQ archive depending on
// whether the path is a directory or a file.
std::unique_ptr<W3xArchive> OpenArchive(const std::filesystem::path& path);

}  // namespace w3x_toolkit::parser::w3x

#endif  // W3X_TOOLKIT_PARSER_W3X_W3X_PARSER_H_
