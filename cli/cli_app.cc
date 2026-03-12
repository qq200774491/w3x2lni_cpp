// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/cli_app.h"

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cli/commands/analyze_command.h"
#include "cli/commands/convert_command.h"
#include "cli/commands/config_command.h"
#include "cli/commands/extract_command.h"
#include "cli/commands/help_command.h"
#include "cli/commands/lni_command.h"
#include "cli/commands/log_command.h"
#include "cli/commands/pack_command.h"
#include "cli/commands/template_command.h"
#include "cli/commands/test_command.h"
#include "cli/commands/unpack_command.h"
#include "cli/commands/version_command.h"
#include "core/error/error.h"
#include "core/logger/logger.h"

namespace w3x_toolkit::cli {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CliApp::CliApp() { RegisterDefaultCommands(); }

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

core::Result<void> CliApp::RegisterCommand(
    std::unique_ptr<Command> command) {
  if (!command) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat, "Cannot register a null command"));
  }

  std::string name = command->Name();

  if (commands_.contains(name)) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Command '" + name + "' is already registered"));
  }

  command_order_.push_back(name);
  commands_.emplace(std::move(name), std::move(command));
  return {};
}

int CliApp::Run(int argc, char** argv) {
  // Build argument list (skip argv[0] which is the program name).
  std::vector<std::string> args;
  args.reserve(static_cast<std::size_t>(argc));
  for (int i = 1; i < argc; ++i) {
    args.emplace_back(argv[i]);
  }

  // No arguments -- print help.
  if (args.empty()) {
    PrintHelp();
    return 0;
  }

  // Check for global flags.
  const std::string& first = args[0];

  if (first == "--help" || first == "-h") {
    PrintHelp();
    return 0;
  }

  if (first == "--version" || first == "-v") {
    PrintVersion();
    return 0;
  }

  // Look up the subcommand.
  Command* command = FindCommand(first);
  if (command == nullptr) {
    std::cerr << "Error: Unknown command '" << first << "'." << std::endl;
    std::cerr << "Run '" << kAppName << " --help' to see available commands."
              << std::endl;
    return 1;
  }

  // Check for per-command --help.
  if (args.size() >= 2 && (args[1] == "--help" || args[1] == "-h")) {
    std::cout << "Usage: " << kAppName << " " << command->Usage()
              << std::endl;
    std::cout << std::endl;
    std::cout << "  " << command->Description() << std::endl;
    return 0;
  }

  // Forward remaining arguments to the command.
  std::vector<std::string> command_args(args.begin() + 1, args.end());
  auto result = command->Execute(command_args);

  if (!result.has_value()) {
    const auto& error = result.error();
    std::cerr << "Error: " << error.message() << std::endl;
    core::Logger::Instance().Error("Command '{}' failed: {}", first,
                                   error.message());
    return 1;
  }

  return 0;
}

void CliApp::PrintHelp() const {
  std::cout << kAppName << " v" << kVersion
            << " - Warcraft III Map Toolchain" << std::endl;
  std::cout << std::endl;
  std::cout << "Usage: " << kAppName << " <command> [options]" << std::endl;
  std::cout << std::endl;
  std::cout << "Commands:" << std::endl;

  // Compute the maximum command name length for alignment.
  std::size_t max_name_len = 0;
  for (const auto& name : command_order_) {
    max_name_len = std::max(max_name_len, name.size());
  }

  for (const auto& name : command_order_) {
    const auto& cmd = commands_.at(name);
    std::cout << "  " << std::left << std::setw(static_cast<int>(max_name_len + 4))
              << name << cmd->Description() << std::endl;
  }

  std::cout << std::endl;
  std::cout << "Global options:" << std::endl;
  std::cout << "  --help, -h       Show this help message" << std::endl;
  std::cout << "  --version, -v    Show version information" << std::endl;
  std::cout << std::endl;
  std::cout << "Use '" << kAppName
            << " <command> --help' for more information on a command."
            << std::endl;
}

void CliApp::PrintVersion() {
  std::cout << kAppName << " version " << kVersion << std::endl;
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void CliApp::RegisterDefaultCommands() {
  // Ignore registration errors here -- these are guaranteed unique names.
  (void)RegisterCommand(std::make_unique<HelpCommand>(
      [this]() { PrintHelp(); },
      [this](const std::string& name) {
        Command* command = FindCommand(name);
        if (command == nullptr) {
          return false;
        }
        std::cout << "Usage: " << kAppName << " " << command->Usage()
                  << std::endl;
        std::cout << std::endl;
        std::cout << "  " << command->Description() << std::endl;
        return true;
      }));
  (void)RegisterCommand(std::make_unique<VersionCommand>());
  (void)RegisterCommand(std::make_unique<ConfigCommand>());
  (void)RegisterCommand(std::make_unique<ConvertCommand>());
  (void)RegisterCommand(std::make_unique<LniCommand>());
  (void)RegisterCommand(std::make_unique<ExtractCommand>());
  (void)RegisterCommand(std::make_unique<AnalyzeCommand>());
  (void)RegisterCommand(std::make_unique<UnpackCommand>());
  (void)RegisterCommand(std::make_unique<PackCommand>());
  (void)RegisterCommand(std::make_unique<TemplateCommand>());
  (void)RegisterCommand(std::make_unique<LogCommand>());
  (void)RegisterCommand(std::make_unique<TestCommand>());
}

Command* CliApp::FindCommand(const std::string& name) const {
  auto it = commands_.find(name);
  if (it == commands_.end()) {
    return nullptr;
  }
  return it->second.get();
}

}  // namespace w3x_toolkit::cli
