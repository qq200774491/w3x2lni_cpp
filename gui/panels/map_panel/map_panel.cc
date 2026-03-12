#include "gui/panels/map_panel/map_panel.h"

#include <algorithm>
#include <set>
#include <string>

namespace w3x_toolkit::gui {

MapPanel::MapPanel() : Panel("Map") {}

void MapPanel::SetMapInfo(const MapInfo& info) { map_info_ = info; }

void MapPanel::SetFileEntries(std::vector<FileEntry> entries) {
  file_entries_ = std::move(entries);
}

void MapPanel::RenderContents() {
  if (ImGui::BeginTabBar("MapTabs")) {
    if (ImGui::BeginTabItem("Info")) {
      RenderMapInfo();
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Files")) {
      RenderFileTree();
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
}

void MapPanel::RenderMapInfo() {
  ImGui::TextWrapped("Map Information (W3I)");
  ImGui::Separator();

  if (map_info_.name.empty()) {
    ImGui::TextDisabled("No map loaded.");
    return;
  }

  ImGui::Text("Name: %s", map_info_.name.c_str());
  ImGui::Text("Author: %s", map_info_.author.c_str());

  ImGui::Spacing();
  ImGui::Text("Description:");
  ImGui::TextWrapped("%s", map_info_.description.c_str());

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Text("Suggested Players: %s", map_info_.suggested_players.c_str());
  ImGui::Text("Player Count: %d", map_info_.player_count);
  ImGui::Text("Force Count: %d", map_info_.force_count);
  ImGui::Text("Map Size: %d x %d", map_info_.map_width, map_info_.map_height);
}

void MapPanel::RenderFileTree() {
  ImGui::Text("Archive Files (%zu entries)", file_entries_.size());
  ImGui::Separator();

  ImGui::InputTextWithHint("##FileFilter", "Filter files...", search_buf_,
                           sizeof(search_buf_));

  ImGui::BeginChild("FileTreeScroll", ImVec2(0, 0), ImGuiChildFlags_Borders);

  if (file_entries_.empty()) {
    ImGui::TextDisabled("No files loaded.");
    ImGui::EndChild();
    return;
  }

  std::string filter(search_buf_);

  // Collect unique top-level directories.
  std::set<std::string> root_dirs;
  for (const auto& entry : file_entries_) {
    if (!filter.empty()) {
      if (entry.path.find(filter) == std::string::npos) continue;
    }
    if (!entry.directory.empty()) {
      // Extract root directory (first path component).
      auto slash = entry.directory.find_first_of("/\\");
      std::string root =
          (slash != std::string::npos)
              ? entry.directory.substr(0, slash)
              : entry.directory;
      root_dirs.insert(root);
    }
  }

  // Render tree nodes per root directory.
  for (const auto& dir : root_dirs) {
    RenderDirectoryNode(dir);
  }

  // Render root-level files (no directory).
  for (const auto& entry : file_entries_) {
    if (!entry.directory.empty()) continue;
    if (!filter.empty() &&
        entry.path.find(filter) == std::string::npos) {
      continue;
    }
    ImGui::TreeNodeEx(entry.filename.c_str(),
                      ImGuiTreeNodeFlags_Leaf |
                          ImGuiTreeNodeFlags_NoTreePushOnOpen);
    if (ImGui::IsItemHovered()) {
      ImGui::SetTooltip("Size: %zu / %zu bytes", entry.compressed_size,
                        entry.original_size);
    }
  }

  ImGui::EndChild();
}

void MapPanel::RenderDirectoryNode(const std::string& directory) {
  if (!ImGui::TreeNode(directory.c_str())) return;

  std::string filter(search_buf_);
  std::set<std::string> sub_dirs;

  for (const auto& entry : file_entries_) {
    if (!filter.empty() &&
        entry.path.find(filter) == std::string::npos) {
      continue;
    }
    // Check if this entry belongs to the given directory.
    if (entry.directory == directory) {
      ImGui::TreeNodeEx(entry.filename.c_str(),
                        ImGuiTreeNodeFlags_Leaf |
                            ImGuiTreeNodeFlags_NoTreePushOnOpen);
      if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Size: %zu / %zu bytes", entry.compressed_size,
                          entry.original_size);
      }
    } else if (entry.directory.starts_with(directory + "/") ||
               entry.directory.starts_with(directory + "\\")) {
      // Collect immediate sub-directories.
      auto rest = entry.directory.substr(directory.size() + 1);
      auto slash = rest.find_first_of("/\\");
      std::string sub =
          directory + "/" +
          ((slash != std::string::npos) ? rest.substr(0, slash) : rest);
      sub_dirs.insert(sub);
    }
  }

  for (const auto& sub : sub_dirs) {
    RenderDirectoryNode(sub);
  }

  ImGui::TreePop();
}

}  // namespace w3x_toolkit::gui
