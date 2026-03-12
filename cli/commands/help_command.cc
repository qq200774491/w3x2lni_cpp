// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/help_command.h"

#include <utility>

#include "core/error/error.h"

namespace w3x_toolkit::cli {

HelpCommand::HelpCommand(HelpPrinter help_printer,
                         SpecificHelpPrinter specific_help_printer)
    : help_printer_(std::move(help_printer)),
      specific_help_printer_(std::move(specific_help_printer)) {}

std::string HelpCommand::Name() const { return "help"; }

std::string HelpCommand::Description() const {
  return "Show help for the CLI or a specific command";
}

std::string HelpCommand::Usage() const { return "help [command]"; }

core::Result<void> HelpCommand::Execute(const std::vector<std::string>& args) {
  if (args.empty()) {
    if (help_printer_) {
      help_printer_();
    }
    return {};
  }

  if (args.size() > 1) {
    return std::unexpected(core::Error::InvalidFormat(
        "Too many arguments.\nUsage: " + Usage()));
  }

  if (!specific_help_printer_ || !specific_help_printer_(args[0])) {
    return std::unexpected(core::Error::InvalidFormat(
        "Unknown command '" + args[0] + "'."));
  }
  return {};
}

}  // namespace w3x_toolkit::cli
