#include "gui/panels/script_panel/script_panel.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace w3x_toolkit::gui {

// ---------------------------------------------------------------------------
// Keyword tables
// ---------------------------------------------------------------------------

const std::unordered_set<std::string>& ScriptPanel::JassKeywords() {
  static const std::unordered_set<std::string> kKeywords = {
      "function",  "endfunction", "returns",  "return",    "local",
      "set",       "call",        "if",       "then",      "else",
      "elseif",    "endif",       "loop",     "endloop",   "exitwhen",
      "globals",   "endglobals",  "constant", "native",    "type",
      "extends",   "array",       "takes",    "nothing",   "and",
      "or",        "not",         "true",     "false",     "null",
      // Lua keywords (for mixed maps).
      "do",        "end",         "while",    "for",       "in",
      "repeat",    "until",       "break",    "continue",  "goto",
      "local",     "nil",
  };
  return kKeywords;
}

const std::unordered_set<std::string>& ScriptPanel::JassTypes() {
  static const std::unordered_set<std::string> kTypes = {
      "integer", "real",   "boolean", "string", "handle",
      "code",    "unit",   "player",  "widget", "timer",
      "trigger", "effect", "group",   "force",  "rect",
      "region",  "void",
  };
  return kTypes;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ScriptPanel::ScriptPanel() : Panel("Script") {}

void ScriptPanel::SetScript(const std::string& name,
                            const std::string& source) {
  script_name_ = name;
  source_ = source;
  RebuildLines();
  search_match_index_ = -1;
}

void ScriptPanel::RebuildLines() {
  lines_.clear();
  std::istringstream stream(source_);
  std::string line;
  while (std::getline(stream, line)) {
    lines_.push_back(std::move(line));
  }
}

// ---------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------

void ScriptPanel::RenderContents() {
  // Header.
  if (script_name_.empty()) {
    ImGui::TextDisabled("No script loaded.");
    return;
  }

  ImGui::Text("File: %s", script_name_.c_str());
  ImGui::SameLine();
  ImGui::Text("(%zu lines)", lines_.size());

  // Ctrl+F toggle.
  ImGuiIO& io = ImGui::GetIO();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F)) {
    show_search_ = !show_search_;
  }

  if (show_search_) {
    RenderSearchBar();
  }

  ImGui::Separator();
  RenderTextArea();
}

void ScriptPanel::RenderSearchBar() {
  ImGui::PushID("ScriptSearch");

  ImGui::SetNextItemWidth(200.0f);
  bool changed =
      ImGui::InputTextWithHint("##Search", "Search...", search_buf_,
                               sizeof(search_buf_),
                               ImGuiInputTextFlags_EnterReturnsTrue);

  ImGui::SameLine();
  if (ImGui::Button("Find Next") || changed) {
    std::string needle(search_buf_);
    if (!needle.empty()) {
      int start = (search_match_index_ >= 0) ? search_match_index_ + 1 : 0;
      for (int i = 0; i < static_cast<int>(lines_.size()); ++i) {
        int idx = (start + i) % static_cast<int>(lines_.size());
        if (lines_[idx].find(needle) != std::string::npos) {
          search_match_index_ = idx;
          break;
        }
      }
    }
  }

  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    show_search_ = false;
  }

  ImGui::PopID();
}

ImVec4 ScriptPanel::GetTokenColor(const std::string& token) {
  if (JassKeywords().count(token)) {
    return ImVec4(0.4f, 0.6f, 1.0f, 1.0f);  // Blue for keywords.
  }
  if (JassTypes().count(token)) {
    return ImVec4(0.3f, 0.9f, 0.6f, 1.0f);  // Green for types.
  }
  // Check if it's a numeric literal.
  if (!token.empty() && (std::isdigit(static_cast<unsigned char>(token[0])) ||
                         (token[0] == '-' && token.size() > 1))) {
    return ImVec4(0.9f, 0.7f, 0.3f, 1.0f);  // Orange for numbers.
  }
  return ImVec4(0.86f, 0.86f, 0.86f, 1.0f);  // Default light grey.
}

void ScriptPanel::RenderTextArea() {
  ImGui::BeginChild("ScriptText", ImVec2(0, 0), ImGuiChildFlags_Borders,
                    ImGuiWindowFlags_HorizontalScrollbar);

  ImGuiListClipper clipper;
  clipper.Begin(static_cast<int>(lines_.size()));

  // If there is a search match, scroll to it.
  if (search_match_index_ >= 0) {
    float line_height = ImGui::GetTextLineHeightWithSpacing();
    ImGui::SetScrollY(
        static_cast<float>(search_match_index_) * line_height);
  }

  while (clipper.Step()) {
    for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; ++i) {
      const auto& line = lines_[i];

      // Highlight the search-matched line.
      bool is_match = (i == search_match_index_);
      if (is_match) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(1.0f, 1.0f, 0.0f, 1.0f));
      }

      // Line number (right-aligned in a fixed-width column).
      ImGui::Text("%5d ", i + 1);
      ImGui::SameLine();

      // Tokenise the line and colour each token.
      std::istringstream tokens(line);
      std::string token;
      bool first = true;

      // Handle comment lines.
      auto trimmed = line;
      auto pos = trimmed.find_first_not_of(" \t");
      if (pos != std::string::npos &&
          (trimmed.substr(pos, 2) == "//" ||
           trimmed.substr(pos, 2) == "--")) {
        ImGui::PushStyleColor(ImGuiCol_Text,
                              ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
        ImGui::TextUnformatted(line.c_str());
        ImGui::PopStyleColor();
      } else {
        // Simple word-by-word colouring.
        while (tokens >> token) {
          if (!first) {
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::TextUnformatted(" ");
            ImGui::SameLine(0.0f, 0.0f);
          }
          ImVec4 color = GetTokenColor(token);
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          ImGui::TextUnformatted(token.c_str());
          ImGui::PopStyleColor();
          first = false;
        }
        if (first) {
          // Empty line -- still need something so the line takes space.
          ImGui::TextUnformatted("");
        }
      }

      if (is_match) {
        ImGui::PopStyleColor();
      }
    }
  }

  ImGui::EndChild();
}

}  // namespace w3x_toolkit::gui
