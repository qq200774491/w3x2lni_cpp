// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/log_command.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/config/runtime_paths.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::cli {

std::string LogCommand::Name() const { return "log"; }

std::string LogCommand::Description() const {
  return "Print the toolkit log file";
}

std::string LogCommand::Usage() const { return "log [--path]"; }

core::Result<void> LogCommand::Execute(const std::vector<std::string>& args) {
  bool print_path_only = false;
  if (args.size() > 1) {
    return std::unexpected(core::Error::InvalidFormat(
        "Too many arguments.\nUsage: " + Usage()));
  }
  if (!args.empty()) {
    if (args[0] != "--path") {
      return std::unexpected(core::Error::InvalidFormat(
          "Unknown option: " + args[0] + "\nUsage: " + Usage()));
    }
    print_path_only = true;
  }

  const std::filesystem::path log_path = core::RuntimePaths::ResolveLogPath();
  if (print_path_only) {
    std::cout << log_path.string() << std::endl;
    return {};
  }
  if (!std::filesystem::exists(log_path)) {
    return std::unexpected(core::Error::FileNotFound(
        "Log file not found: " + log_path.string()));
  }

  auto content = core::FilesystemUtils::ReadTextFile(log_path);
  if (!content.has_value()) {
    return std::unexpected(core::Error::IOError(
        "Failed to read log file '" + log_path.string() + "': " +
        content.error()));
  }
  std::cout << content.value();
  if (!content->empty() && content->back() != '\n') {
    std::cout << std::endl;
  }
  return {};
}

}  // namespace w3x_toolkit::cli
