// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/test_command.h"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "core/config/runtime_paths.h"
#include "core/error/error.h"
#include "core/platform/platform.h"

namespace w3x_toolkit::cli {

namespace fs = std::filesystem;

namespace {

std::string SmokeTestsExecutableName() {
#if defined(W3X_PLATFORM_WINDOWS)
  return "w3x_smoke_tests.exe";
#else
  return "w3x_smoke_tests";
#endif
}

std::vector<fs::path> CandidateSmokeTestPaths() {
  std::vector<fs::path> candidates;
  const fs::path executable_dir = core::Platform::GetExecutableDirectory();
  if (!executable_dir.empty()) {
    candidates.push_back(executable_dir / SmokeTestsExecutableName());
  }
  candidates.push_back(fs::current_path() / SmokeTestsExecutableName());

  if (auto project_root = core::RuntimePaths::ResolveProjectRoot();
      project_root.has_value()) {
    candidates.push_back(project_root.value() / "build2" / "bin" / "Release" /
                         SmokeTestsExecutableName());
    candidates.push_back(project_root.value() / "build" / "bin" / "Release" /
                         SmokeTestsExecutableName());
  }
  return candidates;
}

core::Result<fs::path> ResolveSmokeTestExecutable() {
  for (const fs::path& candidate : CandidateSmokeTestPaths()) {
    if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
      return candidate;
    }
  }
  return std::unexpected(core::Error::FileNotFound(
      "Unable to locate '" + SmokeTestsExecutableName() +
      "'. Build tests first or run from a build tree."));
}

}  // namespace

std::string TestCommand::Name() const { return "test"; }

std::string TestCommand::Description() const {
  return "Run the bundled smoke test executable";
}

std::string TestCommand::Usage() const { return "test"; }

core::Result<void> TestCommand::Execute(const std::vector<std::string>& args) {
  if (!args.empty()) {
    return std::unexpected(core::Error::InvalidFormat(
        "This command does not accept arguments.\nUsage: " + Usage()));
  }

  W3X_ASSIGN_OR_RETURN(auto executable, ResolveSmokeTestExecutable());
  std::cout << "Running smoke tests: " << executable.string() << std::endl;

  const std::string command = "\"" + executable.string() + "\"";
  const int exit_code = std::system(command.c_str());
  if (exit_code != 0) {
    return std::unexpected(core::Error::Unknown(
        "Smoke tests failed with exit code " + std::to_string(exit_code)));
  }

  std::cout << "Smoke tests passed." << std::endl;
  return {};
}

}  // namespace w3x_toolkit::cli
