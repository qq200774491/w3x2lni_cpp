#include "parser/w3x/txt_pack_generator.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/filesystem/filesystem_utils.h"
#include "parser/w3x/wts_file.h"

namespace w3x_toolkit::parser::w3x {

namespace {

constexpr std::array<std::string_view, 44> kRecognizedTxtFiles = {
    "units/campaignunitstrings.txt",
    "units/humanunitstrings.txt",
    "units/neutralunitstrings.txt",
    "units/nightelfunitstrings.txt",
    "units/orcunitstrings.txt",
    "units/undeadunitstrings.txt",
    "units/campaignunitfunc.txt",
    "units/humanunitfunc.txt",
    "units/neutralunitfunc.txt",
    "units/nightelfunitfunc.txt",
    "units/orcunitfunc.txt",
    "units/undeadunitfunc.txt",
    "units/campaignabilitystrings.txt",
    "units/commonabilitystrings.txt",
    "units/humanabilitystrings.txt",
    "units/neutralabilitystrings.txt",
    "units/nightelfabilitystrings.txt",
    "units/orcabilitystrings.txt",
    "units/undeadabilitystrings.txt",
    "units/itemabilitystrings.txt",
    "units/campaignabilityfunc.txt",
    "units/commonabilityfunc.txt",
    "units/humanabilityfunc.txt",
    "units/neutralabilityfunc.txt",
    "units/nightelfabilityfunc.txt",
    "units/orcabilityfunc.txt",
    "units/undeadabilityfunc.txt",
    "units/itemabilityfunc.txt",
    "units/campaignupgradestrings.txt",
    "units/neutralupgradestrings.txt",
    "units/nightelfupgradestrings.txt",
    "units/humanupgradestrings.txt",
    "units/orcupgradestrings.txt",
    "units/undeadupgradestrings.txt",
    "units/campaignupgradefunc.txt",
    "units/humanupgradefunc.txt",
    "units/neutralupgradefunc.txt",
    "units/nightelfupgradefunc.txt",
    "units/orcupgradefunc.txt",
    "units/undeadupgradefunc.txt",
    "units/itemstrings.txt",
    "units/itemfunc.txt",
    "units/destructableskin.txt",
    "doodads/doodadskins.txt",
};

constexpr std::array<std::pair<std::string_view, std::string_view>, 8>
    kTxtOutputBySkinType = {{
        {"ability", "units/campaignabilitystrings.txt"},
        {"buff", "units/commonabilitystrings.txt"},
        {"unit", "units/campaignunitstrings.txt"},
        {"item", "units/itemstrings.txt"},
        {"upgrade", "units/campaignupgradestrings.txt"},
        {"doodad", "doodads/doodadskins.txt"},
        {"destructable", "units/orcunitstrings.txt"},
        {"txt", "units/itemabilitystrings.txt"},
    }};

using TxtValueList = std::vector<std::string>;

struct TxtIniSection {
  std::map<std::string, TxtValueList, std::less<>> values;
  std::vector<std::string> skin_types;
  std::optional<std::string> skinnable_id;
};

using TxtIniSections = std::map<std::string, TxtIniSection, std::less<>>;
using GeneratedFileMap = std::map<std::string, std::vector<std::uint8_t>, std::less<>>;

std::string_view Trim(std::string_view value) {
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.front()))) {
    value.remove_prefix(1);
  }
  while (!value.empty() &&
         std::isspace(static_cast<unsigned char>(value.back()))) {
    value.remove_suffix(1);
  }
  return value;
}

std::string TrimCopy(std::string_view value) {
  const std::string_view trimmed = Trim(value);
  return std::string(trimmed);
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

std::vector<std::string_view> SplitLines(std::string_view content) {
  std::vector<std::string_view> lines;
  std::size_t line_start = 0;
  for (std::size_t i = 0; i < content.size(); ++i) {
    if (content[i] == '\n' || content[i] == '\r') {
      lines.push_back(content.substr(line_start, i - line_start));
      if (content[i] == '\r' && i + 1 < content.size() &&
          content[i + 1] == '\n') {
        ++i;
      }
      line_start = i + 1;
    }
  }
  if (line_start < content.size()) {
    lines.push_back(content.substr(line_start));
  }
  return lines;
}

std::string DecodeQuotedString(std::string_view value) {
  std::string decoded;
  if (value.size() < 2) {
    return decoded;
  }

  decoded.reserve(value.size() - 2);
  for (std::size_t i = 1; i + 1 < value.size(); ++i) {
    const char ch = value[i];
    if (ch != '\\' || i + 2 >= value.size()) {
      decoded.push_back(ch);
      continue;
    }

    const char escaped = value[++i];
    switch (escaped) {
      case 'n':
        decoded.push_back('\n');
        break;
      case 'r':
        decoded.push_back('\r');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      case '\\':
        decoded.push_back('\\');
        break;
      case '"':
        decoded.push_back('"');
        break;
      default:
        decoded.push_back(escaped);
        break;
    }
  }
  return decoded;
}

std::string DecodeLongString(std::string_view value) {
  if (value.size() < 6 || !value.starts_with("[=[") || !value.ends_with("]=]")) {
    return TrimCopy(value);
  }

  std::string inner(value.substr(3, value.size() - 6));
  if (inner.starts_with("\r\n")) {
    inner.erase(0, 2);
  } else if (!inner.empty() && inner.front() == '\n') {
    inner.erase(0, 1);
  }
  return inner;
}

std::string ParseScalarValue(std::string_view value) {
  const std::string_view trimmed = Trim(value);
  if (trimmed.empty()) {
    return {};
  }
  if (trimmed.front() == '"' && trimmed.back() == '"') {
    return DecodeQuotedString(trimmed);
  }
  if (trimmed.starts_with("[=[") && trimmed.ends_with("]=]")) {
    return DecodeLongString(trimmed);
  }
  return std::string(trimmed);
}

std::vector<std::string> ParseArrayValues(std::string_view value) {
  std::vector<std::string> values;
  std::string token;
  bool inside_quotes = false;
  bool inside_long_string = false;

  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (inside_long_string) {
      token.push_back(ch);
      if (token.size() >= 3 && token.ends_with("]=]")) {
        inside_long_string = false;
      }
      continue;
    }

    if (inside_quotes) {
      token.push_back(ch);
      if (ch == '\\' && i + 1 < value.size()) {
        token.push_back(value[++i]);
        continue;
      }
      if (ch == '"') {
        inside_quotes = false;
      }
      continue;
    }

    if (ch == ',') {
      values.push_back(ParseScalarValue(token));
      token.clear();
      continue;
    }
    if (ch == '"') {
      inside_quotes = true;
      token.push_back(ch);
      continue;
    }
    if (ch == '[' && i + 2 < value.size() && value[i + 1] == '=' &&
        value[i + 2] == '[') {
      inside_long_string = true;
      token.append(value.substr(i, 3));
      i += 2;
      continue;
    }
    token.push_back(ch);
  }

  values.push_back(ParseScalarValue(token));
  return values;
}

std::vector<std::string> ParseValueList(std::string_view value) {
  const std::string_view trimmed = Trim(value);
  if (trimmed.empty()) {
    return {};
  }
  if (trimmed.front() == '{' && trimmed.back() == '}') {
    return ParseArrayValues(trimmed.substr(1, trimmed.size() - 2));
  }
  return {ParseScalarValue(trimmed)};
}

std::size_t CountLongStringOpens(std::string_view value) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = value.find("[=[", pos)) != std::string_view::npos) {
    ++count;
    pos += 3;
  }
  return count;
}

std::size_t CountLongStringCloses(std::string_view value) {
  std::size_t count = 0;
  std::size_t pos = 0;
  while ((pos = value.find("]=]", pos)) != std::string_view::npos) {
    ++count;
    pos += 3;
  }
  return count;
}

bool NeedsContinuation(std::string_view value) {
  return CountLongStringOpens(value) > CountLongStringCloses(value);
}

core::Result<TxtIniSections> ParseTxtIni(std::string_view content) {
  TxtIniSections sections;
  std::string current_section;
  const std::vector<std::string_view> lines = SplitLines(content);

  for (std::size_t index = 0; index < lines.size(); ++index) {
    const std::string_view raw_line = lines[index];
    const std::string_view line = Trim(raw_line);
    if (line.empty() || line.starts_with(';') || line.starts_with('#') ||
        line.starts_with("//")) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      current_section = TrimCopy(line.substr(1, line.size() - 2));
      sections[current_section];
      continue;
    }
    if (current_section.empty()) {
      continue;
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string_view::npos) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid txt.ini line without '=' inside section [" +
          current_section + "]"));
    }

    const std::string key = TrimCopy(line.substr(0, equal_pos));
    std::string raw_value = std::string(Trim(line.substr(equal_pos + 1)));
    while (NeedsContinuation(raw_value) && index + 1 < lines.size()) {
      raw_value += "\n";
      raw_value += std::string(lines[++index]);
    }

    TxtIniSection& section = sections[current_section];
    std::vector<std::string> values = ParseValueList(raw_value);
    const std::string lowered_key = Lower(key);
    if (lowered_key == "skintype") {
      section.skin_types = std::move(values);
      continue;
    }
    if (lowered_key == "skinnableid") {
      if (!values.empty() && !values.front().empty()) {
        section.skinnable_id = values.front();
      }
      continue;
    }
    section.values.emplace(key, std::move(values));
  }

  return sections;
}

std::optional<std::string_view> ResolveOutputFile(
    const TxtIniSection& section) {
  for (const std::string& skin_type : section.skin_types) {
    const std::string lowered = Lower(skin_type);
    for (const auto& [type_name, output_path] : kTxtOutputBySkinType) {
      if (lowered == type_name) {
        return output_path;
      }
    }
  }
  return std::string_view("units/itemabilitystrings.txt");
}

bool ShouldSkipField(std::string_view key) {
  const std::string lowered = Lower(key);
  return lowered.empty() || lowered.front() == '_' || lowered == "skintype" ||
         lowered == "skinnableid" || lowered == "editorsuffix" ||
         lowered == "editorname";
}

std::string QuoteTxtValueIfNeeded(std::string value) {
  std::string normalized;
  normalized.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '\r') {
      if (i + 1 < value.size() && value[i + 1] == '\n') {
        ++i;
      }
      normalized += "|n";
      continue;
    }
    if (ch == '\n') {
      normalized += "|n";
      continue;
    }
    normalized.push_back(ch);
  }

  const bool needs_quotes =
      normalized.find(',') != std::string::npos ||
      normalized.find('"') != std::string::npos;
  if (!needs_quotes) {
    return normalized;
  }

  std::string quoted;
  quoted.reserve(normalized.size() + 2);
  quoted.push_back('"');
  for (char ch : normalized) {
    if (ch == '"') {
      quoted.push_back('"');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

bool MustExternalizeToWts(std::string_view value) {
  return value.find(',') != std::string_view::npos &&
         value.find('"') != std::string_view::npos;
}

std::string FormatTxtValue(std::string value, WtsFile* wts) {
  if (wts != nullptr && MustExternalizeToWts(value)) {
    return wts->AddString(std::move(value));
  }
  return QuoteTxtValueIfNeeded(std::move(value));
}

std::optional<std::string> FormatTxtValues(const TxtValueList& values,
                                           WtsFile* wts) {
  if (values.empty()) {
    return std::nullopt;
  }

  std::vector<std::string> formatted_values;
  formatted_values.reserve(values.size());
  for (const std::string& value : values) {
    formatted_values.push_back(FormatTxtValue(value, wts));
  }

  std::string combined;
  for (std::size_t index = 0; index < formatted_values.size(); ++index) {
    if (index > 0) {
      combined += ",";
    }
    combined += formatted_values[index];
  }
  if (combined.empty()) {
    return std::nullopt;
  }
  return combined;
}

core::Result<std::optional<WtsFile>> LoadExistingWts(
    const std::filesystem::path& input_dir) {
  const std::filesystem::path input_path = input_dir / "war3map.wts";
  if (!core::FilesystemUtils::Exists(input_path)) {
    return std::optional<WtsFile>();
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read WTS file '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto wts, ParseWtsFile(content));
  return std::optional<WtsFile>(std::move(wts));
}

core::Result<void> GenerateTxtFilesFromIni(const std::filesystem::path& input_dir,
                                           WtsFile* wts,
                                           GeneratedFileMap& outputs) {
  const std::filesystem::path input_path = input_dir / "txt.ini";
  if (!core::FilesystemUtils::Exists(input_path)) {
    return {};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(input_path).transform_error(
          [&input_path](const std::string& error) {
            return core::Error::IOError("Failed to read txt ini '" +
                                        input_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto sections, ParseTxtIni(content));

  std::map<std::string, std::vector<std::string>, std::less<>> grouped_lines;
  for (const auto& [section_name, section] : sections) {
    const std::optional<std::string_view> output_file = ResolveOutputFile(section);
    if (!output_file.has_value()) {
      continue;
    }

    std::vector<std::string> lines;
    const std::string output_section_name =
        section.skinnable_id.value_or(section_name);
    lines.push_back("[" + output_section_name + "]");
    for (const auto& [key, values] : section.values) {
      if (ShouldSkipField(key)) {
        continue;
      }
      const std::optional<std::string> formatted = FormatTxtValues(values, wts);
      if (!formatted.has_value()) {
        continue;
      }
      lines.push_back(key + "=" + *formatted);
    }
    if (lines.size() == 1) {
      continue;
    }
    lines.push_back({});

    auto& output_lines = grouped_lines[std::string(*output_file)];
    output_lines.insert(output_lines.end(), lines.begin(), lines.end());
  }

  for (auto& [relative_path, lines] : grouped_lines) {
    while (!lines.empty() && lines.back().empty()) {
      lines.pop_back();
    }
    if (lines.empty()) {
      continue;
    }

    std::string rendered_text;
    for (std::size_t index = 0; index < lines.size(); ++index) {
      if (index > 0) {
        rendered_text += "\r\n";
      }
      rendered_text += lines[index];
    }
    outputs[relative_path] =
        std::vector<std::uint8_t>(rendered_text.begin(), rendered_text.end());
  }
  return {};
}

core::Result<void> LoadOneGeneratedFile(const std::filesystem::path& input_dir,
                                        std::string_view relative_path,
                                        GeneratedFileMap& outputs) {
  const std::string normalized_path(relative_path);
  if (outputs.contains(normalized_path)) {
    return {};
  }

  const std::filesystem::path path = input_dir / normalized_path;
  if (!core::FilesystemUtils::Exists(path)) {
    return {};
  }
  W3X_ASSIGN_OR_RETURN(
      auto bytes,
      core::FilesystemUtils::ReadBinaryFile(path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read text sidecar '" +
                                        path.string() + "': " + error);
          }));
  outputs.emplace(normalized_path, std::move(bytes));
  return {};
}

}  // namespace

core::Result<TxtPackResult> GenerateSyntheticTxtFiles(
    const std::filesystem::path& input_dir, const PackOptions& options) {
  TxtPackResult result;
  if (options.profile != PackProfile::kSlk) {
    return result;
  }

  GeneratedFileMap outputs;
  W3X_ASSIGN_OR_RETURN(auto existing_wts, LoadExistingWts(input_dir));
  WtsFile rebuilt_wts = existing_wts.value_or(WtsFile{});
  W3X_RETURN_IF_ERROR(GenerateTxtFilesFromIni(input_dir, &rebuilt_wts, outputs));

  for (std::string_view relative_path : kRecognizedTxtFiles) {
    W3X_RETURN_IF_ERROR(LoadOneGeneratedFile(input_dir, relative_path, outputs));
  }

  for (std::string_view extra_path : {"war3mapextra.txt", "war3mapskin.txt"}) {
    W3X_RETURN_IF_ERROR(LoadOneGeneratedFile(input_dir, extra_path, outputs));
  }
  if (!rebuilt_wts.entries.empty()) {
    const std::string rendered_wts = rebuilt_wts.Render();
    outputs["war3map.wts"] = std::vector<std::uint8_t>(
        rendered_wts.begin(), rendered_wts.end());
  }

  for (auto& [relative_path, content] : outputs) {
    result.generated_files.push_back(
        GeneratedMapFile{relative_path, std::move(content)});
  }
  return result;
}

}  // namespace w3x_toolkit::parser::w3x
