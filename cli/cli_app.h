// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_CLI_APP_H_
#define W3X_TOOLKIT_CLI_CLI_APP_H_

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "cli/commands/command.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Main CLI application.
//
// Parses the command line, dispatches to the appropriate Command subclass,
// and handles global flags such as --help and --version.
//
// Usage:
//   CliApp app;
//   return app.Run(argc, argv);
//
// The constructor automatically registers the built-in commands (convert,
// extract, analyze).
class CliApp {
 public:
  // Constructs the application and registers default commands.
  CliApp();
  ~CliApp() = default;

  // Non-copyable, non-movable.
  CliApp(const CliApp&) = delete;
  CliApp& operator=(const CliApp&) = delete;
  CliApp(CliApp&&) = delete;
  CliApp& operator=(CliApp&&) = delete;

  // Registers a command.  Ownership is transferred to the CliApp.
  // Returns an error if a command with the same name is already registered.
  core::Result<void> RegisterCommand(std::unique_ptr<Command> command);

  // Parses the command line and dispatches to the matching command.
  // Returns 0 on success or a non-zero exit code on failure.
  int Run(int argc, char** argv);

  // Prints the top-level help message listing all commands.
  void PrintHelp() const;

  // Prints the version string.
  static void PrintVersion();

 private:
  // Registers the built-in commands (convert, extract, analyze).
  void RegisterDefaultCommands();

  // Looks up a command by name.  Returns nullptr if not found.
  Command* FindCommand(const std::string& name) const;

  // Version constants.
  static constexpr const char* kVersion = "0.1.0";
  static constexpr const char* kAppName = "w3x_toolkit";

  // Registered commands keyed by name.
  std::unordered_map<std::string, std::unique_ptr<Command>> commands_;

  // Insertion-order list of command names (for help output).
  std::vector<std::string> command_order_;
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_CLI_APP_H_
