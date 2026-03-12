#ifndef W3X_TOOLKIT_CLI_COMMANDS_PACK_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_PACK_COMMAND_H_

#include <string>
#include <vector>

#include "cli/commands/command.h"

namespace w3x_toolkit::cli {

class PackCommand final : public Command {
 public:
  PackCommand() = default;
  ~PackCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_PACK_COMMAND_H_
