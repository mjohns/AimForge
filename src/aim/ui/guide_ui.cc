#include "guide_ui.h"

#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/object_type.h"
#include "aim/common/proto_util.h"
#include "aim/common/resource_name.h"
#include "aim/common/util.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/guide_manager.h"
#include "aim/core/history_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/proto/guide.pb.h"
#include "aim/ui/playlist_ui.h"
#include "aim/ui/search_selector.h"
#include "aim/ui/ui_app.h"
#include "aim/ui/object_browser.h"
#include "imgui.h"

namespace aim {
namespace {

class AddGuideDialog {
 public:
  explicit AddGuideDialog(const std::string& id) : id_(id) {}

  void NotifyOpen() {
    open_ = true;
  }

  bool Draw(Application& app) {
    ImGui::IdGuard cid("AddGuideDialogContent");
    bool did_add = false;
    if (is_open_) {
      if (ImGui::BeginDefaultPopupModal(id_.c_str(), &is_open_)) {
        ImGui::SimpleDropdown("BundlePicker",
                              name_.mutable_bundle_name(),
                              bundle_names_,
                              ImGui::GetFrameHeight() * 9);
        ImGui::SameLine();
        ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

        ImGui::Spacing();
        if (ImGui::Button("Add")) {
          auto taken_names = app.guide_manager().GetAllRelativeNamesInBundle(name_.bundle_name());
          *name_.mutable_relative_name() = MakeUniqueName(name_.relative_name(), taken_names);
          app.guide_manager().UpdateGuide(name_.full_name(), GuideDef());
          // app.guide_manager().SetCurrentGuide(name_.full_name());
          app.history_manager().UpdateRecentView(ObjectType::GUIDE, name_.full_name());
          did_add = true;
          ImGui::CloseCurrentPopup();
          is_open_ = false;
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
          is_open_ = false;
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
    }
    if (open_) {
      ImGui::OpenPopup(id_.c_str());
      open_ = false;
      is_open_ = true;
      bundle_names_ = app.bundle_manager().GetWritableBundleNames();
      name_.set(kUserBundleName, "New guide");
    }
    return did_add;
  }

 private:
  bool open_ = false;
  bool is_open_ = false;

  ResourceName name_;
  std::vector<std::string> bundle_names_;
  std::string id_;
};

class GuideEditor {
 public:
  GuideEditor(const GuideItem& original_guide)
      : original_guide_(original_guide), updated_guide_(original_guide) {}

  void Draw() {
    ImGui::IdGuard cid("GuideEditor");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

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

      ImGui::Indent();
      ImGui::SameLine();
      if (ImGui::MenuButton()) {
        ImGui::OpenPopup(menu_id);
      }

      if (i != 0) {
        ImGui::SpacedSeparator();
      }
      DrawSectionEditor(*section);
      ImGui::Unindent();
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
  }

 private:
  void DrawSectionEditor(GuideSection& section) {
    ImGui::InputTextMultiline("##DescriptionInput",
                              section.mutable_text(),
                              ImVec2(0, 0),
                              ImGuiInputTextFlags_AllowTabInput);
    DrawPlaylistsEditor(section);
  }

  void DrawPlaylistsEditor(GuideSection& section) {
    ImGui::IdGuard cid("Playlists");
    ImGui::LoopId loop_id;
    int remove_i = -1;
    for (int i = 0; i < section.playlists_size(); ++i) {
      auto lid = loop_id.Get("Playlist");
      const std::string& playlist_name = section.playlists(i);
      ImGui::AlignTextToFramePadding();
      ImGui::Text(playlist_name);
      ImGui::SameLine();
      if (ImGui::ClearButton()) {
        remove_i = i;
      }
    }

    if (remove_i >= 0) {
      section.mutable_playlists()->erase(section.mutable_playlists()->begin() + remove_i);
    }
    ImGui::Text("Add playlist");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 18);
    ImGui::InputText("###AddPlaylistInput", &playlist_search_text_);
    ImGui::SameLine();
    if (ImGui::ClearButton()) {
      playlist_search_text_ = "";
    }
    if (playlist_search_text_.size() > 0) {
      ImGui::Indent();
      auto names = app_.playlist_manager().playlist_names();
      SearchSelectorOptions options;
      // options.additional_predicate = [&](const std::string& scenario_name) {
      //   bool already_in_guide =
      //       std::any_of(scenario_items_.begin(), scenario_items_.end(), [=](const auto& item) {
      //         return item.scenario() == scenario_name;
      //       });
      //   return !already_in_playlist;
      // };

      std::optional<std::string> selected_playlist =
          SearchSelector(playlist_search_text_, *names, options);
      if (selected_playlist) {
        section.add_playlists(*selected_playlist);
      }
      ImGui::Unindent();
    }
  }

  float char_x_ = 0;
  std::string playlist_search_text_;
  Application& app_ = GetUiApp();
  const GuideItem original_guide_;
  GuideItem updated_guide_;
};

class GuideViewer {
 public:
  GuideViewer() : app_(GetUiApp()) {
    playlist_component_ = CreatePlaylistComponent();
  }

  void SetPlaylistRunIfInGuide(const GuideDef& guide) {
    run_ = {};
    auto current_run = app_.playlist_manager().GetCurrentRun();
    if (!current_run) {
      return;
    }
    for (const auto& section : guide.sections()) {
      for (const std::string& playlist : section.playlists()) {
        if (playlist == current_run->playlist.name) {
          run_ = current_run;
          return;
        }
      }
    }
  }

  void Draw(const std::string& name, const GuideDef& guide) {
    SetPlaylistRunIfInGuide(guide);

    // ImGui::BeginChild("GuideContainer", ImVec2(0, 0));
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("GuideColumns", 3, flags)) {
      return;
    }

    ImGui::TableNextColumn();
    ImGui::BeginChild("GuideColumn");
    ImGui::LoopId loop_id;
    for (const auto& section : guide.sections()) {
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
  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::shared_ptr<PlaylistRun> run_;
};

class GuidesComponentImpl : public GuidesComponent {
 public:
  GuidesComponentImpl() : app_(GetUiApp()), editor_({}) {
    playlist_component_ = CreatePlaylistComponent();
  }

  void Show() override {
    ImGui::IdGuard cid("Guides");
    if (add_dialog_.Draw(app_)) {
      app_.bundle_manager().SaveDirtyBundles();
    }

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (!ImGui::BeginTable("GuideColumns", 3, flags)) {
      return;
    }

    ImGui::TableNextColumn();
    ImGui::BeginChild("GuideColumn");
    if (ImGui::Button(std::format("{} Guide", icons::kAdd))) {
      add_dialog_.NotifyOpen();
    }

    ImGui::SpacedSeparator();

    ObjectBrowser::Result browser_result;
    browser_->Draw(&browser_result);

    ImGui::EndChild();
    ImGui::EndTable();
  }

 private:
  Application& app_;

  std::unique_ptr<ObjectBrowser> browser_ = CreateObjectBrowser(ObjectType::GUIDE);
  std::unique_ptr<PlaylistComponent> playlist_component_;
  std::shared_ptr<PlaylistRun> run_;
  GuideEditor editor_;
  AddGuideDialog add_dialog_{"AddGuideDialog"};
  std::string current_guide_name_;
};

}  // namespace

std::unique_ptr<GuidesComponent> CreateGuidesComponent() {
  return std::make_unique<GuidesComponentImpl>();
}

}  // namespace aim
