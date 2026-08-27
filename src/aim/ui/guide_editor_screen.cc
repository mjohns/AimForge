#include "guide_editor_screen.h"

#include "absl/strings/ascii.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/proto_util.h"
#include "aim/common/resource_name.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/guide_manager.h"
#include "aim/core/history_manager.h"
#include "aim/core/playlist_manager.h"
#include "aim/proto/guide.pb.h"
#include "aim/ui/search_selector.h"
#include "aim/ui/ui_app.h"
#include "imgui.h"

namespace aim {
namespace {

class GuideEditorScreen : public UiScreen {
 public:
  GuideEditorScreen(const GuideEditorOptions& opts) : UiScreen(), options_(opts) {
    bundle_names_ = app_.bundle_manager().GetWritableBundleNames();
    if (!opts.name.empty()) {
      original_name_ = ResourceName::Parse(opts.name);
      name_ = *original_name_;

      auto initial_guide = app_.guide_manager().GetGuide(opts.name);
      if (initial_guide) {
        original_guide_ = initial_guide->def;
        updated_guide_ = original_guide_;
      } else {
        notification_popup_.NotifyOpen(std::format("Guide \"{}\" does not exist.", opts.name));
        exit_after_notification_ = true;
      }
    }

    // GuideEditor(const GuideItem& original_guide)
    //     : original_guide_(original_guide), updated_guide_(original_guide) {}
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("GuideEditor");
    bool notification_confirmed = notification_popup_.Draw();
    if (notification_confirmed && exit_after_notification_) {
      PopSelf();
    }

    DrawTopBar();
    if (BeginMainWindow("MainEditor", 0.9)) {
      DrawEditor();
    }
    ImGui::End();
  }

  void DrawEditor() {
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    ImGui::LoopId loop_id;
    GuideDef& def = updated_guide_;

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
      updated_guide_.add_sections();
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
  void DrawTopBar() {
    float middle = app_.screen_info().width / 2.0;
    ImGui::SetNextWindowPos(ImVec2(middle, char_x_ / 3.0), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
    if (!ImGui::Begin("TopBar",
                      nullptr,
                      ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                          ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::End();
      return;
    }

    // bool notification_confirmed = notification_popup_.Draw();
    // if (notification_confirmed && exit_after_notification_) {
    //   PopSelf();
    // }

    ImGui::SimpleDropdown("BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 11);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 40);
    ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

    ImGui::SameLine();
    std::string save_text = options_.is_new_guide ? std::format("{} Create", icons::kSave)
                                                  : std::format("{} Update", icons::kSave);
    if (ImGui::Button(save_text, ImVec2(char_x_ * 8, 0))) {
      if (SaveGuide()) {
        PopSelf();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      PopSelf();
    }

    ImGui::End();
  }

  bool SaveGuide() {
    if (name_.bundle_name().size() == 0 || name_.relative_name().size() == 0) {
      SetErrorMessage("Missing guide name");
      return false;
    }

    absl::StripAsciiWhitespace(name_.mutable_relative_name());

    auto& mgr = app_.guide_manager();

    bool is_rename = original_name_.has_value() && original_name_->full_name() != name_.full_name();
    if (is_rename || options_.is_new_guide) {
      // Make sure new name is not taken.
      auto existing_with_name = mgr.GetGuide(name_.full_name());
      if (existing_with_name.has_value()) {
        SetErrorMessage(std::format("Guide \"{}\" already exists", name_.full_name()));
        return false;
      }
    }

    if (is_rename) {
      mgr.RenameGuide(original_name_->full_name(), name_.full_name());
    }

    mgr.UpdateGuide(name_.full_name(), updated_guide_);
    if (!app_.bundle_manager().SaveDirtyBundles()) {
      SetErrorMessage("Unable to save bundle to disk.");
      return false;
    }

    {
      // Make sure we preserve the original level/sens in the name for the current scenario.
      // app_.scenario_manager().SetCurrentScenario(current_name.GetFullName());
      app_.history_manager().UpdateRecentView(ObjectType::GUIDE, name_.full_name());
    }

    return true;
  }

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

  void SetErrorMessage(const std::string& msg) {
    notification_popup_.NotifyOpen(msg);
  }

  bool BeginMainWindow(const std::string& name, float width_multiple) {
    float padding = char_x_ * 0.3;
    float start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 1;
    float end_y = app_.screen_info().height - padding;
    float width = app_.screen_info().width * width_multiple;
    float height = end_y - start_y;

    ImGui::SetNextWindowPos(ImVec2((app_.screen_info().width - width) / 2.0, start_y));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    return ImGui::Begin(
        name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  }

  Application& app_ = GetUiApp();
  float char_x_ = 0;
  std::string playlist_search_text_;
  GuideDef original_guide_;
  GuideDef updated_guide_;
  GuideEditorOptions options_;
  std::vector<std::string> bundle_names_;
  std::optional<ResourceName> original_name_;
  ResourceName name_;
  ImGui::NotificationPopup notification_popup_{"Notification"};
  bool exit_after_notification_ = false;
};

}  // namespace

std::unique_ptr<UiScreen> CreateGuideEditorScreen(const GuideEditorOptions& options) {
  return std::make_unique<GuideEditorScreen>(options);
}

}  // namespace aim
