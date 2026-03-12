#ifndef W3X_TOOLKIT_GUI_MAP_PANEL_H_
#define W3X_TOOLKIT_GUI_MAP_PANEL_H_

#include <string>
#include <vector>

#include "imgui.h"

#include "gui/panels/panel.h"

namespace w3x_toolkit::gui {

// Holds basic W3I (map information) data for display.
struct MapInfo {
  std::string name;
  std::string author;
  std::string description;
  std::string suggested_players;
  int player_count = 0;
  int force_count = 0;
  int map_width = 0;
  int map_height = 0;
};

// A single entry in the map archive file listing.
struct FileEntry {
  std::string path;            // Full path inside the archive.
  std::string directory;       // Parent directory (for tree grouping).
  std::string filename;        // Leaf filename.
  std::size_t compressed_size = 0;
  std::size_t original_size = 0;
};

// Panel that displays map information and the archive file tree.
class MapPanel : public Panel {
 public:
  MapPanel();

  // Loads map information to display.
  void SetMapInfo(const MapInfo& info);

  // Loads the archive file listing.
  void SetFileEntries(std::vector<FileEntry> entries);

 protected:
  void RenderContents() override;

 private:
  // Renders the W3I information section.
  void RenderMapInfo();

  // Renders the file tree section.
  void RenderFileTree();

  // Recursive helper to build tree nodes by directory.
  void RenderDirectoryNode(const std::string& directory);

  MapInfo map_info_;
  std::vector<FileEntry> file_entries_;
  char search_buf_[256] = {};
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_MAP_PANEL_H_
