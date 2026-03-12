// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_LOG_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_LOG_COMMAND_H_

#include <string>
#include <vector>

#include "cli/commands/command.h"

namespace w3x_toolkit::cli {

class LogCommand : public Command {
 public:
  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_LOG_COMMAND_H_
