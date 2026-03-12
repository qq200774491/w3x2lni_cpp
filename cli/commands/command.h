// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_COMMAND_H_

#include <string>
#include <vector>

#include "core/error/error.h"

namespace w3x_toolkit::cli
{

  // Abstract base class for all CLI commands.
  //
  // Each command has a name (used as the subcommand on the command line), a
  // short description (shown in the help listing), a usage string, and an
  // Execute() method that performs the actual work.
  class Command
  {
  public:
    virtual ~Command() = default;

    // The subcommand name, e.g. "convert".
    virtual std::string Name() const = 0;

    // A one-line description shown in the help listing.
    virtual std::string Description() const = 0;

    // A usage string showing expected arguments, e.g.
    //   "convert <input> <output> [--format=lni|slk|obj]"
    virtual std::string Usage() const = 0;

    // Executes the command with the given argument list.
    // |args| contains only the arguments after the subcommand name.
    virtual core::Result<void> Execute(
        const std::vector<std::string> &args) = 0;

  protected:
    Command() = default;

    // Non-copyable, non-movable.
    Command(const Command &) = delete;
    Command &operator=(const Command &) = delete;
    Command(Command &&) = delete;
    Command &operator=(Command &&) = delete;
  };

} // namespace w3x_toolkit::cli

#endif // W3X_TOOLKIT_CLI_COMMANDS_COMMAND_H_
