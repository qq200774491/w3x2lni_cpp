// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/obj_command.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands/map_input_utils.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"
#include "parser/w3x/map_archive_io.h"

namespace w3x_toolkit::cli {

namespace {

class TemporaryDirectory {
 public:
  explicit TemporaryDirectory(std::string_view prefix) {
    const auto suffix =
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    path_ = std::filesystem::temp_directory_path() /
            (std::string(prefix) + "_" + suffix);
    std::error_code ec;
    std::filesystem::create_directories(path_, ec);
  }

  ~TemporaryDirectory() {
    std::error_code ec;
    std::filesystem::remove_all(path_, ec);
  }

  const std::filesystem::path& path() const { return path_; }

 private:
  std::filesystem::path path_;
};

std::filesystem::path DefaultOutputPath(const std::filesystem::path& input_path) {
  if (core::FilesystemUtils::IsDirectory(input_path)) {
    return input_path.parent_path() / (input_path.filename().string() + "_obj.w3x");
  }
  return input_path.parent_path() / (input_path.stem().string() + "_obj.w3x");
}

}  // namespace

std::string ObjCommand::Name() const { return "obj"; }

std::string ObjCommand::Description() const {
  return "Convert a map to an Obj-style packed map";
}

std::string ObjCommand::Usage() const {
  return "obj <input_map_dir|input_map.w3x|input_map.w3m> [output_map.w3x|output_map.w3m]";
}

core::Result<void> ObjCommand::Execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing required arguments.\nUsage: " + Usage()));
  }
  if (args.size() > 2) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Too many arguments.\nUsage: " + Usage()));
  }

  W3X_ASSIGN_OR_RETURN(auto input_path, ResolveMapInputPath(args[0]));
  const std::filesystem::path output_path =
      args.size() >= 2 ? std::filesystem::path(args[1]) : DefaultOutputPath(input_path);

  auto& logger = core::Logger::Instance();
  logger.Info("Converting '{}' -> '{}' as Obj packed map", input_path.string(),
              output_path.string());

  if (core::FilesystemUtils::IsDirectory(input_path)) {
    W3X_ASSIGN_OR_RETURN(auto packed_files,
                         parser::w3x::PackMapDirectory(input_path, output_path));
    std::cout << "Obj map written to: " << output_path.string() << std::endl;
    std::cout << "Packed " << packed_files << " file(s)." << std::endl;
    return {};
  }

  TemporaryDirectory unpacked_dir("w3x_toolkit_obj");
  W3X_RETURN_IF_ERROR(
      parser::w3x::UnpackMapArchive(input_path, unpacked_dir.path())
          .transform([](const parser::w3x::UnpackManifest&) {})
          .transform_error([&input_path](const core::Error& error) {
            return core::Error::ConvertError(
                "Failed to unpack packed map input '" + input_path.string() +
                "': " + error.message());
          }));
  W3X_ASSIGN_OR_RETURN(
      auto packed_files,
      parser::w3x::PackMapDirectory(unpacked_dir.path(), output_path));
  std::cout << "Obj map written to: " << output_path.string() << std::endl;
  std::cout << "Packed " << packed_files << " file(s)." << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
