#include "gui/panels/log_panel/log_panel.h"

#include <algorithm>

#include "spdlog/pattern_formatter.h"

namespace w3x_toolkit::gui {

// ---------------------------------------------------------------------------
// ImGuiLogSink
// ---------------------------------------------------------------------------

void ImGuiLogSink::sink_it_(const spdlog::details::log_msg& msg) {
  // Format the message using the default formatter.
  spdlog::memory_buf_t formatted;
  spdlog::sinks::base_sink<std::mutex>::formatter_->format(msg, formatted);

  LogEntry entry;
  entry.level = msg.level;
  entry.message = std::string(formatted.data(), formatted.size());

  // Strip trailing newline if present.
  if (!entry.message.empty() && entry.message.back() == '\n') {
    entry.message.pop_back();
  }

  std::lock_guard<std::mutex> lock(data_mutex_);
  entries_.push_back(std::move(entry));
}

void ImGuiLogSink::flush_() {
  // Nothing to flush -- entries are stored in memory.
}

std::vector<LogEntry> ImGuiLogSink::GetEntries() const {
  std::lock_guard<std::mutex> lock(data_mutex_);
  return entries_;
}

void ImGuiLogSink::Clear() {
  std::lock_guard<std::mutex> lock(data_mutex_);
  entries_.clear();
}

// ---------------------------------------------------------------------------
// LogPanel
// ---------------------------------------------------------------------------

LogPanel::LogPanel()
    : Panel("Log"),
      sink_(std::make_shared<ImGuiLogSink>()) {}

std::shared_ptr<ImGuiLogSink> LogPanel::GetSink() const { return sink_; }

ImVec4 LogPanel::LevelColor(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace:
      return ImVec4(0.6f, 0.6f, 0.6f, 1.0f);   // Gray.
    case spdlog::level::debug:
      return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);   // White.
    case spdlog::level::info:
      return ImVec4(0.4f, 0.9f, 0.4f, 1.0f);   // Green.
    case spdlog::level::warn:
      return ImVec4(1.0f, 0.9f, 0.3f, 1.0f);   // Yellow.
    case spdlog::level::err:
      return ImVec4(1.0f, 0.3f, 0.3f, 1.0f);   // Red.
    case spdlog::level::critical:
      return ImVec4(1.0f, 0.2f, 0.2f, 1.0f);   // Bright red.
    default:
      return ImVec4(0.9f, 0.9f, 0.9f, 1.0f);
  }
}

const char* LogPanel::LevelLabel(spdlog::level::level_enum level) {
  switch (level) {
    case spdlog::level::trace:    return "[TRACE]";
    case spdlog::level::debug:    return "[DEBUG]";
    case spdlog::level::info:     return "[INFO] ";
    case spdlog::level::warn:     return "[WARN] ";
    case spdlog::level::err:      return "[ERROR]";
    case spdlog::level::critical: return "[CRIT] ";
    default:                      return "[???]  ";
  }
}

void LogPanel::RenderContents() {
  // Toolbar.
  if (ImGui::Button("Clear")) {
    sink_->Clear();
  }
  ImGui::SameLine();

  ImGui::Checkbox("Auto-scroll", &auto_scroll_);
  ImGui::SameLine();

  ImGui::SetNextItemWidth(100.0f);
  ImGui::Combo("##LogLevel", &filter_level_, kLevelNames,
               IM_ARRAYSIZE(kLevelNames));

  ImGui::Separator();

  // Log area.
  ImGui::BeginChild("LogScroll", ImVec2(0, 0), ImGuiChildFlags_Borders);

  auto entries = sink_->GetEntries();
  auto min_level = static_cast<spdlog::level::level_enum>(filter_level_);

  for (const auto& entry : entries) {
    if (entry.level < min_level) continue;

    ImVec4 color = LevelColor(entry.level);

    // Bold effect for critical.
    if (entry.level == spdlog::level::critical) {
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextUnformatted(LevelLabel(entry.level));
      ImGui::SameLine();
      ImGui::TextUnformatted(entry.message.c_str());
      ImGui::PopStyleColor();
    } else {
      ImGui::PushStyleColor(ImGuiCol_Text, color);
      ImGui::TextUnformatted(LevelLabel(entry.level));
      ImGui::SameLine();
      ImGui::TextUnformatted(entry.message.c_str());
      ImGui::PopStyleColor();
    }
  }

  if (auto_scroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
    ImGui::SetScrollHereY(1.0f);
  }

  ImGui::EndChild();
}

}  // namespace w3x_toolkit::gui
