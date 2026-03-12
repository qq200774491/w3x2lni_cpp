// Copyright 2026 W3X Toolkit Authors
//
// Licensed under the MIT License.

#include "cli/commands/analyze_command.h"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "cli/commands/map_input_utils.h"
#include "converter/resource_extract/resource_extractor.h"
#include "core/error/error.h"
#include "core/filesystem/filesystem_utils.h"
#include "core/logger/logger.h"
#include "parser/w3i/w3i_parser.h"
#include "parser/w3x/w3x_parser.h"

namespace w3x_toolkit::cli {

namespace {

struct ResourceSummaryRow {
  converter::ResourceType type;
  std::string_view label;
  std::size_t count = 0;
  std::uint64_t size = 0;
};

std::string FormatSize(std::uint64_t size) {
  std::ostringstream out;
  out << size << " B";
  if (size >= 1024 * 1024) {
    out << " (" << std::fixed << std::setprecision(2)
        << static_cast<double>(size) / (1024.0 * 1024.0) << " MB)";
  } else if (size >= 1024) {
    out << " (" << std::fixed << std::setprecision(2)
        << static_cast<double>(size) / 1024.0 << " KB)";
  }
  return out.str();
}

std::string ScriptTypeName(int32_t script_type) {
  if (script_type == 0) {
    return "JASS";
  }
  if (script_type == 1) {
    return "Lua";
  }
  return "Unknown";
}

void PrintW3iSummary(const std::filesystem::path& input_path) {
  std::cout << "Map Info (war3map.w3i):" << std::endl;

  auto archive = parser::w3x::OpenArchive(input_path);
  if (!archive) {
    std::cout << "  Failed to open input as a directory or MPQ archive."
              << std::endl
              << std::endl;
    return;
  }

  if (!archive->FileExists("war3map.w3i")) {
    std::cout << "  Not found in input map." << std::endl << std::endl;
    return;
  }

  auto data = archive->ExtractFile("war3map.w3i");
  if (!data.has_value()) {
    std::cout << "  Failed to read file: " << data.error().message()
              << std::endl
              << std::endl;
    return;
  }

  auto parsed = parser::w3i::ParseW3i(data.value());
  if (!parsed.has_value()) {
    std::cout << "  Failed to parse W3I metadata: "
              << parsed.error().message() << std::endl
              << std::endl;
    return;
  }

  const auto& w3i = parsed.value();
  std::cout << "  Name:            " << w3i.map_name << std::endl;
  std::cout << "  Author:          " << w3i.author << std::endl;
  std::cout << "  Description:     " << w3i.description << std::endl;
  std::cout << "  Recommended:     " << w3i.players_recommended << std::endl;
  std::cout << "  Dimensions:      " << w3i.map_width << " x "
            << w3i.map_height << std::endl;
  std::cout << "  Players:         " << w3i.players.size() << std::endl;
  std::cout << "  Forces:          " << w3i.forces.size() << std::endl;
  std::cout << "  Script type:     " << ScriptTypeName(w3i.script_type)
            << std::endl
            << std::endl;
}

}  // namespace

std::string AnalyzeCommand::Name() const { return "analyze"; }

std::string AnalyzeCommand::Description() const {
  return "Analyze a directory or packed map and print resource statistics";
}

std::string AnalyzeCommand::Usage() const {
  return "analyze <input_map_dir|input_map.w3x|input_map.w3m>";
}

core::Result<void> AnalyzeCommand::Execute(
    const std::vector<std::string>& args) {
  if (args.empty()) {
    return std::unexpected(core::Error(
        core::ErrorCode::kInvalidFormat,
        "Missing required argument.\nUsage: " + Usage()));
  }

  if (args.size() > 1) {
    core::Logger::Instance().Warn("Ignoring extra arguments after input path");
  }

  return RunAnalysis(args[0]);
}

core::Result<void> AnalyzeCommand::RunAnalysis(const std::string& input_path) {
  auto& logger = core::Logger::Instance();

  W3X_ASSIGN_OR_RETURN(auto input_source, ResolveMapInputPath(input_path));
  logger.Info("Analyzing map input: {}", input_source.string());

  converter::ResourceExtractor extractor(input_source);
  W3X_ASSIGN_OR_RETURN(auto resources, extractor.ListResources());

  std::array<ResourceSummaryRow, 7> summaries{{
      {converter::ResourceType::kModel, "Models"},
      {converter::ResourceType::kTexture, "Textures"},
      {converter::ResourceType::kSound, "Sounds"},
      {converter::ResourceType::kScript, "Scripts"},
      {converter::ResourceType::kUi, "UI"},
      {converter::ResourceType::kData, "Data"},
      {converter::ResourceType::kMap, "Map"},
  }};

  std::uint64_t total_size = 0;
  std::size_t unknown_count = 0;
  std::uint64_t unknown_size = 0;
  for (const auto& resource : resources) {
    total_size += resource.size;
    bool matched = false;
    for (auto& summary : summaries) {
      if (summary.type == resource.type) {
        ++summary.count;
        summary.size += resource.size;
        matched = true;
        break;
      }
    }
    if (!matched) {
      ++unknown_count;
      unknown_size += resource.size;
    }
  }

  std::cout << std::string(60, '=') << std::endl;
  std::cout << "  W3X Directory Analysis" << std::endl;
  std::cout << std::string(60, '=') << std::endl;
  std::cout << std::endl;

  std::cout << "Input Information:" << std::endl;
  std::cout << "  Path:            " << input_source.string() << std::endl;
  std::cout << "  Total files:     " << resources.size() << std::endl;
  std::cout << "  Total size:      " << FormatSize(total_size) << std::endl;
  std::cout << std::endl;

  PrintW3iSummary(input_source);

  std::cout << "Resource Breakdown:" << std::endl;
  for (const auto& summary : summaries) {
    std::cout << "  " << std::left << std::setw(14) << summary.label
              << std::right << std::setw(6) << summary.count << " file(s), "
              << FormatSize(summary.size) << std::endl;
  }
  if (unknown_count > 0) {
    std::cout << "  " << std::left << std::setw(14) << "Unknown"
              << std::right << std::setw(6) << unknown_count << " file(s), "
              << FormatSize(unknown_size) << std::endl;
  }
  std::cout << std::endl;
  std::cout << std::string(60, '=') << std::endl;

  logger.Info("Analysis complete.");
  return {};
}

}  // namespace w3x_toolkit::cli
