// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/config_command.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>

#include "core/config/config_manager.h"
#include "core/config/runtime_paths.h"
#include "core/error/error.h"

namespace w3x_toolkit::cli {

namespace {

core::Result<core::ConfigManager> LoadConfig(
    const std::filesystem::path& map_workspace) {
  core::ConfigManager config;
  W3X_RETURN_IF_ERROR(
      config.LoadDefaults(core::RuntimePaths::ResolveDefaultConfigPath()));
  W3X_RETURN_IF_ERROR(
      config.LoadGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));
  if (!map_workspace.empty()) {
    W3X_RETURN_IF_ERROR(config.LoadMap(map_workspace / "w3x2lni" / "config.ini"));
  }
  return config;
}

void PrintResolvedValue(const core::ConfigManager& config,
                        std::string_view section, std::string_view key) {
  if (auto value = config.GetRaw(section, key)) {
    std::cout << "[" << core::ConfigManager::LayerName(value->layer) << "] "
              << section << "." << key << " = " << value->value << std::endl;
  }
}

}  // namespace

std::string ConfigCommand::Name() const { return "config"; }

std::string ConfigCommand::Description() const {
  return "Inspect or update layered configuration values";
}

std::string ConfigCommand::Usage() const {
  return "config [--map <workspace_dir>] [section.key[=value]]";
}

core::Result<void> ConfigCommand::Execute(const std::vector<std::string>& args) {
  std::filesystem::path map_workspace;
  std::string request;

  for (std::size_t i = 0; i < args.size(); ++i) {
    if (args[i] == "--map") {
      if (i + 1 >= args.size()) {
        return std::unexpected(core::Error::InvalidFormat(
            "Missing workspace directory after --map.\nUsage: " + Usage()));
      }
      map_workspace = args[++i];
      continue;
    }
    if (!request.empty()) {
      return std::unexpected(core::Error::InvalidFormat(
          "Too many arguments.\nUsage: " + Usage()));
    }
    request = args[i];
  }

  W3X_ASSIGN_OR_RETURN(auto config, LoadConfig(map_workspace));

  if (request.empty()) {
    for (const std::string& section : config.Sections()) {
      for (const std::string& key : config.Keys(section)) {
        if (!config.IsVisible(section, key)) {
          continue;
        }
        PrintResolvedValue(config, section, key);
      }
    }
    return {};
  }

  const std::size_t equals_pos = request.find('=');
  if (equals_pos == std::string::npos) {
    W3X_ASSIGN_OR_RETURN(auto parsed,
                         core::ConfigManager::ParseQualifiedKey(request));
    if (!config.IsVisible(parsed.first, parsed.second)) {
      return std::unexpected(core::Error::ConfigError(
          "Unknown config key '" + request + "'"));
    }
    PrintResolvedValue(config, parsed.first, parsed.second);
    return {};
  }

  W3X_ASSIGN_OR_RETURN(auto parsed,
                       core::ConfigManager::ParseQualifiedKey(
                           request.substr(0, equals_pos)));
  const std::string value = request.substr(equals_pos + 1);
  if (!config.IsVisible(parsed.first, parsed.second)) {
    return std::unexpected(core::Error::ConfigError(
        "Unknown config key '" + request.substr(0, equals_pos) + "'"));
  }

  if (map_workspace.empty()) {
    W3X_RETURN_IF_ERROR(config.SetGlobal(parsed.first, parsed.second, value));
    W3X_RETURN_IF_ERROR(
        config.SaveGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));
  } else {
    W3X_RETURN_IF_ERROR(config.SetMap(parsed.first, parsed.second, value));
    W3X_RETURN_IF_ERROR(config.SaveMap(map_workspace / "w3x2lni" / "config.ini"));
  }

  PrintResolvedValue(config, parsed.first, parsed.second);
  return {};
}

}  // namespace w3x_toolkit::cli
