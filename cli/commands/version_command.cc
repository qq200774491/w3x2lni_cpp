// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/version_command.h"

#include "cli/cli_app.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

std::string VersionCommand::Name() const { return "version"; }

std::string VersionCommand::Description() const {
  return "Show version information";
}

std::string VersionCommand::Usage() const { return "version"; }

core::Result<void> VersionCommand::Execute(
    const std::vector<std::string>& args) {
  if (!args.empty()) {
    return std::unexpected(core::Error::InvalidFormat(
        "This command does not accept arguments.\nUsage: " + Usage()));
  }
  CliApp::PrintVersion();
  return {};
}

}  // namespace w3x_toolkit::cli
