#ifndef W3X_TOOLKIT_GUI_LOG_PANEL_H_
#define W3X_TOOLKIT_GUI_LOG_PANEL_H_

#include <mutex>
#include <string>
#include <vector>

#include "imgui.h"
#include "spdlog/sinks/base_sink.h"
#include "spdlog/spdlog.h"

#include "gui/panels/panel.h"

namespace w3x_toolkit::gui {

// A single log entry displayed in the panel.
struct LogEntry {
  spdlog::level::level_enum level = spdlog::level::info;
  std::string message;
};

// Custom spdlog sink that stores messages for display in the LogPanel.
// Thread-safe (spdlog calls sink_it_ under a mutex).
class ImGuiLogSink : public spdlog::sinks::base_sink<std::mutex> {
 public:
  // Returns all accumulated entries (copies under lock).
  std::vector<LogEntry> GetEntries() const;

  // Clears all stored entries.
  void Clear();

 protected:
  void sink_it_(const spdlog::details::log_msg& msg) override;
  void flush_() override;

 private:
  mutable std::mutex data_mutex_;
  std::vector<LogEntry> entries_;
};

// Panel that displays spdlog output in a scrollable, colour-coded view.
class LogPanel : public Panel {
 public:
  LogPanel();

  // Returns the sink to register with spdlog.
  std::shared_ptr<ImGuiLogSink> GetSink() const;

 protected:
  void RenderContents() override;

 private:
  // Returns the colour for a given log level.
  static ImVec4 LevelColor(spdlog::level::level_enum level);

  // Returns a short label for a log level.
  static const char* LevelLabel(spdlog::level::level_enum level);

  std::shared_ptr<ImGuiLogSink> sink_;
  bool auto_scroll_ = true;
  int filter_level_ = 0;  // Index into level_names_.

  static constexpr const char* kLevelNames[] = {
      "Trace", "Debug", "Info", "Warn", "Error", "Critical", "Off",
  };
};

}  // namespace w3x_toolkit::gui

#endif  // W3X_TOOLKIT_GUI_LOG_PANEL_H_
