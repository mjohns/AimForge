#include "guide_ui.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/proto_util.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/guide_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/proto/guide.pb.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/ui_app.h"
#include "imgui.h"

namespace aim {
namespace {

class GuideEditor {
 public:
  GuideEditor(const GuideItem& original_guide)
      : original_guide_(original_guide), updated_guide_(original_guide) {}

  void Draw() {
    ImGui::IdGuard cid("GuideEditor");

    ImGui::LoopId loop_id;
    GuideDef& def = updated_guide_.def;

    int delete_i = -1;
    int copy_i = -1;
    int move_up_i = -1;
    int move_down_i = -1;
    int insert_at_i = -1;

    for (int i = 0; i < def.sections_size(); ++i) {
      auto lid = loop_id.Get("Section");
      GuideSection* section = def.mutable_sections(i);

      ImGui::AlignTextToFramePadding();
      ImGui::TextFmt("Section {}", i + 1);

      const char* menu_id = "GuideSectionMenu";
      if (ImGui::BeginPopupContextItem(menu_id)) {
        if (ImGui::Selectable(std::format("{} Copy", icons::kContentCopy))) {
          copy_i = i;
        }
        if (ImGui::Selectable(std::format("{} Move up", icons::kArrowUpward))) {
          move_up_i = i;
        }
        if (ImGui::Selectable(std::format("{} Move down", icons::kArrowDownward))) {
          move_down_i = i;
        }
        ImGui::SpacedSeparator();
        if (ImGui::Selectable(std::format("{} Delete", icons::kDelete))) {
          delete_i = i;
        }

        ImGui::EndPopup();
      }

      ImGui::SameLine();
      if (ImGui::MenuButton()) {
        ImGui::OpenPopup(menu_id);
      }

      DrawSectionEditor(section);
    }

    if (ImGui::Button(std::format("{} Add section", icons::kAdd))) {
      updated_guide_.def.add_sections();
    }

    auto& sections = *def.mutable_sections();
    if (delete_i >= 0) {
      sections.erase(sections.begin() + delete_i);
    } else if (move_up_i > 0) {
      int i1 = move_up_i;
      int i2 = move_up_i - 1;
      std::swap(sections[i1], sections[i2]);
    } else if (move_down_i >= 0) {
      int i1 = move_down_i;
      int i2 = move_down_i + 1;
      if (i2 < sections.size()) {
        std::swap(sections[i1], sections[i2]);
      }
    } else if (copy_i >= 0) {
      InsertAtIndex(&sections, sections[copy_i], copy_i);
    }

    // ImGui::
  }

 private:
  void DrawSectionEditor(GuideSection* section) {
    ImGui::InputTextMultiline("##DescriptionInput",
                              section->mutable_text(),
                              ImVec2(0, 0),
                              ImGuiInputTextFlags_AllowTabInput);
  }

  Application& app_ = GetUiApp();
  const GuideItem original_guide_;
  GuideItem updated_guide_;
};

class GuidesComponentImpl : public GuidesComponent {
 public:
  GuidesComponentImpl() : app_(GetUiApp()), editor_({}) {
    auto* section = guide_.add_sections();
    // section->set_text("Example playlists");
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder";
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder 15cm";
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder 25cm";
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder 35cm";
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder 45cm";
    *section->add_playlists() = "AF SmoothSwerve Precision Ladder 55cm";

    playlist_component_ = CreatePlaylistComponent();
  }

  void SetPlaylistRunIfInGuide() {
    run_ = {};
    auto current_run = app_.playlist_manager().GetCurrentRun();
    if (!current_run) {
      return;
    }
    for (const auto& section : guide_.sections()) {
      for (const std::string& playlist : section.playlists()) {
        if (playlist == current_run->playlist.name) {
          run_ = current_run;
          return;
        }
      }
    }
  }

  void Show() override {
    SetPlaylistRunIfInGuide();

    editor_.Draw();

    // ImGui::BeginChild("GuideContainer", ImVec2(0, 0));
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("GuideColumns", 3, flags)) {
      return;
    }

    ImGui::TableNextColumn();
    ImGui::BeginChild("GuideColumn");
    ImGui::LoopId loop_id;
    for (const auto& section : guide_.sections()) {
      auto lid = loop_id.Get("Section");
      DrawSection(section);
    }
    ImGui::EndChild();

    ImGui::TableNextColumn();
    ImGui::BeginChild("PlaylistColumn");
    if (run_) {
      playlist_component_->Show(run_, /*is_playlist_screen*/ false);
    }
    ImGui::EndChild();

    ImGui::EndTable();
  }

  void DrawSection(const GuideSection& section) {
    if (!section.text().empty()) {
      ImGui::TextWrapped(section.text());
    }
    if (section.playlists_size() > 0) {
      DrawPlaylists(section);
    }
  }

  void DrawPlaylists(const GuideSection& section) {
    ImGuiTableFlags flags =
        ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersV | ImGuiTableFlags_Borders;
    if (!ImGui::BeginTable("Playlists", 2, flags)) {
      return;
    }
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);

    // TODO: Only add this column if there is actually a level to show
    float level_width = ImGui::CalcTextSize("L22.5_").x;
    ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, level_width);

    ImGui::LoopId loop_id;
    for (const std::string& playlist : section.playlists()) {
      ImGui::TableNextRow();
      auto lid = loop_id.Get("Playlist");
      ImGui::TableNextColumn();
      bool is_selected = run_ && playlist == run_->playlist.name;
      if (ImGui::Selectable(playlist, is_selected)) {
        app_.playlist_manager().SetCurrentPlaylist(playlist);
      }
      auto maybe_playlist = app_.playlist_manager().GetPlaylist(playlist);
      if (maybe_playlist) {
        auto highest_complete_level = app_.playlist_manager().GetHighestCompleteLevel(
            *maybe_playlist, app_.scenario_manager(), app_.stats_manager());
        if (highest_complete_level) {
          ImGui::TableNextColumn();
          ImGui::TextFmt("L{}{}", MaybeIntToString(*highest_complete_level, 1), icons::kVerified);
        }
      }
    }

    ImGui::EndTable();
  }

 private:
  Application& app_;

  GuideDef guide_;
  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::shared_ptr<PlaylistRun> run_;
  GuideEditor editor_;
};

}  // namespace

std::unique_ptr<GuidesComponent> CreateGuidesComponent() {
  return std::make_unique<GuidesComponentImpl>();
}

}  // namespace aim
