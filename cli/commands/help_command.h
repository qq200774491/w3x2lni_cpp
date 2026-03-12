// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_HELP_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_HELP_COMMAND_H_

#include <functional>
#include <string>
#include <vector>

#include "cli/commands/command.h"

namespace w3x_toolkit::cli {

class HelpCommand final : public Command {
 public:
  using HelpPrinter = std::function<void()>;
  using SpecificHelpPrinter = std::function<bool(const std::string&)>;

  HelpCommand(HelpPrinter help_printer,
              SpecificHelpPrinter specific_help_printer);

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  HelpPrinter help_printer_;
  SpecificHelpPrinter specific_help_printer_;
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_HELP_COMMAND_H_
