// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#ifndef W3X_TOOLKIT_CLI_COMMANDS_ANALYZE_COMMAND_H_
#define W3X_TOOLKIT_CLI_COMMANDS_ANALYZE_COMMAND_H_

#include <string>
#include <vector>

#include "cli/commands/command.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

// Analyzes an unpacked Warcraft III map directory and prints structural
// information.
//
// Usage:
//   w3x_toolkit analyze <input_map_dir>
class AnalyzeCommand final : public Command {
 public:
  AnalyzeCommand() = default;
  ~AnalyzeCommand() override = default;

  std::string Name() const override;
  std::string Description() const override;
  std::string Usage() const override;
  core::Result<void> Execute(const std::vector<std::string>& args) override;

 private:
  core::Result<void> RunAnalysis(const std::string& input_path);
};

}  // namespace w3x_toolkit::cli

#endif  // W3X_TOOLKIT_CLI_COMMANDS_ANALYZE_COMMAND_H_
