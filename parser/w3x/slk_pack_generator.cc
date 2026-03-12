#include "parser/w3x/slk_pack_generator.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/filesystem/filesystem_utils.h"

namespace w3x_toolkit::parser::w3x {

namespace {

using SectionEntries = std::vector<std::pair<std::string, std::string>>;
using IniSections = std::unordered_map<std::string, SectionEntries>;

struct SlkGenerationSpec {
  const char* input_ini;
  const char* output_file;
  const char* alias_header;
  const char* covered_object_file;
  bool controlled_by_slk_doodad = false;
};

constexpr SlkGenerationSpec kSpecs[] = {
    {"ability.ini", "units/abilitydata.slk", "alias", "war3map.w3a", false},
    {"buff.ini", "units/abilitybuffdata.slk", "alias", "war3map.w3h", false},
    {"item.ini", "units/itemdata.slk", "itemid", "war3map.w3t", false},
    {"upgrade.ini", "units/upgradedata.slk", "upgradeid", "war3map.w3q",
     false},
    {"destructable.ini", "units/destructabledata.slk", "destructableid",
     "war3map.w3b", true},
    {"doodad.ini", "doodads/doodads.slk", "doodid", "war3map.w3d", true},
};

std::string TrimCopy(std::string_view value) {
  std::size_t begin = 0;
  while (begin < value.size() &&
         std::isspace(static_cast<unsigned char>(value[begin]))) {
    ++begin;
  }
  std::size_t end = value.size();
  while (end > begin &&
         std::isspace(static_cast<unsigned char>(value[end - 1]))) {
    --end;
  }
  return std::string(value.substr(begin, end - begin));
}

std::string Lower(std::string_view value) {
  std::string lowered;
  lowered.reserve(value.size());
  for (char ch : value) {
    lowered.push_back(
        static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
  }
  return lowered;
}

std::string StripQuotes(std::string_view value) {
  std::string trimmed = TrimCopy(value);
  if (trimmed.size() >= 2 && trimmed.front() == '"' && trimmed.back() == '"') {
    return trimmed.substr(1, trimmed.size() - 2);
  }
  return trimmed;
}

bool StartsWithInsensitive(std::string_view value, std::string_view prefix) {
  if (value.size() < prefix.size()) {
    return false;
  }
  return Lower(value.substr(0, prefix.size())) == Lower(prefix);
}

bool IsRawFieldKey(std::string_view key) {
  return StartsWithInsensitive(key, "raw(");
}

core::Result<IniSections> ParseSectionedText(std::string_view content) {
  IniSections sections;
  std::string current_section;
  std::size_t line_start = 0;

  const auto consume_line = [&](std::string_view raw_line)
      -> core::Result<void> {
    const std::string line = TrimCopy(raw_line);
    if (line.empty() || line.starts_with(';') || line.starts_with('#') ||
        line.starts_with("//")) {
      return {};
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = StripQuotes(line.substr(1, line.size() - 2));
      sections[current_section];
      return {};
    }

    if (current_section.empty()) {
      return {};
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string::npos) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid ini line without '=' inside section [" + current_section +
          "]"));
    }

    std::string key = TrimCopy(line.substr(0, equal_pos));
    std::string value = TrimCopy(line.substr(equal_pos + 1));
    sections[current_section].push_back({std::move(key), std::move(value)});
    return {};
  };

  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n' || content[i] == '\r') {
      W3X_RETURN_IF_ERROR(
          consume_line(content.substr(line_start, i - line_start)));
      if (content[i] == '\r' && i + 1 < content.size() &&
          content[i + 1] == '\n') {
        ++i;
      }
      line_start = i + 1;
    }
  }
  if (line_start < content.size()) {
    W3X_RETURN_IF_ERROR(consume_line(content.substr(line_start)));
  }
  return sections;
}

std::string QuoteSlkValue(std::string_view value) {
  std::string quoted = "\"";
  for (char ch : value) {
    if (ch == '"') {
      quoted += "\"\"";
    } else {
      quoted.push_back(ch);
    }
  }
  quoted.push_back('"');
  return quoted;
}

core::Result<std::optional<GeneratedMapFile>> GenerateOneSlkFile(
    const std::filesystem::path& input_dir, const SlkGenerationSpec& spec,
    SlkPackResult& result) {
  const std::filesystem::path input_path = input_dir / spec.input_ini;
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::optional<GeneratedMapFile>();
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read ini file '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseSectionedText(content));
  if (sections.empty()) {
    return std::optional<GeneratedMapFile>();
  }

  std::vector<std::string> section_names;
  section_names.reserve(sections.size());
  for (const auto& [name, _] : sections) {
    section_names.push_back(name);
  }
  std::ranges::sort(section_names);

  std::map<std::string, std::string, std::less<>> header_names;
  bool can_cover_object_file = true;
  std::vector<std::unordered_map<std::string, std::string>> rows;
  rows.reserve(section_names.size());

  for (const std::string& section_name : section_names) {
    std::unordered_map<std::string, std::string> row;
    row.emplace(Lower(spec.alias_header), section_name);
    row.emplace("code", section_name);

    for (const auto& [key, value] : sections.at(section_name)) {
      const std::string normalized_key = Lower(key);
      if (normalized_key == "_parent") {
        const std::string parent = StripQuotes(value);
        if (!parent.empty()) {
          row["code"] = parent;
        }
        continue;
      }
      if (!key.empty() && key.front() == '_') {
        continue;
      }
      if (IsRawFieldKey(key)) {
        can_cover_object_file = false;
        continue;
      }
      header_names.emplace(normalized_key, key);
      row[normalized_key] = StripQuotes(value);
    }
    rows.push_back(std::move(row));
  }

  std::vector<std::string> headers;
  headers.push_back(spec.alias_header);
  headers.push_back("code");
  for (const auto& [normalized_key, original_key] : header_names) {
    if (normalized_key == Lower(spec.alias_header) || normalized_key == "code") {
      continue;
    }
    headers.push_back(original_key);
  }

  std::string output;
  output += "ID;PWXL;N;E\r\n";
  output += "B;X" + std::to_string(headers.size()) + ";Y" +
            std::to_string(rows.size() + 1) + ";D0\r\n";

  for (std::size_t col = 0; col < headers.size(); ++col) {
    output += "C;X" + std::to_string(col + 1) + ";Y1;K" +
              QuoteSlkValue(headers[col]) + "\r\n";
  }

  for (std::size_t row_index = 0; row_index < rows.size(); ++row_index) {
    const auto& row = rows[row_index];
    for (std::size_t col = 0; col < headers.size(); ++col) {
      const std::string normalized_header = Lower(headers[col]);
      const auto it = row.find(normalized_header);
      if (it == row.end() || it->second.empty()) {
        continue;
      }
      output += "C;X" + std::to_string(col + 1) + ";Y" +
                std::to_string(row_index + 2) + ";K" +
                QuoteSlkValue(it->second) + "\r\n";
    }
  }
  output += "E\r\n";

  if (can_cover_object_file) {
    result.covered_object_files.insert(spec.covered_object_file);
  }
  return std::optional<GeneratedMapFile>(GeneratedMapFile{
      spec.output_file, std::vector<std::uint8_t>(output.begin(), output.end())});
}

}  // namespace

core::Result<SlkPackResult> GenerateSyntheticSlkFiles(
    const std::filesystem::path& input_dir, const PackOptions& options) {
  SlkPackResult result;
  if (options.profile != PackProfile::kSlk) {
    return result;
  }

  for (const SlkGenerationSpec& spec : kSpecs) {
    if (spec.controlled_by_slk_doodad && !options.slk_doodad) {
      continue;
    }
    W3X_ASSIGN_OR_RETURN(auto generated,
                         GenerateOneSlkFile(input_dir, spec, result));
    if (generated.has_value()) {
      result.generated_files.push_back(std::move(*generated));
    }
  }
  return result;
}

}  // namespace w3x_toolkit::parser::w3x
