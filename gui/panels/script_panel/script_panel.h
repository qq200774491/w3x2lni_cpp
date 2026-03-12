#ifndef W3X_TOOLKIT_GUI_SCRIPT_PANEL_H_
#define W3X_TOOLKIT_GUI_SCRIPT_PANEL_H_

#include <string>
#include <unordered_set>
#include <vector>

#include "imgui.h"

#include "gui/panels/panel.h"

namespace w3x_toolkit::gui {

// Panel that displays JASS / Lua script source code with basic syntax
// highlighting, line numbers, and a search bar.
class ScriptPanel : public Panel {
 public:
  ScriptPanel();

  // Loads script text for display.
  void SetScript(const std::string& name, const std::string& source);

 protected:
  void RenderContents() override;

 private:
  // Draws the search bar (Ctrl+F).
  void RenderSearchBar();

  // Draws the script text area with line numbers and highlighting.
  void RenderTextArea();

  // Returns the color for a token based on JASS/Lua keywords.
  static ImVec4 GetTokenColor(const std::string& token);

  // Splits the source into individual lines.
  void RebuildLines();

  std::string script_name_;
  std::string source_;
  std::vector<std::string> lines_;

  // Search state.
  bool show_search_ = false;
  char search_buf_[256] = {};
  int search_match_index_ = -1;

  // Keyword sets initialised once.
  static const std::unordered_set<std::string>& JassKeywords();
  static const std::unordered_set<std::string>& JassTypes();
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_SCRIPT_PANEL_H_
