#include "parser/w3x/content_cleanup.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <optional>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "core/config/config_manager.h"
#include "core/config/runtime_paths.h"
#include "core/filesystem/filesystem_utils.h"
#include "parser/w3i/w3i_parser.h"

namespace w3x_toolkit::parser::w3x {

namespace {

namespace fs = std::filesystem;

constexpr std::array<std::pair<std::string_view, std::string_view>, 9>
    kSourceFiles = {{
        {"ability", "ability.ini"},
        {"buff", "buff.ini"},
        {"unit", "unit.ini"},
        {"item", "item.ini"},
        {"upgrade", "upgrade.ini"},
        {"doodad", "doodad.ini"},
        {"destructable", "destructable.ini"},
        {"txt", "txt.ini"},
        {"misc", "misc.ini"},
    }};

constexpr std::array<std::string_view, 7> kObjectTypes = {
    "ability", "buff", "unit", "item", "upgrade", "doodad", "destructable",
};

constexpr std::array<std::string_view, 7> kRemovableUnusedTypes = {
    "ability", "buff", "unit", "item", "upgrade", "doodad", "destructable",
};

struct IniEntry {
  std::string key;
  std::string value;
  bool removed = false;
};

struct IniSection {
  std::string name;
  std::vector<IniEntry> entries;
  bool removed = false;
};

struct IniDocument {
  std::vector<IniSection> sections;
};

struct LoadedDocument {
  std::string type;
  fs::path path;
  IniDocument document;
  bool exists = false;
  bool dirty = false;
};

enum class ValueType : std::uint8_t {
  kUnknown = 0,
  kInteger = 1,
  kReal = 2,
  kString = 3,
};

struct ParsedValue {
  bool is_list = false;
  std::vector<std::string> values;
};

struct SearchEntry {
  std::string field;
  std::vector<std::string> target_types;
};

struct CurrentObjectNode {
  std::string type;
  std::size_t section_index = 0;
  std::string id;
  std::string normalized_id;
  std::string parent_id;
  std::string code;
  bool is_custom = false;
  bool marked = false;
};

struct FieldMetadata {
  std::string key;
  ValueType type = ValueType::kUnknown;
  bool profile = false;
  bool appendindex = false;
  std::string reforge;
};

struct JassSearchResult {
  std::unordered_set<std::string> ids;
  bool creeps = false;
  bool building = false;
  bool item = false;
  bool marketplace = false;
};

struct DooSearchResult {
  std::unordered_set<std::string> destructable_ids;
  std::unordered_set<std::string> doodad_ids;
};

using DefaultDocumentMap = std::unordered_map<std::string, IniDocument>;
using MetadataMap =
    std::unordered_map<std::string,
                       std::unordered_map<std::string, FieldMetadata>>;
using SearchMap =
    std::unordered_map<std::string, std::vector<SearchEntry>>;

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
  return std::string(Trim(value));
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

std::string NormalizeIdentifier(std::string_view value) {
  value = Trim(value);
  if (value.size() >= 2 && value.front() == '"' && value.back() == '"') {
    value.remove_prefix(1);
    value.remove_suffix(1);
  }
  return Lower(value);
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

std::string DecodeQuotedString(std::string_view value) {
  std::string decoded;
  if (value.size() < 2) {
    return decoded;
  }

  decoded.reserve(value.size() - 2);
  for (std::size_t i = 1; i + 1 < value.size(); ++i) {
    const char ch = value[i];
    if (ch == '"' && i + 2 < value.size() && value[i + 1] == '"') {
      decoded.push_back('"');
      ++i;
      continue;
    }
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

std::string DecodeScalarValue(std::string_view value) {
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
      if (ch == '"' && i + 1 < value.size() && value[i + 1] == '"') {
        token.push_back(value[++i]);
        continue;
      }
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
      values.push_back(DecodeScalarValue(token));
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

  values.push_back(DecodeScalarValue(token));
  return values;
}

ParsedValue ParseValue(std::string_view value) {
  const std::string_view trimmed = Trim(value);
  if (trimmed.empty()) {
    return ParsedValue{};
  }
  if (trimmed.front() == '{' && trimmed.back() == '}') {
    return ParsedValue{
        .is_list = true,
        .values = ParseArrayValues(trimmed.substr(1, trimmed.size() - 2)),
    };
  }
  return ParsedValue{.is_list = false, .values = {DecodeScalarValue(trimmed)}};
}

std::string QuoteIniString(std::string_view value) {
  const bool needs_quotes =
      value.empty() || value.find(',') != std::string_view::npos ||
      value.find('"') != std::string_view::npos ||
      value.find('\r') != std::string_view::npos ||
      value.find('\n') != std::string_view::npos ||
      value.find('{') != std::string_view::npos ||
      value.find('}') != std::string_view::npos ||
      std::isspace(static_cast<unsigned char>(value.front())) ||
      std::isspace(static_cast<unsigned char>(value.back()));
  if (!needs_quotes) {
    return std::string(value);
  }

  std::string quoted;
  quoted.reserve(value.size() + 2);
  quoted.push_back('"');
  for (char ch : value) {
    if (ch == '"') {
      quoted.push_back('"');
    }
    quoted.push_back(ch);
  }
  quoted.push_back('"');
  return quoted;
}

std::string RenderValue(const ParsedValue& value) {
  if (!value.is_list) {
    return QuoteIniString(value.values.empty() ? std::string_view{}
                                               : std::string_view(value.values[0]));
  }
  std::string rendered = "{";
  for (std::size_t i = 0; i < value.values.size(); ++i) {
    if (i > 0) {
      rendered += ",";
    }
    rendered += QuoteIniString(value.values[i]);
  }
  rendered += "}";
  return rendered;
}

core::Result<IniDocument> ParseIniDocument(std::string_view content) {
  IniDocument document;
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
      document.sections.push_back(IniSection{.name = current_section});
      continue;
    }
    if (current_section.empty() || document.sections.empty()) {
      continue;
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string_view::npos) {
      return std::unexpected(core::Error::InvalidFormat(
          "Invalid ini line without '=' inside section [" + current_section +
          "]"));
    }

    const std::string key = TrimCopy(line.substr(0, equal_pos));
    std::string raw_value = std::string(Trim(line.substr(equal_pos + 1)));
    while (NeedsContinuation(raw_value) && index + 1 < lines.size()) {
      raw_value += "\n";
      raw_value += std::string(lines[++index]);
    }
    document.sections.back().entries.push_back(
        IniEntry{.key = key, .value = std::move(raw_value)});
  }

  return document;
}

std::string RenderIniDocument(const IniDocument& document) {
  std::string output;
  bool first_section = true;
  for (const IniSection& section : document.sections) {
    if (section.removed) {
      continue;
    }

    bool has_entries = false;
    for (const IniEntry& entry : section.entries) {
      if (!entry.removed) {
        has_entries = true;
        break;
      }
    }
    if (!has_entries) {
      continue;
    }

    if (!first_section) {
      output += "\r\n";
    }
    first_section = false;
    output += "[" + section.name + "]\r\n";
    for (const IniEntry& entry : section.entries) {
      if (entry.removed) {
        continue;
      }
      output += entry.key + "=" + entry.value + "\r\n";
    }
  }
  return output;
}

const IniSection* FindSectionById(const IniDocument& document,
                                  std::string_view id) {
  const std::string normalized = NormalizeIdentifier(id);
  for (const IniSection& section : document.sections) {
    if (!section.removed &&
        NormalizeIdentifier(section.name) == normalized) {
      return &section;
    }
  }
  return nullptr;
}

IniSection* FindSectionById(IniDocument& document, std::string_view id) {
  const std::string normalized = NormalizeIdentifier(id);
  for (IniSection& section : document.sections) {
    if (!section.removed &&
        NormalizeIdentifier(section.name) == normalized) {
      return &section;
    }
  }
  return nullptr;
}

const IniEntry* FindEntryByKey(const IniSection& section, std::string_view key) {
  const std::string normalized = NormalizeIdentifier(key);
  for (const IniEntry& entry : section.entries) {
    if (!entry.removed && NormalizeIdentifier(entry.key) == normalized) {
      return &entry;
    }
  }
  return nullptr;
}

IniEntry* FindEntryByKey(IniSection& section, std::string_view key) {
  const std::string normalized = NormalizeIdentifier(key);
  for (IniEntry& entry : section.entries) {
    if (!entry.removed && NormalizeIdentifier(entry.key) == normalized) {
      return &entry;
    }
  }
  return nullptr;
}

std::optional<std::string> GetDecodedScalarField(const IniSection& section,
                                                 std::string_view key) {
  const IniEntry* entry = FindEntryByKey(section, key);
  if (entry == nullptr) {
    return std::nullopt;
  }
  ParsedValue parsed = ParseValue(entry->value);
  if (parsed.values.empty() || parsed.is_list) {
    return std::nullopt;
  }
  return parsed.values.front();
}

std::optional<int> ParseInt32(std::string_view value) {
  const std::string stripped = DecodeScalarValue(value);
  if (stripped.empty()) {
    return 0;
  }
  int parsed = 0;
  const auto result = std::from_chars(
      stripped.data(), stripped.data() + stripped.size(), parsed);
  if (result.ec == std::errc() && result.ptr == stripped.data() + stripped.size()) {
    return parsed;
  }
  return std::nullopt;
}

std::optional<double> ParseDouble(std::string_view value) {
  const std::string stripped = DecodeScalarValue(value);
  if (stripped.empty()) {
    return 0.0;
  }
  char* end = nullptr;
  const double parsed = std::strtod(stripped.c_str(), &end);
  if (end != stripped.c_str() + stripped.size()) {
    return std::nullopt;
  }
  return parsed;
}

std::optional<bool> ParseBoolish(std::string_view value) {
  const std::string normalized = NormalizeIdentifier(DecodeScalarValue(value));
  if (normalized == "true" || normalized == "1") {
    return true;
  }
  if (normalized == "false" || normalized == "0") {
    return false;
  }
  return std::nullopt;
}

bool AreScalarValuesEqual(std::string_view lhs, std::string_view rhs,
                          ValueType type) {
  if (type == ValueType::kInteger) {
    const auto lhs_value = ParseInt32(lhs);
    const auto rhs_value = ParseInt32(rhs);
    if (lhs_value.has_value() && rhs_value.has_value()) {
      return lhs_value.value() == rhs_value.value();
    }
  } else if (type == ValueType::kReal) {
    const auto lhs_value = ParseDouble(lhs);
    const auto rhs_value = ParseDouble(rhs);
    if (lhs_value.has_value() && rhs_value.has_value()) {
      return std::fabs(lhs_value.value() - rhs_value.value()) < 0.000001;
    }
  }
  return lhs == rhs;
}

bool AreValuesEqual(const ParsedValue& lhs, const ParsedValue& rhs,
                    ValueType type) {
  if (lhs.is_list != rhs.is_list || lhs.values.size() != rhs.values.size()) {
    return false;
  }
  for (std::size_t i = 0; i < lhs.values.size(); ++i) {
    if (!AreScalarValuesEqual(lhs.values[i], rhs.values[i], type)) {
      return false;
    }
  }
  return true;
}

bool IsTextContentKey(std::string_view normalized_key) {
  return !normalized_key.empty() && normalized_key.front() != '_' &&
         normalized_key != "skintype" && normalized_key != "skinnableid";
}

bool HasMeaningfulSectionContent(std::string_view type,
                                 const IniSection& section) {
  for (const IniEntry& entry : section.entries) {
    if (entry.removed) {
      continue;
    }
    const std::string normalized_key = NormalizeIdentifier(entry.key);
    if (type == "txt") {
      if (IsTextContentKey(normalized_key)) {
        return true;
      }
      continue;
    }
    if (!normalized_key.empty() && normalized_key.front() != '_') {
      return true;
    }
  }
  return false;
}

core::Result<fs::path> ResolvePrebuiltRoot() {
  core::ConfigManager config;
  W3X_RETURN_IF_ERROR(
      config.LoadDefaults(core::RuntimePaths::ResolveDefaultConfigPath()));
  W3X_RETURN_IF_ERROR(
      config.LoadGlobal(core::RuntimePaths::ResolveGlobalConfigPath()));

  if (auto data_root = core::RuntimePaths::ResolveDataRoot();
      data_root.has_value()) {
    const std::string preferred_version =
        config.GetString("global", "data", "zhCN-1.32.8");
    const fs::path preferred = data_root.value() / preferred_version / "prebuilt";
    if (fs::exists(preferred)) {
      return preferred;
    }

    for (const auto& entry : fs::directory_iterator(data_root.value())) {
      if (!entry.is_directory()) {
        continue;
      }
      const fs::path candidate = entry.path() / "prebuilt";
      if (fs::exists(candidate)) {
        return candidate;
      }
    }
  }

  const fs::path source_root = core::RuntimePaths::GetSourceRoot();
  if (!source_root.empty()) {
    const std::string preferred_version =
        config.GetString("global", "data", "zhCN-1.32.8");
    const fs::path preferred =
        source_root / "external" / "data" / preferred_version / "prebuilt";
    if (fs::exists(preferred)) {
      return preferred;
    }

    const fs::path fallback_data_root = source_root / "external" / "data";
    if (fs::exists(fallback_data_root)) {
      for (const auto& entry : fs::directory_iterator(fallback_data_root)) {
        if (!entry.is_directory()) {
          continue;
        }
        const fs::path candidate = entry.path() / "prebuilt";
        if (fs::exists(candidate)) {
          return candidate;
        }
      }
    }
  }

  return std::unexpected(core::Error::FileNotFound(
      "No bundled or fallback prebuilt metadata directory was found"));
}

core::Result<LoadedDocument> LoadInputDocument(const fs::path& input_dir,
                                               std::string_view type,
                                               std::string_view filename) {
  LoadedDocument loaded;
  loaded.type = std::string(type);
  loaded.path = input_dir / std::string(filename);
  if (!core::FilesystemUtils::Exists(loaded.path)) {
    return loaded;
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(loaded.path).transform_error(
          [&loaded](const std::string& error) {
            return core::Error::IOError("Failed to read source ini '" +
                                        loaded.path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(loaded.document, ParseIniDocument(content));
  loaded.exists = true;
  return loaded;
}

core::Result<void> SaveDocument(const LoadedDocument& document) {
  if (!document.exists || !document.dirty) {
    return {};
  }

  const std::string rendered = RenderIniDocument(document.document);
  if (rendered.empty()) {
    std::error_code ec;
    fs::remove(document.path, ec);
    return {};
  }

  W3X_RETURN_IF_ERROR(
      core::FilesystemUtils::WriteTextFile(document.path, rendered)
          .transform_error([&document](const std::string& error) {
            return core::Error::IOError("Failed to write cleaned ini '" +
                                        document.path.string() + "': " + error);
          }));
  return {};
}

core::Result<void> LoadDefaultDocuments(
    const fs::path& prebuilt_root, DefaultDocumentMap& defaults) {
  for (const auto& [type, filename] : kSourceFiles) {
    const fs::path path = prebuilt_root / "Custom" / std::string(filename);
    if (!core::FilesystemUtils::Exists(path)) {
      continue;
    }
    W3X_ASSIGN_OR_RETURN(
        auto content,
        core::FilesystemUtils::ReadTextFile(path).transform_error(
            [&path](const std::string& error) {
              return core::Error::IOError("Failed to read default ini '" +
                                          path.string() + "': " + error);
            }));
    W3X_ASSIGN_OR_RETURN(defaults[std::string(type)], ParseIniDocument(content));
  }
  return {};
}

core::Result<void> LoadMetadata(const fs::path& metadata_path,
                                MetadataMap& metadata) {
  if (!core::FilesystemUtils::Exists(metadata_path)) {
    return {};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(metadata_path).transform_error(
          [&metadata_path](const std::string& error) {
            return core::Error::IOError("Failed to read metadata ini '" +
                                        metadata_path.string() + "': " + error);
          }));

  std::string current_table;
  std::string current_entry;
  const std::vector<std::string_view> lines = SplitLines(content);
  for (std::string_view raw_line : lines) {
    const std::string_view line = Trim(raw_line);
    if (line.empty() || line.starts_with(';') || line.starts_with('#') ||
        line.starts_with("//")) {
      continue;
    }

    if (line.front() == '[' && line.back() == ']') {
      const std::string name =
          DecodeScalarValue(line.substr(1, line.size() - 2));
      if (!name.empty() && name.front() == '.') {
        if (current_table.empty()) {
          continue;
        }
        current_entry = NormalizeIdentifier(name.substr(1));
        metadata[current_table][current_entry];
      } else {
        current_table = NormalizeIdentifier(name);
        current_entry.clear();
        metadata[current_table];
      }
      continue;
    }

    if (current_table.empty() || current_entry.empty()) {
      continue;
    }

    const std::size_t equal_pos = line.find('=');
    if (equal_pos == std::string_view::npos) {
      continue;
    }
    const std::string key = NormalizeIdentifier(line.substr(0, equal_pos));
    FieldMetadata& field = metadata[current_table][current_entry];
    if (key == "type") {
      const auto parsed_type = ParseInt32(line.substr(equal_pos + 1));
      if (!parsed_type.has_value()) {
        continue;
      }
      switch (parsed_type.value()) {
        case 0:
          field.type = ValueType::kInteger;
          break;
        case 1:
        case 2:
          field.type = ValueType::kReal;
          break;
        case 3:
          field.type = ValueType::kString;
          break;
        default:
          field.type = ValueType::kUnknown;
          break;
      }
      continue;
    }
    if (key == "key") {
      field.key = NormalizeIdentifier(line.substr(equal_pos + 1));
      continue;
    }
    if (key == "profile") {
      field.profile = ParseBoolish(line.substr(equal_pos + 1)).value_or(false);
      continue;
    }
    if (key == "appendindex") {
      field.appendindex =
          ParseBoolish(line.substr(equal_pos + 1)).value_or(false);
      continue;
    }
    if (key == "reforge") {
      field.reforge = NormalizeIdentifier(line.substr(equal_pos + 1));
      continue;
    }
  }
  return {};
}

std::vector<std::string> SplitSimpleCsv(std::string_view value) {
  std::vector<std::string> parts;
  std::string current;
  for (char ch : value) {
    if (ch == ',') {
      parts.push_back(TrimCopy(current));
      current.clear();
      continue;
    }
    current.push_back(ch);
  }
  parts.push_back(TrimCopy(current));
  return parts;
}

core::Result<void> LoadSearchMetadata(const fs::path& search_path,
                                      SearchMap& search_map) {
  if (!core::FilesystemUtils::Exists(search_path)) {
    return {};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(search_path).transform_error(
          [&search_path](const std::string& error) {
            return core::Error::IOError("Failed to read search ini '" +
                                        search_path.string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto document, ParseIniDocument(content));
  for (const IniSection& section : document.sections) {
    if (section.removed) {
      continue;
    }
    std::vector<SearchEntry>& entries =
        search_map[NormalizeIdentifier(section.name)];
    for (const IniEntry& entry : section.entries) {
      if (entry.removed) {
        continue;
      }
      entries.push_back(SearchEntry{
          .field = NormalizeIdentifier(entry.key),
          .target_types = SplitSimpleCsv(DecodeScalarValue(entry.value)),
      });
      for (std::string& target_type : entries.back().target_types) {
        target_type = NormalizeIdentifier(target_type);
      }
    }
  }
  return {};
}

const IniSection* LookupDefaultSection(
    const DefaultDocumentMap& defaults, std::string_view type,
    std::string_view id) {
  const auto defaults_it = defaults.find(std::string(type));
  if (defaults_it == defaults.end()) {
    return nullptr;
  }
  return FindSectionById(defaults_it->second, id);
}

std::string ResolveSectionParent(std::string_view type, const IniSection& section,
                                 const DefaultDocumentMap& defaults) {
  if (const std::optional<std::string> parent =
          GetDecodedScalarField(section, "_parent");
      parent.has_value()) {
    return *parent;
  }
  if (const IniSection* same = LookupDefaultSection(defaults, type, section.name);
      same != nullptr) {
    return same->name;
  }
  return {};
}

const IniSection* ResolveDefaultSectionForCurrent(
    std::string_view type, const IniSection& section,
    const DefaultDocumentMap& defaults) {
  const std::optional<std::string> parent = GetDecodedScalarField(section, "_parent");
  if (parent.has_value()) {
    return LookupDefaultSection(defaults, type, *parent);
  }
  return LookupDefaultSection(defaults, type, section.name);
}

std::string ResolveSectionCode(std::string_view type, const IniSection& section,
                               const DefaultDocumentMap& defaults) {
  if (const std::optional<std::string> code =
          GetDecodedScalarField(section, "_code");
      code.has_value() && !code->empty()) {
    return *code;
  }

  if (const std::optional<std::string> parent =
          GetDecodedScalarField(section, "_parent");
      parent.has_value()) {
    if (const IniSection* parent_section =
            LookupDefaultSection(defaults, type, *parent);
        parent_section != nullptr) {
      if (const std::optional<std::string> code =
              GetDecodedScalarField(*parent_section, "_code");
          code.has_value() && !code->empty()) {
        return *code;
      }
      return parent_section->name;
    }
    return *parent;
  }

  if (const IniSection* same = LookupDefaultSection(defaults, type, section.name);
      same != nullptr) {
    if (const std::optional<std::string> code =
            GetDecodedScalarField(*same, "_code");
        code.has_value() && !code->empty()) {
      return *code;
    }
    return same->name;
  }

  return section.name;
}

const FieldMetadata* ResolveFieldMetadata(const MetadataMap& metadata,
                                          std::string_view type,
                                          std::string_view code,
                                          std::string_view key) {
  const std::string normalized_key = NormalizeIdentifier(key);
  if (const auto it = metadata.find(NormalizeIdentifier(type));
      it != metadata.end()) {
    if (const auto meta_it = it->second.find(normalized_key);
        meta_it != it->second.end()) {
      return &meta_it->second;
    }
  }
  if (!code.empty()) {
    if (const auto it = metadata.find(NormalizeIdentifier(code));
        it != metadata.end()) {
      if (const auto meta_it = it->second.find(normalized_key);
          meta_it != it->second.end()) {
        return &meta_it->second;
      }
    }
  }
  return nullptr;
}

bool IsTxtStyleField(std::string_view document_type,
                     const FieldMetadata* metadata) {
  return document_type == "txt" || (metadata != nullptr && metadata->profile);
}

std::vector<std::string> TrimSlkList(const std::vector<std::string>& data,
                                     const std::vector<std::string>& defaults,
                                     ValueType type) {
  if (data.empty()) {
    return {};
  }
  std::ptrdiff_t last_kept = -1;
  for (std::size_t i = 0; i < data.size(); ++i) {
    const std::string& default_value =
        defaults.empty() ? std::string()
                         : defaults[std::min(i, defaults.size() - 1)];
    if (!AreScalarValuesEqual(data[i], default_value, type)) {
      last_kept = static_cast<std::ptrdiff_t>(i);
    }
  }
  if (last_kept < 0) {
    return {};
  }
  return std::vector<std::string>(data.begin(), data.begin() + last_kept + 1);
}

std::vector<std::string> TrimTxtList(const std::vector<std::string>& data,
                                     const std::vector<std::string>& defaults,
                                     ValueType type, bool appendindex) {
  if (data.empty()) {
    return {};
  }

  if (appendindex) {
    std::ptrdiff_t last_kept = -1;
    for (std::size_t i = 0; i < data.size(); ++i) {
      const std::string default_value =
          i < defaults.size() ? defaults[i] : std::string();
      if (!AreScalarValuesEqual(data[i], default_value, type)) {
        last_kept = static_cast<std::ptrdiff_t>(i);
      }
    }
    if (last_kept < 0) {
      return {};
    }
    return std::vector<std::string>(data.begin(), data.begin() + last_kept + 1);
  }

  std::ptrdiff_t last_kept = -1;
  bool saw_difference = false;
  for (std::ptrdiff_t i = static_cast<std::ptrdiff_t>(data.size()) - 1; i >= 0;
       --i) {
    if (static_cast<std::size_t>(i) >= defaults.size()) {
      const std::string previous =
          i > 0 ? data[static_cast<std::size_t>(i - 1)] : std::string();
      if (saw_difference || !AreScalarValuesEqual(
                                data[static_cast<std::size_t>(i)], previous,
                                type)) {
        saw_difference = true;
        last_kept = std::max(last_kept, i);
      }
      continue;
    }

    if (!AreScalarValuesEqual(data[static_cast<std::size_t>(i)],
                              defaults[static_cast<std::size_t>(i)], type)) {
      saw_difference = true;
      last_kept = std::max(last_kept, i);
    }
  }

  if (!saw_difference) {
    return {};
  }
  return std::vector<std::string>(data.begin(), data.begin() + last_kept + 1);
}

bool ApplyRemoveSameToEntry(std::string_view document_type, IniSection& section,
                            IniEntry& entry, const IniSection& default_section,
                            const FieldMetadata* field_metadata,
                            ValueType field_type) {
  const IniEntry* default_entry = FindEntryByKey(default_section, entry.key);
  if (default_entry == nullptr) {
    return false;
  }

  ParsedValue current_value = ParseValue(entry.value);
  ParsedValue default_value = ParseValue(default_entry->value);
  if (field_metadata != nullptr && !field_metadata->reforge.empty()) {
    if (const IniEntry* reforge_entry =
            FindEntryByKey(section, field_metadata->reforge);
        reforge_entry != nullptr) {
      default_value = ParseValue(reforge_entry->value);
    }
  }

  if (current_value.is_list && default_value.is_list) {
    const bool txt_style = IsTxtStyleField(document_type, field_metadata);
    std::vector<std::string> trimmed =
        txt_style
            ? TrimTxtList(current_value.values, default_value.values, field_type,
                          field_metadata != nullptr && field_metadata->appendindex)
            : TrimSlkList(current_value.values, default_value.values, field_type);
    if (trimmed.empty()) {
      entry.removed = true;
      return true;
    }

    ParsedValue rendered_value{.is_list = true, .values = std::move(trimmed)};
    const std::string rendered = RenderValue(rendered_value);
    if (rendered == entry.value) {
      return false;
    }
    entry.value = rendered;
    return true;
  }

  if (AreValuesEqual(current_value, default_value, field_type)) {
    entry.removed = true;
    return true;
  }
  return false;
}

bool ApplyRemoveSameToDocument(LoadedDocument& current_document,
                               const DefaultDocumentMap& defaults,
                               const MetadataMap& metadata) {
  if (!current_document.exists) {
    return false;
  }

  bool changed = false;
  for (IniSection& section : current_document.document.sections) {
    if (section.removed) {
      continue;
    }
    const IniSection* default_section = ResolveDefaultSectionForCurrent(
        current_document.type, section, defaults);
    if (default_section == nullptr) {
      continue;
    }
    const std::string code =
        ResolveSectionCode(current_document.type, section, defaults);

    for (IniEntry& entry : section.entries) {
      if (entry.removed) {
        continue;
      }
      const std::string normalized_key = NormalizeIdentifier(entry.key);
      if (normalized_key.empty() || normalized_key.front() == '_') {
        continue;
      }
      if (current_document.type == "txt" && !IsTextContentKey(normalized_key)) {
        continue;
      }

      const FieldMetadata* field_metadata =
          current_document.type == "txt"
              ? nullptr
              : ResolveFieldMetadata(metadata, current_document.type, code,
                                     normalized_key);
      const ValueType field_type =
          current_document.type == "txt"
              ? ValueType::kString
              : (field_metadata != nullptr ? field_metadata->type
                                           : ValueType::kUnknown);
      changed = ApplyRemoveSameToEntry(current_document.type, section, entry,
                                       *default_section, field_metadata,
                                       field_type) ||
                changed;
    }

    if (!HasMeaningfulSectionContent(current_document.type, section)) {
      section.removed = true;
      changed = true;
    }
  }

  current_document.dirty = current_document.dirty || changed;
  return changed;
}

constexpr std::uint32_t kMinJassRawcodeValue = 0x41303030U;  // 'A000'

std::optional<fs::path> FindFirstExistingPath(
    std::initializer_list<fs::path> candidates) {
  for (const fs::path& candidate : candidates) {
    if (core::FilesystemUtils::Exists(candidate)) {
      return candidate;
    }
  }
  return std::nullopt;
}

std::string RawcodeFromUint32(std::uint32_t value) {
  std::string rawcode(4, '\0');
  rawcode[0] = static_cast<char>((value >> 24) & 0xFFU);
  rawcode[1] = static_cast<char>((value >> 16) & 0xFFU);
  rawcode[2] = static_cast<char>((value >> 8) & 0xFFU);
  rawcode[3] = static_cast<char>(value & 0xFFU);
  return rawcode;
}

std::optional<std::uint32_t> ParseUnsignedInteger(std::string_view value,
                                                  int base) {
  if (value.empty()) {
    return std::nullopt;
  }
  std::uint32_t parsed = 0;
  const auto result =
      std::from_chars(value.data(), value.data() + value.size(), parsed, base);
  if (result.ec != std::errc() || result.ptr != value.data() + value.size()) {
    return std::nullopt;
  }
  return parsed;
}

void AddRawcodeId(JassSearchResult& result, std::string_view rawcode) {
  if (rawcode.size() != 4) {
    return;
  }
  result.ids.insert(NormalizeIdentifier(rawcode));
}

void AddRawcodeInteger(JassSearchResult& result, std::uint32_t value) {
  if (value < kMinJassRawcodeValue) {
    return;
  }
  AddRawcodeId(result, RawcodeFromUint32(value));
}

bool IsIdentifierStart(char ch) {
  return std::isalpha(static_cast<unsigned char>(ch)) || ch == '_';
}

bool IsIdentifierContinue(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) || ch == '_';
}

const std::unordered_map<std::string, std::vector<std::string>>&
GetJassExtraFunctions() {
  static const auto* kExtraFunctions =
      new std::unordered_map<std::string, std::vector<std::string>>{
          {"meleestartingunitshuman",
           {"hpea", "Hamg", "Hpal", "Hblm", "Hmkg", "htow", "Amic",
            "stwp"}},
          {"meleestartingunitsorc",
           {"opeo", "Obla", "Ofar", "ogre", "Otch", "Oshd", "stwp"}},
          {"meleestartingunitsundead",
           {"Udre", "Udea", "Ucrl", "uaco", "unpl", "ugho", "Ulic",
            "stwp"}},
          {"meleestartingunitsnightelf",
           {"etol", "Edem", "ewsp", "Ewar", "Emoo", "Ekee", "stwp"}},
          {"meleeStartingUnitsUnknownRace", {"nshe"}},
      };
  static const auto* kCanonicalized = [] {
    auto* canonicalized =
        new std::unordered_map<std::string, std::vector<std::string>>();
    for (const auto& [name, ids] : *kExtraFunctions) {
      std::vector<std::string> lowered;
      lowered.reserve(ids.size());
      for (std::string_view id : ids) {
        lowered.push_back(NormalizeIdentifier(id));
      }
      canonicalized->emplace(Lower(name), std::move(lowered));
    }
    canonicalized->emplace(
        "meleegrantitemstohero",
        std::vector<std::string>{NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "meleegrantitemstotrainedhero",
        std::vector<std::string>{NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "meleegrantitemstohiredhero",
        std::vector<std::string>{NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "meleegrantheroitems",
        std::vector<std::string>{NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "meleerandomheroloc",
        std::vector<std::string>{NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "changeelevatorwallblocker",
        std::vector<std::string>{NormalizeIdentifier("DTep")});
    canonicalized->emplace(
        "nearbyelevatorexistsenum",
        std::vector<std::string>{NormalizeIdentifier("DTrx"),
                                 NormalizeIdentifier("DTrf")});
    canonicalized->emplace(
        "nearbyelevatorexists",
        std::vector<std::string>{NormalizeIdentifier("DTrx"),
                                 NormalizeIdentifier("DTrf")});
    canonicalized->emplace(
        "changeelevatorwalls",
        std::vector<std::string>{NormalizeIdentifier("DTep"),
                                 NormalizeIdentifier("DTrx"),
                                 NormalizeIdentifier("DTrf")});
    canonicalized->emplace(
        "meleestartingunitsforplayer",
        std::vector<std::string>{
            NormalizeIdentifier("hpea"), NormalizeIdentifier("Hamg"),
            NormalizeIdentifier("Hpal"), NormalizeIdentifier("Hblm"),
            NormalizeIdentifier("Hmkg"), NormalizeIdentifier("htow"),
            NormalizeIdentifier("Amic"), NormalizeIdentifier("opeo"),
            NormalizeIdentifier("Obla"), NormalizeIdentifier("Ofar"),
            NormalizeIdentifier("ogre"), NormalizeIdentifier("Otch"),
            NormalizeIdentifier("Oshd"), NormalizeIdentifier("Udre"),
            NormalizeIdentifier("Udea"), NormalizeIdentifier("Ucrl"),
            NormalizeIdentifier("uaco"), NormalizeIdentifier("unpl"),
            NormalizeIdentifier("ugho"), NormalizeIdentifier("Ulic"),
            NormalizeIdentifier("etol"), NormalizeIdentifier("Edem"),
            NormalizeIdentifier("ewsp"), NormalizeIdentifier("Ewar"),
            NormalizeIdentifier("Emoo"), NormalizeIdentifier("Ekee"),
            NormalizeIdentifier("stwp")});
    canonicalized->emplace(
        "meleestartingunits",
        std::vector<std::string>{
            NormalizeIdentifier("hpea"), NormalizeIdentifier("Hamg"),
            NormalizeIdentifier("Hpal"), NormalizeIdentifier("Hblm"),
            NormalizeIdentifier("Hmkg"), NormalizeIdentifier("htow"),
            NormalizeIdentifier("Amic"), NormalizeIdentifier("opeo"),
            NormalizeIdentifier("Obla"), NormalizeIdentifier("Ofar"),
            NormalizeIdentifier("ogre"), NormalizeIdentifier("Otch"),
            NormalizeIdentifier("Oshd"), NormalizeIdentifier("Udre"),
            NormalizeIdentifier("Udea"), NormalizeIdentifier("Ucrl"),
            NormalizeIdentifier("uaco"), NormalizeIdentifier("unpl"),
            NormalizeIdentifier("ugho"), NormalizeIdentifier("Ulic"),
            NormalizeIdentifier("etol"), NormalizeIdentifier("Edem"),
            NormalizeIdentifier("ewsp"), NormalizeIdentifier("Ewar"),
            NormalizeIdentifier("Emoo"), NormalizeIdentifier("Ekee"),
            NormalizeIdentifier("nshe"), NormalizeIdentifier("stwp")});
    return canonicalized;
  }();
  return *kCanonicalized;
}

void MarkJassIdentifier(JassSearchResult& result, std::string_view identifier) {
  const std::string normalized = Lower(identifier);

  if (const auto it = GetJassExtraFunctions().find(normalized);
      it != GetJassExtraFunctions().end()) {
    for (const std::string& id : it->second) {
      result.ids.insert(id);
    }
  }

  if (normalized == "chooserandomcreep" ||
      normalized == "chooserandomcreepbj") {
    result.creeps = true;
  } else if (normalized == "chooserandomnpbuilding" ||
             normalized == "chooserandomnpbuildingbj") {
    result.building = true;
  } else if (normalized == "chooserandomitem" ||
             normalized == "chooserandomitembj" ||
             normalized == "chooserandomitemex" ||
             normalized == "chooserandomitemexbj" ||
             normalized == "updateeachstockbuildingenum") {
    result.item = true;
  } else if (normalized == "updateeachstockbuilding" ||
             normalized == "performstockupdates" ||
             normalized == "startstockupdates" ||
             normalized == "initneutralbuildings" ||
             normalized == "initblizzard") {
    result.marketplace = true;
  }
}

core::Result<JassSearchResult> LoadJassSearchResult(const fs::path& input_dir) {
  const std::optional<fs::path> path = FindFirstExistingPath(
      {input_dir / "war3map.j", input_dir / "map" / "war3map.j",
       input_dir / "scripts" / "war3map.j",
       input_dir / "map" / "scripts" / "war3map.j"});
  if (!path.has_value()) {
    return JassSearchResult{};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadTextFile(*path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read JASS file '" +
                                        path->string() + "': " + error);
          }));

  JassSearchResult result;
  std::size_t pos = 0;
  while (pos < content.size()) {
    const char ch = content[pos];
    if (std::isspace(static_cast<unsigned char>(ch))) {
      ++pos;
      continue;
    }
    if (ch == '/' && pos + 1 < content.size() && content[pos + 1] == '/') {
      pos += 2;
      while (pos < content.size() && content[pos] != '\r' &&
             content[pos] != '\n') {
        ++pos;
      }
      continue;
    }
    if (ch == '"') {
      ++pos;
      while (pos < content.size()) {
        if (content[pos] == '\\' && pos + 1 < content.size()) {
          pos += 2;
          continue;
        }
        if (content[pos] == '"') {
          ++pos;
          break;
        }
        ++pos;
      }
      continue;
    }
    if (ch == '\'' && pos + 5 < content.size() && content[pos + 5] == '\'') {
      bool printable = true;
      for (std::size_t i = pos + 1; i < pos + 5; ++i) {
        if (content[i] == '\r' || content[i] == '\n' ||
            static_cast<unsigned char>(content[i]) < 32U) {
          printable = false;
          break;
        }
      }
      if (printable) {
        AddRawcodeId(result, std::string_view(content).substr(pos + 1, 4));
        pos += 6;
        continue;
      }
    }
    if (ch == '$') {
      std::size_t end = pos + 1;
      while (end < content.size() &&
             std::isxdigit(static_cast<unsigned char>(content[end]))) {
        ++end;
      }
      if (const auto parsed = ParseUnsignedInteger(
              std::string_view(content).substr(pos + 1, end - pos - 1), 16);
          parsed.has_value()) {
        AddRawcodeInteger(result, parsed.value());
      }
      pos = end;
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(ch))) {
      if (ch == '0' && pos + 2 <= content.size() &&
          pos + 1 < content.size() &&
          (content[pos + 1] == 'x' || content[pos + 1] == 'X')) {
        std::size_t end = pos + 2;
        while (end < content.size() &&
               std::isxdigit(static_cast<unsigned char>(content[end]))) {
          ++end;
        }
        if (const auto parsed = ParseUnsignedInteger(
                std::string_view(content).substr(pos + 2, end - pos - 2), 16);
            parsed.has_value()) {
          AddRawcodeInteger(result, parsed.value());
        }
        pos = end;
        continue;
      }

      std::size_t end = pos + 1;
      while (end < content.size() &&
             std::isdigit(static_cast<unsigned char>(content[end]))) {
        ++end;
      }
      if (const auto parsed = ParseUnsignedInteger(
              std::string_view(content).substr(pos, end - pos), 10);
          parsed.has_value()) {
        AddRawcodeInteger(result, parsed.value());
      }
      pos = end;
      continue;
    }
    if (IsIdentifierStart(ch)) {
      std::size_t end = pos + 1;
      while (end < content.size() && IsIdentifierContinue(content[end])) {
        ++end;
      }
      MarkJassIdentifier(result, std::string_view(content).substr(pos, end - pos));
      pos = end;
      continue;
    }
    ++pos;
  }

  return result;
}

class BinaryCursor {
 public:
  explicit BinaryCursor(const std::vector<std::uint8_t>& data) : data_(data) {}

  bool Has(std::size_t count) const { return pos_ + count <= data_.size(); }

  bool ReadInt32(std::int32_t& value) {
    if (!Has(sizeof(value))) {
      return false;
    }
    std::memcpy(&value, data_.data() + pos_, sizeof(value));
    pos_ += sizeof(value);
    return true;
  }

  bool ReadByte(std::uint8_t& value) {
    if (!Has(1)) {
      return false;
    }
    value = data_[pos_++];
    return true;
  }

  bool ReadRawcode(std::string& value) {
    if (!Has(4)) {
      return false;
    }
    value.assign(reinterpret_cast<const char*>(data_.data() + pos_), 4);
    pos_ += 4;
    return true;
  }

  bool PeekRawcode(std::string_view value) const {
    return value.size() == 4 && Has(4) &&
           std::memcmp(data_.data() + pos_, value.data(), 4) == 0;
  }

  bool Skip(std::size_t count) {
    if (!Has(count)) {
      return false;
    }
    pos_ += count;
    return true;
  }

 private:
  const std::vector<std::uint8_t>& data_;
  std::size_t pos_ = 0;
};

core::Result<DooSearchResult> LoadDooSearchResult(const fs::path& input_dir) {
  const std::optional<fs::path> path = FindFirstExistingPath(
      {input_dir / "war3map.doo", input_dir / "map" / "war3map.doo"});
  if (!path.has_value()) {
    return DooSearchResult{};
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadBinaryFile(*path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read DOO file '" +
                                        path->string() + "': " + error);
          }));

  BinaryCursor cursor(content);
  std::int32_t file_id = 0;
  std::int32_t version = 0;
  std::int32_t sub_version = 0;
  std::int32_t destructable_count = 0;
  if (!cursor.ReadInt32(file_id) || !cursor.ReadInt32(version) ||
      !cursor.ReadInt32(sub_version) ||
      !cursor.ReadInt32(destructable_count)) {
    return std::unexpected(
        core::Error::ParseError("war3map.doo header is truncated"));
  }

  DooSearchResult result;
  for (std::int32_t i = 0; i < destructable_count; ++i) {
    std::string rawcode;
    if (!cursor.ReadRawcode(rawcode)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo destructable rawcode table is truncated"));
    }
    result.destructable_ids.insert(NormalizeIdentifier(rawcode));

    if (version < 8) {
      if (!cursor.Skip(4 + 7 * 4 + 1 + 1 + 4)) {
        return std::unexpected(core::Error::ParseError(
            "war3map.doo legacy destructable block is truncated"));
      }
      continue;
    }

    if (!cursor.Skip(4 + 7 * 4)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo destructable transform block is truncated"));
    }
    if (cursor.PeekRawcode(rawcode) && !cursor.Skip(4)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo TM variation rawcode is truncated"));
    }

    std::uint8_t life = 0;
    std::uint8_t item_table = 0;
    std::int32_t dropped_item_table_count = 0;
    if (!cursor.ReadByte(life) || !cursor.ReadByte(item_table) ||
        !cursor.ReadInt32(dropped_item_table_count)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo destructable random table header is truncated"));
    }

    for (std::int32_t set_index = 0; set_index < dropped_item_table_count;
         ++set_index) {
      std::int32_t item_count = 0;
      if (!cursor.ReadInt32(item_count)) {
        return std::unexpected(core::Error::ParseError(
            "war3map.doo destructable random item set is truncated"));
      }
      for (std::int32_t item_index = 0; item_index < item_count; ++item_index) {
        if (!cursor.Skip(8)) {
          return std::unexpected(core::Error::ParseError(
              "war3map.doo destructable random item entry is truncated"));
        }
      }
    }

    std::int32_t editor_id = 0;
    if (!cursor.ReadInt32(editor_id)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo destructable editor id is truncated"));
    }
  }

  std::int32_t doodad_version = 0;
  std::int32_t doodad_count = 0;
  if (!cursor.ReadInt32(doodad_version) || !cursor.ReadInt32(doodad_count)) {
    return result;
  }

  for (std::int32_t i = 0; i < doodad_count; ++i) {
    std::string rawcode;
    if (!cursor.ReadRawcode(rawcode) || !cursor.Skip(12)) {
      return std::unexpected(core::Error::ParseError(
          "war3map.doo doodad block is truncated"));
    }
    result.doodad_ids.insert(NormalizeIdentifier(rawcode));
  }

  return result;
}

core::Result<char> LoadMapMainGround(const fs::path& input_dir) {
  const std::optional<fs::path> path = FindFirstExistingPath(
      {input_dir / "war3map.w3i", input_dir / "map" / "war3map.w3i"});
  if (!path.has_value()) {
    return '\0';
  }

  W3X_ASSIGN_OR_RETURN(
      auto content,
      core::FilesystemUtils::ReadBinaryFile(*path).transform_error(
          [&path](const std::string& error) {
            return core::Error::IOError("Failed to read W3I file '" +
                                        path->string() + "': " + error);
          }));
  W3X_ASSIGN_OR_RETURN(auto w3i, parser::w3i::ParseW3i(content));
  return w3i.main_ground;
}

std::vector<std::string> ExtractReferenceIds(const IniEntry& entry) {
  ParsedValue parsed = ParseValue(entry.value);
  std::vector<std::string> ids;
  for (const std::string& value : parsed.values) {
    for (std::string part : SplitSimpleCsv(value)) {
      part = TrimCopy(part);
      if (part.empty()) {
        continue;
      }
      ids.push_back(part);
    }
  }
  return ids;
}

std::string MakeNodeKey(std::string_view type, std::string_view id) {
  return std::string(type) + ":" + std::string(id);
}

void MarkNode(std::size_t node_index, std::vector<CurrentObjectNode>& nodes,
              std::queue<std::size_t>& pending) {
  if (node_index >= nodes.size() || nodes[node_index].marked) {
    return;
  }
  nodes[node_index].marked = true;
  pending.push(node_index);
}

const IniSection* ResolveCurrentNodeSection(
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const CurrentObjectNode& node) {
  const auto doc_it = documents.find(node.type);
  if (doc_it == documents.end() || !doc_it->second.exists ||
      node.section_index >= doc_it->second.document.sections.size()) {
    return nullptr;
  }
  return &doc_it->second.document.sections[node.section_index];
}

const IniSection* ResolveDefaultNodeSection(
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, const CurrentObjectNode& node) {
  const IniSection* current_section = ResolveCurrentNodeSection(documents, node);
  if (current_section == nullptr) {
    return nullptr;
  }
  return ResolveDefaultSectionForCurrent(node.type, *current_section, defaults);
}

const IniEntry* FindEffectiveEntry(const IniSection& current_section,
                                   const IniSection* default_section,
                                   std::string_view key) {
  if (const IniEntry* entry = FindEntryByKey(current_section, key);
      entry != nullptr) {
    return entry;
  }
  if (default_section != nullptr) {
    return FindEntryByKey(*default_section, key);
  }
  return nullptr;
}

std::optional<std::string> GetEffectiveScalarField(
    std::string_view type, const IniSection& current_section,
    const DefaultDocumentMap& defaults, std::string_view key) {
  if (const std::optional<std::string> value =
          GetDecodedScalarField(current_section, key);
      value.has_value()) {
    return value;
  }
  if (const IniSection* default_section =
          ResolveDefaultSectionForCurrent(type, current_section, defaults);
      default_section != nullptr) {
    return GetDecodedScalarField(*default_section, key);
  }
  return std::nullopt;
}

int GetEffectiveIntegerField(std::string_view type,
                             const IniSection& current_section,
                             const DefaultDocumentMap& defaults,
                             std::string_view key) {
  if (const std::optional<std::string> value =
          GetEffectiveScalarField(type, current_section, defaults, key);
      value.has_value()) {
    return ParseInt32(*value).value_or(0);
  }
  return 0;
}

bool MatchesMapTileset(const IniSection& current_section,
                       const DefaultDocumentMap& defaults, char main_ground) {
  const std::optional<std::string> tilesets =
      GetEffectiveScalarField("unit", current_section, defaults, "tilesets");
  if (!tilesets.has_value() || tilesets->empty()) {
    return false;
  }

  const std::string normalized_tilesets = Lower(*tilesets);
  if (normalized_tilesets == "*") {
    return true;
  }
  if (main_ground == '\0') {
    return true;
  }
  const char lowered_main_ground =
      static_cast<char>(std::tolower(static_cast<unsigned char>(main_ground)));
  return normalized_tilesets.find(lowered_main_ground) != std::string::npos;
}

bool IsMarketplaceSection(const IniSection& section,
                          const DefaultDocumentMap& defaults) {
  if (NormalizeIdentifier(ResolveSectionCode("unit", section, defaults)) ==
      NormalizeIdentifier("nmrk")) {
    return true;
  }
  if (const std::optional<std::string> internal_name =
          GetEffectiveScalarField("unit", section, defaults, "_name");
      internal_name.has_value()) {
    return NormalizeIdentifier(*internal_name) == "marketplace";
  }
  return false;
}

bool IsMarketplaceObjectId(
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, std::string_view id) {
  const auto doc_it = documents.find("unit");
  if (doc_it != documents.end() && doc_it->second.exists) {
    if (const IniSection* current = FindSectionById(doc_it->second.document, id);
        current != nullptr) {
      return IsMarketplaceSection(*current, defaults);
    }
  }
  if (const IniSection* base = LookupDefaultSection(defaults, "unit", id);
      base != nullptr) {
    return IsMarketplaceSection(*base, defaults);
  }
  return NormalizeIdentifier(id) == NormalizeIdentifier("nmrk");
}

const std::unordered_map<std::string, std::pair<std::string, std::string>>&
GetMustMarkTargets() {
  static const auto* must_mark =
      new std::unordered_map<std::string, std::pair<std::string, std::string>>{
          {NormalizeIdentifier("Asac"),
           {NormalizeIdentifier("ushd"), "unit"}},
          {NormalizeIdentifier("Alam"),
           {NormalizeIdentifier("ushd"), "unit"}},
          {NormalizeIdentifier("Aspa"),
           {NormalizeIdentifier("Bspa"), "buff"}},
          {NormalizeIdentifier("Amil"),
           {NormalizeIdentifier("Bmil"), "buff"}},
          {NormalizeIdentifier("AHav"),
           {NormalizeIdentifier("BHav"), "buff"}},
          {NormalizeIdentifier("Aphx"),
           {NormalizeIdentifier("Bphx"), "buff"}},
          {NormalizeIdentifier("Apxf"),
           {NormalizeIdentifier("Bpxf"), "buff"}},
          {NormalizeIdentifier("Auns"),
           {NormalizeIdentifier("Buns"), "buff"}},
          {NormalizeIdentifier("Asta"),
           {NormalizeIdentifier("Bstt"), "buff"}},
          {NormalizeIdentifier("Achd"),
           {NormalizeIdentifier("Bchd"), "buff"}},
          {NormalizeIdentifier("Aoar"),
           {NormalizeIdentifier("Boar"), "buff"}},
          {NormalizeIdentifier("Aarm"),
           {NormalizeIdentifier("Barm"), "buff"}},
      };
  return *must_mark;
}

void DrainMarkedDependencies(
    std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, const SearchMap& search_map,
    std::vector<CurrentObjectNode>& nodes,
    const std::unordered_map<std::string, std::size_t>& node_lookup,
    std::queue<std::size_t>& pending) {
  while (!pending.empty()) {
    const std::size_t node_index = pending.front();
    pending.pop();
    const CurrentObjectNode& node = nodes[node_index];
    const IniSection* section = ResolveCurrentNodeSection(documents, node);
    if (section == nullptr || section->removed) {
      continue;
    }

    const IniSection* default_section =
        ResolveDefaultNodeSection(documents, defaults, node);
    if (!node.parent_id.empty()) {
      const auto parent_it =
          node_lookup.find(MakeNodeKey(node.type, node.parent_id));
      if (parent_it != node_lookup.end()) {
        MarkNode(parent_it->second, nodes, pending);
      }
    }

    for (const std::string& relation_key :
         {NormalizeIdentifier(node.type), node.code}) {
      if (relation_key.empty()) {
        continue;
      }
      const auto relation_it = search_map.find(relation_key);
      if (relation_it == search_map.end()) {
        continue;
      }
      for (const SearchEntry& relation : relation_it->second) {
        const IniEntry* entry =
            FindEffectiveEntry(*section, default_section, relation.field);
        if (entry == nullptr) {
          continue;
        }
        const std::vector<std::string> ids = ExtractReferenceIds(*entry);
        for (const std::string& id : ids) {
          const std::string normalized_id = NormalizeIdentifier(id);
          if (normalized_id.empty()) {
            continue;
          }
          for (const std::string& target_type : relation.target_types) {
            const auto target_it =
                node_lookup.find(MakeNodeKey(target_type, normalized_id));
            if (target_it != node_lookup.end()) {
              MarkNode(target_it->second, nodes, pending);
            }
          }
        }
      }
    }

    if (const auto it = GetMustMarkTargets().find(node.code);
        it != GetMustMarkTargets().end()) {
      const auto target_it =
          node_lookup.find(MakeNodeKey(it->second.second, it->second.first));
      if (target_it != node_lookup.end()) {
        MarkNode(target_it->second, nodes, pending);
      }
    }
  }
}

void ApplyRemoveUnusedObjects(
    std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, const SearchMap& search_map,
    const JassSearchResult& jass_result, const DooSearchResult& doo_result,
    char main_ground) {
  std::vector<CurrentObjectNode> nodes;
  std::unordered_map<std::string, std::size_t> node_lookup;

  for (std::string_view type : kObjectTypes) {
    auto it = documents.find(std::string(type));
    if (it == documents.end() || !it->second.exists) {
      continue;
    }
    for (std::size_t i = 0; i < it->second.document.sections.size(); ++i) {
      const IniSection& section = it->second.document.sections[i];
      if (section.removed) {
        continue;
      }
      const bool is_custom =
          GetDecodedScalarField(section, "_parent").has_value() ||
          LookupDefaultSection(defaults, type, section.name) == nullptr;
      CurrentObjectNode node;
      node.type = std::string(type);
      node.section_index = i;
      node.id = section.name;
      node.normalized_id = NormalizeIdentifier(section.name);
      node.parent_id = NormalizeIdentifier(
          ResolveSectionParent(type, section, defaults));
      node.code = NormalizeIdentifier(ResolveSectionCode(type, section, defaults));
      node.is_custom = is_custom;
      node_lookup.emplace(MakeNodeKey(type, node.normalized_id), nodes.size());
      nodes.push_back(std::move(node));
    }
  }

  std::queue<std::size_t> pending;
  for (const std::string& id : jass_result.ids) {
    for (std::string_view type : kObjectTypes) {
      const auto it = node_lookup.find(MakeNodeKey(type, id));
      if (it != node_lookup.end()) {
        MarkNode(it->second, nodes, pending);
      }
    }
  }

  for (const std::string& id : doo_result.destructable_ids) {
    for (std::string_view type : {"destructable", "doodad"}) {
      const auto it = node_lookup.find(MakeNodeKey(type, id));
      if (it != node_lookup.end()) {
        MarkNode(it->second, nodes, pending);
      }
    }
  }
  for (const std::string& id : doo_result.doodad_ids) {
    const auto it = node_lookup.find(MakeNodeKey("doodad", id));
    if (it != node_lookup.end()) {
      MarkNode(it->second, nodes, pending);
    }
  }

  for (std::size_t i = 0; i < nodes.size(); ++i) {
    const CurrentObjectNode& node = nodes[i];
    const IniSection* section = ResolveCurrentNodeSection(documents, node);
    if (section == nullptr || section->removed) {
      continue;
    }

    if (node.type == "unit" &&
        NormalizeIdentifier(
            GetEffectiveScalarField("unit", *section, defaults, "race")
                .value_or("")) == "creeps" &&
        MatchesMapTileset(*section, defaults, main_ground)) {
      const int is_building =
          GetEffectiveIntegerField("unit", *section, defaults, "isbldg");
      const int special =
          GetEffectiveIntegerField("unit", *section, defaults, "special");
      const int neutral_random =
          GetEffectiveIntegerField("unit", *section, defaults, "nbrandom");
      if (jass_result.creeps && is_building == 0 && special == 0) {
        MarkNode(i, nodes, pending);
      }
      if (jass_result.building && is_building == 1 && neutral_random == 1) {
        MarkNode(i, nodes, pending);
      }
    }

    if (node.type == "item" && jass_result.item &&
        GetEffectiveIntegerField("item", *section, defaults, "pickrandom") ==
            1) {
      MarkNode(i, nodes, pending);
    }
  }

  DrainMarkedDependencies(documents, defaults, search_map, nodes, node_lookup,
                          pending);

  if (jass_result.marketplace && !jass_result.item) {
    bool uses_marketplace = false;
    for (const std::string& id : jass_result.ids) {
      if (IsMarketplaceObjectId(documents, defaults, id)) {
        uses_marketplace = true;
        break;
      }
    }
    if (!uses_marketplace) {
      for (const CurrentObjectNode& node : nodes) {
        if (node.type != "unit" || !node.marked) {
          continue;
        }
        const IniSection* section = ResolveCurrentNodeSection(documents, node);
        if (section != nullptr && IsMarketplaceSection(*section, defaults)) {
          uses_marketplace = true;
          break;
        }
      }
    }

    if (uses_marketplace) {
      for (std::size_t i = 0; i < nodes.size(); ++i) {
        const CurrentObjectNode& node = nodes[i];
        if (node.type != "item") {
          continue;
        }
        const IniSection* section = ResolveCurrentNodeSection(documents, node);
        if (section == nullptr || section->removed) {
          continue;
        }
        if (GetEffectiveIntegerField("item", *section, defaults, "pickrandom") ==
                1 &&
            GetEffectiveIntegerField("item", *section, defaults, "sellable") ==
                1) {
          MarkNode(i, nodes, pending);
        }
      }
    }
    DrainMarkedDependencies(documents, defaults, search_map, nodes, node_lookup,
                            pending);
  }

  std::unordered_map<std::string, std::unordered_set<std::string>> deleted_ids;
  for (CurrentObjectNode& node : nodes) {
    if (!node.is_custom || node.marked) {
      continue;
    }
    const bool removable =
        std::ranges::find(kRemovableUnusedTypes, node.type) !=
        kRemovableUnusedTypes.end();
    if (!removable) {
      continue;
    }

    LoadedDocument& document = documents.at(node.type);
    IniSection& section = document.document.sections[node.section_index];
    if (!section.removed) {
      section.removed = true;
      document.dirty = true;
      deleted_ids[node.type].insert(node.normalized_id);
    }
  }

  auto txt_it = documents.find("txt");
  if (txt_it == documents.end() || !txt_it->second.exists) {
    return;
  }
  for (IniSection& section : txt_it->second.document.sections) {
    if (section.removed) {
      continue;
    }
    const IniEntry* skin_type_entry = FindEntryByKey(section, "skintype");
    if (skin_type_entry == nullptr) {
      continue;
    }
    const ParsedValue skin_types = ParseValue(skin_type_entry->value);
    const std::string object_id = NormalizeIdentifier(
        GetDecodedScalarField(section, "skinnableid").value_or(section.name));
    if (object_id.empty()) {
      continue;
    }
    bool should_remove = false;
    for (const std::string& skin_type : skin_types.values) {
      const auto deleted_it = deleted_ids.find(NormalizeIdentifier(skin_type));
      if (deleted_it != deleted_ids.end() &&
          deleted_it->second.contains(object_id)) {
        should_remove = true;
        break;
      }
    }
    if (should_remove) {
      section.removed = true;
      txt_it->second.dirty = true;
    }
  }
}

void ApplyTxtKeyCleanup(std::unordered_map<std::string, LoadedDocument>& documents,
                        const DefaultDocumentMap& defaults,
                        const MetadataMap& metadata) {
  auto txt_it = documents.find("txt");
  if (txt_it == documents.end() || !txt_it->second.exists) {
    return;
  }

  std::unordered_map<std::string, std::unordered_set<std::string>>
      removable_keys_by_type;
  for (std::string_view type : kObjectTypes) {
    const auto meta_it = metadata.find(std::string(type));
    if (meta_it == metadata.end()) {
      continue;
    }
    std::unordered_set<std::string>& removable =
        removable_keys_by_type[std::string(type)];
    for (const auto& [_, field] : meta_it->second) {
      if (field.key.empty()) {
        continue;
      }
      std::string normalized_key = NormalizeIdentifier(field.key);
      const std::size_t colon_pos = normalized_key.find(':');
      if (colon_pos != std::string::npos) {
        normalized_key.resize(colon_pos);
      }
      if (!normalized_key.empty()) {
        removable.insert(std::move(normalized_key));
      }
    }
  }

  bool changed = false;
  for (IniSection& section : txt_it->second.document.sections) {
    if (section.removed) {
      continue;
    }

    const IniEntry* skin_type_entry = FindEntryByKey(section, "skintype");
    if (skin_type_entry == nullptr) {
      continue;
    }
    const ParsedValue skin_types = ParseValue(skin_type_entry->value);
    const std::string object_id = NormalizeIdentifier(
        GetDecodedScalarField(section, "skinnableid").value_or(section.name));
    if (object_id.empty()) {
      continue;
    }

    std::unordered_set<std::string> removable_keys;
    for (const std::string& skin_type : skin_types.values) {
      const std::string normalized_type = NormalizeIdentifier(skin_type);
      const auto doc_it = documents.find(normalized_type);
      const bool exists_in_current =
          doc_it != documents.end() && doc_it->second.exists &&
          FindSectionById(doc_it->second.document, object_id) != nullptr;
      const bool exists_in_defaults =
          LookupDefaultSection(defaults, normalized_type, object_id) != nullptr;
      if (!exists_in_current && !exists_in_defaults) {
        continue;
      }
      if (const auto remove_it = removable_keys_by_type.find(normalized_type);
          remove_it != removable_keys_by_type.end()) {
        removable_keys.insert(remove_it->second.begin(), remove_it->second.end());
      }
    }
    if (removable_keys.empty()) {
      continue;
    }

    for (IniEntry& entry : section.entries) {
      if (entry.removed) {
        continue;
      }
      const std::string normalized_key = NormalizeIdentifier(entry.key);
      if (!IsTextContentKey(normalized_key)) {
        continue;
      }
      std::string origin_key = normalized_key;
      const std::size_t colon_pos = origin_key.find(':');
      if (colon_pos != std::string::npos) {
        origin_key.resize(colon_pos);
      }
      if (removable_keys.contains(origin_key)) {
        entry.removed = true;
        changed = true;
      }
    }

    if (!HasMeaningfulSectionContent("txt", section)) {
      section.removed = true;
      changed = true;
    }
  }

  txt_it->second.dirty = txt_it->second.dirty || changed;
}

struct LookupObject {
  std::string type;
  const IniSection* current = nullptr;
  const IniSection* defaults = nullptr;
};

LookupObject ResolveLookupObject(
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, std::string_view id) {
  const std::string normalized_id = NormalizeIdentifier(id);
  for (std::string_view type : {"ability", "unit", "item", "upgrade"}) {
    const auto doc_it = documents.find(std::string(type));
    if (doc_it != documents.end() && doc_it->second.exists) {
      if (const IniSection* current =
              FindSectionById(doc_it->second.document, normalized_id);
          current != nullptr) {
        return LookupObject{
            .type = std::string(type),
            .current = current,
            .defaults = ResolveDefaultSectionForCurrent(type, *current, defaults),
        };
      }
    }
    if (const IniSection* base = LookupDefaultSection(defaults, type, normalized_id);
        base != nullptr) {
      return LookupObject{
          .type = std::string(type),
          .current = nullptr,
          .defaults = base,
      };
    }
  }
  return LookupObject{};
}

int ResolveObjectMaxLevel(const LookupObject& object) {
  for (const IniSection* section : {object.current, object.defaults}) {
    if (section == nullptr) {
      continue;
    }
    if (const IniEntry* entry = FindEntryByKey(*section, "_max_level");
        entry != nullptr) {
      if (const auto parsed = ParseInt32(entry->value); parsed.has_value()) {
        return std::max(1, parsed.value());
      }
    }
  }
  return 1;
}

std::optional<ParsedValue> GetEffectiveField(const LookupObject& object,
                                             std::string_view key) {
  for (const IniSection* section : {object.current, object.defaults}) {
    if (section == nullptr) {
      continue;
    }
    if (const IniEntry* entry = FindEntryByKey(*section, key); entry != nullptr) {
      return ParseValue(entry->value);
    }
  }
  return std::nullopt;
}

std::optional<std::string> ResolveComputedSourceValue(const LookupObject& object,
                                                      std::string_view key) {
  if (object.type.empty()) {
    return std::nullopt;
  }

  if (const std::optional<ParsedValue> exact = GetEffectiveField(object, key);
      exact.has_value() && !exact->is_list && !exact->values.empty()) {
    return exact->values.front();
  }

  const std::string normalized_key = NormalizeIdentifier(key);
  std::size_t suffix_pos = normalized_key.size();
  while (suffix_pos > 0 &&
         std::isdigit(static_cast<unsigned char>(normalized_key[suffix_pos - 1]))) {
    --suffix_pos;
  }
  if (suffix_pos == normalized_key.size()) {
    return std::nullopt;
  }

  const std::string base_key = normalized_key.substr(0, suffix_pos);
  const auto level = ParseInt32(normalized_key.substr(suffix_pos));
  if (!level.has_value()) {
    return std::nullopt;
  }

  const std::optional<ParsedValue> field = GetEffectiveField(object, base_key);
  if (!field.has_value() || !field->is_list) {
    return std::nullopt;
  }
  if (level.value() > ResolveObjectMaxLevel(object)) {
    return std::string("0");
  }
  const std::size_t index =
      static_cast<std::size_t>(std::max(1, level.value()) - 1);
  if (index >= field->values.size()) {
    return std::nullopt;
  }
  return field->values[index];
}

std::optional<double> ResolveComputedNumericValue(
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults, std::string_view id,
    std::string_view key) {
  const LookupObject object = ResolveLookupObject(documents, defaults, id);
  if (object.type.empty()) {
    return std::nullopt;
  }

  const std::string normalized_key = NormalizeIdentifier(key);
  const auto read_numeric = [&](std::string_view field) -> std::optional<double> {
    const auto value = ResolveComputedSourceValue(object, field);
    if (!value.has_value()) {
      return std::nullopt;
    }
    return ParseDouble(*value);
  };

  if (normalized_key == "mindmg1") {
    const auto dmgplus = read_numeric("dmgplus1");
    const auto dice = read_numeric("dice1");
    if (!dmgplus.has_value() || !dice.has_value()) {
      return std::nullopt;
    }
    return dmgplus.value() + dice.value();
  }
  if (normalized_key == "maxdmg1") {
    const auto dmgplus = read_numeric("dmgplus1");
    const auto dice = read_numeric("dice1");
    const auto sides = read_numeric("sides1");
    if (!dmgplus.has_value() || !dice.has_value() || !sides.has_value()) {
      return std::nullopt;
    }
    return dmgplus.value() + dice.value() * sides.value();
  }
  if (normalized_key == "mindmg2") {
    const auto dmgplus = read_numeric("dmgplus2");
    const auto dice = read_numeric("dice2");
    if (!dmgplus.has_value() || !dice.has_value()) {
      return std::nullopt;
    }
    return dmgplus.value() + dice.value();
  }
  if (normalized_key == "maxdmg2") {
    const auto dmgplus = read_numeric("dmgplus2");
    const auto dice = read_numeric("dice2");
    const auto sides = read_numeric("sides2");
    if (!dmgplus.has_value() || !dice.has_value() || !sides.has_value()) {
      return std::nullopt;
    }
    return dmgplus.value() + dice.value() * sides.value();
  }
  if (normalized_key == "realhp") {
    return read_numeric("hp");
  }

  const auto value = ResolveComputedSourceValue(object, normalized_key);
  if (!value.has_value()) {
    return std::nullopt;
  }
  return ParseDouble(*value);
}

std::string FormatComputedNumber(double value) {
  const long long integer_like =
      static_cast<long long>(std::floor(value + 0.00005));
  return std::to_string(integer_like);
}

std::string ApplyComputedText(
    std::string input,
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults) {
  std::string output;
  output.reserve(input.size());

  std::size_t pos = 0;
  while (pos < input.size()) {
    const std::size_t open = input.find('<', pos);
    if (open == std::string::npos) {
      output.append(input.substr(pos));
      break;
    }
    output.append(input.substr(pos, open - pos));
    const std::size_t close = input.find('>', open + 1);
    if (close == std::string::npos) {
      output.append(input.substr(open));
      break;
    }

    const std::string placeholder = input.substr(open + 1, close - open - 1);
    const std::vector<std::string> parts = SplitSimpleCsv(placeholder);
    if (parts.size() < 2) {
      output.append(input.substr(open, close - open + 1));
      pos = close + 1;
      continue;
    }

    auto numeric_value = ResolveComputedNumericValue(documents, defaults, parts[0],
                                                     parts[1]);
    if (!numeric_value.has_value()) {
      output.append(input.substr(open, close - open + 1));
      pos = close + 1;
      continue;
    }
    if (parts.size() >= 3 && parts[2] == "%") {
      numeric_value = numeric_value.value() * 100.0;
    }
    output += FormatComputedNumber(numeric_value.value());
    pos = close + 1;
  }

  return output;
}

bool ApplyComputedField(
    IniSection& section, std::string_view key,
    const std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults) {
  IniEntry* entry = FindEntryByKey(section, key);
  if (entry == nullptr) {
    return false;
  }

  ParsedValue parsed = ParseValue(entry->value);
  bool changed = false;
  for (std::string& value : parsed.values) {
    const std::string computed = ApplyComputedText(value, documents, defaults);
    if (computed != value) {
      value = computed;
      changed = true;
    }
  }
  if (!changed) {
    return false;
  }

  entry->value = RenderValue(parsed);
  return true;
}

void ApplyComputedTextPass(
    std::unordered_map<std::string, LoadedDocument>& documents,
    const DefaultDocumentMap& defaults) {
  struct ComputedSpec {
    std::string_view type;
    std::array<std::string_view, 2> fields;
    std::size_t field_count;
  };

  constexpr std::array<ComputedSpec, 3> kSpecs = {{
      {"ability", {"researchubertip", "ubertip"}, 2},
      {"item", {"ubertip", "description"}, 2},
      {"upgrade", {"ubertip", ""}, 1},
  }};

  for (const ComputedSpec& spec : kSpecs) {
    auto doc_it = documents.find(std::string(spec.type));
    if (doc_it == documents.end() || !doc_it->second.exists) {
      continue;
    }
    for (IniSection& section : doc_it->second.document.sections) {
      if (section.removed) {
        continue;
      }
      bool section_changed = false;
      for (std::size_t i = 0; i < spec.field_count; ++i) {
        section_changed = ApplyComputedField(section, spec.fields[i], documents,
                                             defaults) ||
                          section_changed;
      }
      doc_it->second.dirty = doc_it->second.dirty || section_changed;
    }
  }
}

}  // namespace

core::Result<void> ApplyContentCleanupPasses(
    const std::filesystem::path& input_dir, const PackOptions& options) {
  if (options.profile != PackProfile::kSlk) {
    return {};
  }
  if (!options.remove_same && !options.remove_unused_objects &&
      !options.computed_text) {
    return {};
  }

  W3X_ASSIGN_OR_RETURN(const fs::path prebuilt_root, ResolvePrebuiltRoot());

  DefaultDocumentMap defaults;
  MetadataMap metadata;
  SearchMap search_map;
  W3X_RETURN_IF_ERROR(LoadDefaultDocuments(prebuilt_root, defaults));
  W3X_RETURN_IF_ERROR(LoadMetadata(prebuilt_root / "metadata.ini", metadata));
  W3X_RETURN_IF_ERROR(
      LoadSearchMetadata(prebuilt_root / "search.ini", search_map));

  std::unordered_map<std::string, LoadedDocument> documents;
  for (const auto& [type, filename] : kSourceFiles) {
    W3X_ASSIGN_OR_RETURN(
        auto loaded, LoadInputDocument(input_dir, type, filename));
    if (loaded.exists) {
      documents.emplace(std::string(type), std::move(loaded));
    }
  }

  if (options.remove_unused_objects) {
    W3X_ASSIGN_OR_RETURN(auto jass_result, LoadJassSearchResult(input_dir));
    W3X_ASSIGN_OR_RETURN(auto doo_result, LoadDooSearchResult(input_dir));
    W3X_ASSIGN_OR_RETURN(const char main_ground, LoadMapMainGround(input_dir));
    ApplyRemoveUnusedObjects(documents, defaults, search_map, jass_result,
                             doo_result, main_ground);
  }

  if (options.computed_text) {
    ApplyComputedTextPass(documents, defaults);
  }

  if (options.remove_same) {
    for (const auto& [type, _] : kSourceFiles) {
      auto it = documents.find(std::string(type));
      if (it == documents.end()) {
        continue;
      }
      ApplyRemoveSameToDocument(it->second, defaults, metadata);
    }
    ApplyTxtKeyCleanup(documents, defaults, metadata);
  }

  for (const auto& [type, _] : kSourceFiles) {
    const auto it = documents.find(std::string(type));
    if (it == documents.end()) {
      continue;
    }
    W3X_RETURN_IF_ERROR(SaveDocument(it->second));
  }

  return {};
}

}  // namespace w3x_toolkit::parser::w3x
