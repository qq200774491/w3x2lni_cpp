// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/template_command.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/config/config_manager.h"
#include "core/config/runtime_paths.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::cli {

namespace fs = std::filesystem;

namespace {

core::Result<core::ConfigManager> LoadConfig() {
  core::ConfigManager config;
  W3X_RETURN_IF_ERROR(
      config.LoadDefaults(core::RuntimePaths::ResolveDefaultConfigPath()));
  W3X_RETURN_IF_ERROR(
      config.LoadGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));
  return config;
}

core::Result<void> CopyDirectoryTree(const fs::path& source,
                                     const fs::path& destination) {
  W3X_ASSIGN_OR_RETURN(auto files,
                       core::FilesystemUtils::ListDirectoryRecursive(source)
                           .transform_error([&source](const std::string& error) {
                             return core::Error::IOError(
                                 "Failed to enumerate '" + source.string() +
                                 "': " + error);
                           }));

  for (const fs::path& file : files) {
    if (!core::FilesystemUtils::IsFile(file)) {
      continue;
    }
    const fs::path relative = fs::relative(file, source);
    auto create_result =
        core::FilesystemUtils::CreateDirectories((destination / relative).parent_path());
    if (!create_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to create destination directory '" +
          (destination / relative).parent_path().string() + "': " +
          create_result.error()));
    }
    auto copy_result = core::FilesystemUtils::CopyFile(file, destination / relative);
    if (!copy_result.has_value()) {
      return std::unexpected(core::Error::IOError(
          "Failed to copy '" + file.string() + "': " + copy_result.error()));
    }
  }
  return {};
}

}  // namespace

std::string TemplateCommand::Name() const { return "template"; }

std::string TemplateCommand::Description() const {
  return "Export bundled Melee and Custom table templates";
}

std::string TemplateCommand::Usage() const {
  return "template [output_dir]";
}

core::Result<void> TemplateCommand::Execute(const std::vector<std::string>& args) {
  if (args.size() > 1) {
    return std::unexpected(core::Error::InvalidFormat(
        "Too many arguments.\nUsage: " + Usage()));
  }

  W3X_ASSIGN_OR_RETURN(auto config, LoadConfig());
  W3X_ASSIGN_OR_RETURN(auto data_root, core::RuntimePaths::ResolveDataRoot());

  const std::string data_version =
      config.GetString("global", "data", "zhCN-1.32.8");
  const fs::path prebuilt_root = data_root / data_version / "prebuilt";
  if (!fs::exists(prebuilt_root / "Melee") || !fs::exists(prebuilt_root / "Custom")) {
    return std::unexpected(core::Error::FileNotFound(
        "Prebuilt template directories not found under '" +
        prebuilt_root.string() + "'"));
  }

  const fs::path output_root =
      args.empty() ? (fs::current_path() / "template") : fs::path(args[0]);
  W3X_RETURN_IF_ERROR(CopyDirectoryTree(prebuilt_root / "Melee",
                                        output_root / "Melee"));
  W3X_RETURN_IF_ERROR(CopyDirectoryTree(prebuilt_root / "Custom",
                                        output_root / "Custom"));

  std::cout << "Templates written to: " << output_root.string() << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
