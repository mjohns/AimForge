#include "playlist_editor_component.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor_screen.h"
#include "google/protobuf/util/message_differencer.h"
#include "imgui.h"

namespace aim {
namespace {

enum class PlaylistType {
  DEFAULT,
  LEVELS,
};

const std::vector<std::pair<PlaylistType, std::string>> kPlaylistTypes{
    {PlaylistType::DEFAULT, "Default"},
    {PlaylistType::LEVELS, "Levels"},
};

class PlaylistEditorComponentImpl : public PlaylistEditorComponent {
 public:
  explicit PlaylistEditorComponentImpl(Screen& screen, const std::string& playlist_name)
      : app_(screen.app()), screen_(screen) {
    std::shared_ptr<PlaylistRun> run = app_.playlist_manager().GetRun(playlist_name);
    if (run != nullptr) {
      new_playlist_name_ = run->playlist.name.relative_name();
      original_playlist_name_ = run->playlist.name;
      bundle_name_ = run->playlist.name.bundle_name();
      auto maybe_playlist = app_.playlist_manager().GetPlaylist(run->playlist.name);
      if (maybe_playlist) {
        original_playlist_def_ = maybe_playlist->def();
        for (auto& i : maybe_playlist->items()) {
          scenario_items_.push_back(i);
        }
        if (maybe_playlist->def().has_scenario_levels_def()) {
          levels_def_ = maybe_playlist->def().scenario_levels_def();
        }
      }
    }
  }

  void Draw(EditorResult* result) {
    ImGui::IdGuard cid("PlaylistEditor");

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Error", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      ImGui::Text("Unable to save playlist");

      if (ImGui::Button("OK", ImVec2(120, 0))) {
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    ImGui::AlignTextToFramePadding();
    ImGui::Text(bundle_name_);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(400);
    ImGui::InputText("###PlaylistNameInput", &new_playlist_name_);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Type");
    ImGui::SameLine();
    PlaylistType type = levels_def_.has_value() ? PlaylistType::LEVELS : PlaylistType::DEFAULT;
    if (ImGui::SimpleTypeDropdown("##TypeSelector", &type, kPlaylistTypes, char_x_ * 10)) {
      if (type == PlaylistType::LEVELS) {
        levels_def_ = ScenarioLevelsDef();
        ScenarioLevelsDef& levels = *levels_def_;
        levels.set_max_level(10);
        levels.set_num_plays_per_level(1);
      }
      if (type == PlaylistType::DEFAULT) {
        levels_def_ = {};
      }
    }

    if (type == PlaylistType::LEVELS) {
      DrawLevelsEditor();
    }

    if (ImGui::Button("Save")) {
      if (SavePlaylist()) {
        result->editor_closed = true;
        result->playlist_updated = true;
        return;
      }
      ImGui::OpenPopup("Error");
    }

    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
      result->editor_closed = true;
      return;
    }

    if (type == PlaylistType::DEFAULT) {
      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      ImGui::BeginChild("PlaylistScrollableContent");
      DrawPlaylistScenariosEditor(result);
      ImGui::EndChild();
    }
  }

 private:
  void DrawPlaylistScenariosEditor(EditorResult* result) {
    int remove_i = -1;
    bool still_dragging = false;
    for (int i = 0; i < scenario_items_.size(); ++i) {
      ImGui::IdGuard lid("PlaylistItem", i);
      PlaylistItem& item = scenario_items_[i];
      const std::string& scenario_name = item.scenario();

      if (i == dragging_i_) {
        ImGui::BeginDisabled();
        ImGui::Button(scenario_name);
        ImGui::EndDisabled();
      } else {
        ImGui::Button(scenario_name);
      }
      if (ImGui::BeginDragDropSource()) {
        ImGui::SetDragDropPayload("PLAYLIST_ITEM_TYPE", &i, sizeof(int));
        ImGui::Text("Move \"%s\"", scenario_name.c_str());
        dragging_i_ = i;
        ImGui::EndDragDropSource();
      }
      if (ImGui::BeginDragDropTarget()) {
        ImGuiDragDropFlags drop_target_flags =
            ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
        if (const ImGuiPayload* payload =
                ImGui::AcceptDragDropPayload("PLAYLIST_ITEM_TYPE", drop_target_flags)) {
          ImVec2 rect_min = ImGui::GetItemRectMin();
          ImVec2 rect_max = ImGui::GetItemRectMax();

          float max_y = rect_max.y;
          float min_y = rect_min.y;
          float mouse_y = ImGui::GetMousePos().y;

          ImGuiIO& io = ImGui::GetIO();
          ImDrawList* draw_list = ImGui::GetWindowDrawList();
          float mid_y = min_y + (max_y - min_y) / 2.0;
          float draw_y = min_y;
          int dest_before_i = i;
          if (mouse_y > mid_y) {
            draw_y = max_y;
            dest_before_i++;
          }

          draw_list->AddLine(ImVec2(rect_min.x, draw_y),
                             ImVec2(rect_min.x + 200, draw_y),
                             ImGui::GetColorU32(ImGuiCol_DragDropTarget),
                             2.0f);

          if (payload->IsDelivery()) {
            scenario_items_ = MoveVectorItem(scenario_items_, dragging_i_, dest_before_i);
            dragging_i_ = -1;
          }
        }

        ImGui::EndDragDropTarget();
      }
      bool still_dragging_item = dragging_i_ == i && ImGui::IsItemActive() &&
                                 ImGui::IsMouseDragging(ImGuiMouseButton_Left);
      if (still_dragging_item) {
        still_dragging = true;
      }

      const char* item_menu = "PlaylistItemMenu";
      if (ImGui::BeginPopupContextItem(item_menu)) {
        if (ImGui::Selectable("Copy")) {
          scenario_items_.push_back(item);
        }
        if (ImGui::Selectable("Delete")) {
          remove_i = i;
        }
        if (ImGui::Selectable("Edit")) {
          ScenarioEditorOptions opts;
          opts.scenario_id = item.scenario();
          screen_.PushNextScreen(CreateScenarioEditorScreen(opts, &app_));
        }
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick(item_menu, ImGuiPopupFlags_MouseButtonRight);

      ImGui::SameLine();
      u32 num_plays = item.num_plays();
      u32 step = 1;
      ImGui::SetNextItemWidth(char_x_ * 8);
      ImGui::InputScalar("###NumPlays", ImGuiDataType_U32, &num_plays, &step, nullptr, "%u");

      item.set_num_plays(num_plays);
    }

    if (IsValidIndex(scenario_items_, remove_i)) {
      auto it = scenario_items_.begin() + remove_i;
      scenario_items_.erase(it);
    }

    if (!still_dragging) {
      dragging_i_ = -1;
    }

    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Text("Add scenario");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 18);
    ImGui::InputText("###AddScenarioInput", &scenario_search_text_);
    ImGui::SameLine();
    if (ImGui::Button(kIconCancel)) {
      scenario_search_text_ = "";
    }
    if (scenario_search_text_.size() > 0) {
      auto search_words = GetSearchWords(scenario_search_text_);
      ImGui::Indent();
      auto scenarios = app_.scenario_manager().scenarios();
      for (int i = 0; i < scenarios->size(); ++i) {
        ImGui::IdGuard id("ScenarioSearch", i);
        const auto& scenario = (*scenarios)[i];
        if (StringMatchesSearch(scenario.id(), search_words, /*empty_matches=*/false)) {
          bool already_in_playlist =
              std::any_of(scenario_items_.begin(), scenario_items_.end(), [=](const auto& item) {
                return item.scenario() == scenario.id();
              });
          if (!already_in_playlist) {
            if (ImGui::Button(scenario.id().c_str())) {
              PlaylistItem item;
              item.set_scenario(scenario.id());
              item.set_num_plays(1);
              scenario_items_.push_back(item);
            }
          }
        }
      }
      ImGui::Unindent();
    }
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Spacing();
  }

  void DrawLevelsEditor() {
    if (!levels_def_) {
      return;
    }
    ScenarioLevelsDef& levels = *levels_def_;
    if (levels.max_level() < 1) {
      levels.set_max_level(1);
    }

    std::string original_base_scenario_name =
        AddLevelSuffix(original_playlist_name_.full_name(), 1);
    std::string new_base_scenario_name = AddLevelSuffix(new_playlist_name_, 1);

    std::optional<ScenarioItem> original_base_scenario =
        app_.scenario_manager().GetScenario(original_base_scenario_name);
    std::optional<ScenarioItem> new_base_scenario =
        app_.scenario_manager().GetScenario(new_base_scenario_name);

    scenario_items_.clear();

    ImGui::IdGuard cid("LevelsEditor");

    if (!original_base_scenario.has_value() && !new_base_scenario.has_value()) {
      // There  is no base scenario. Add UI to copy the base scenario from other existing scenario.
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Copy initial level from scenario");
      ImGui::SameLine();
      ImGui::HelpMarker(
          "The first scenario level which the overrides will be applied to. The scenario will be "
          "copied into \"<playlist  name> L01\"");
      ImGui::SameLine();
      ImGui::InputText("##BaseScenario", &source_base_scenario_);

      if (source_base_scenario_.size() > 0) {
        auto matching_scenario = app_.scenario_manager().GetScenario(source_base_scenario_);
        if (!matching_scenario) {
          // Show search results for scenarios.
          int num_matches = 0;
          auto search_words = GetSearchWords(source_base_scenario_);
          ImGui::Indent();
          for (const auto& scenario : *app_.scenario_manager().scenarios()) {
            if (StringMatchesSearch(scenario.id(), search_words)) {
              num_matches++;
              if (ImGui::Button(scenario.id())) {
                source_base_scenario_ = scenario.id();
              }
            }
          }
          if (num_matches == 0) {
            ImGui::Text("No matching scenarios found");
          }
          ImGui::Unindent();
        }
      }
    }

    ImGui::InputInt(ImGui::InputIntParams::WithLabelAsId("Max level")
                        .set_step(1, 2)
                        .set_min(2)
                        .set_default(10)
                        .set_width(char_x_ * 10),
                    PROTO_INT_FIELD(ScenarioLevelsDef, &levels, max_level));
    ImGui::InputInt(ImGui::InputIntParams::WithLabelAsId("Plays per level")
                        .set_step(1, 2)
                        .set_min(1)
                        .set_default(1)
                        .set_width(char_x_ * 10),
                    PROTO_INT_FIELD(ScenarioLevelsDef, &levels, num_plays_per_level));

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Per level overrides");
    ImGui::Indent();

    ImGui::InputFloat(
        ImGui::InputFloatParams::WithLabelAsId("Target radius multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(
            ScenarioOverrides, levels.mutable_scenario_overrides(), target_radius_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("SpeedMult")
            .set_label("Speed multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(
            ScenarioOverrides, levels.mutable_scenario_overrides(), speed_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("AccelMult")
            .set_label("Acceleration multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(
            ScenarioOverrides, levels.mutable_scenario_overrides(), acceleration_multiplier));

    ImGui::InputFloat(
        ImGui::InputFloatParams("TimeScale")
            .set_label("Time scale multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(
            ScenarioOverrides, levels.mutable_scenario_overrides(), time_scale_multiplier));
    ImGui::InputFloat(
        ImGui::InputFloatParams("Distance")
            .set_label("Distance multiplier")
            .set_step(0.01, 0.25)
            .set_min(0.01)
            .set_precision(2)
            .set_default(1)
            .set_is_optional()
            .set_width(char_x_ * 10),
        PROTO_FLOAT_FIELD(
            ScenarioOverrides, levels.mutable_scenario_overrides(), distance_multiplier));

    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
  }

  // For a levels playlist, ensure all of  the scenarios for the various levels are saved.
  bool EnsureLevelsAreSaved(ScenarioLevelsDef& levels,
                            const PlaylistDef& playlist,
                            ResourceName& final_name) {
    bool is_rename = final_name != original_playlist_name_;
    std::string original_base_scenario_name =
        AddLevelSuffix(original_playlist_name_.full_name(), 1);
    std::string new_base_scenario_name = AddLevelSuffix(final_name.full_name(), 1);

    std::optional<ScenarioItem> new_base_scenario =
        app_.scenario_manager().GetScenario(new_base_scenario_name);
    if (!new_base_scenario.has_value()) {
      // The new base scenario does not exist. We need to copy it from the source or rename.
      std::optional<ScenarioItem> source_base_scenario =
          app_.scenario_manager().GetScenario(source_base_scenario_);
      if (source_base_scenario.has_value()) {
        // Copy from the source scenario.
        if (!app_.scenario_manager().SaveScenario(ResourceName::Parse(new_base_scenario_name),
                                                  source_base_scenario->unevaluated_def)) {
          return false;
        }
      } else {
        std::optional<ScenarioItem> original_base_scenario =
            app_.scenario_manager().GetScenario(original_base_scenario_name);
        if (!original_base_scenario.has_value()) {
          // This should have been validated earlier. Source should have been present.
          return false;
        }
        if (!app_.scenario_manager().RenameScenario(original_base_scenario->name,
                                                    ResourceName::Parse(new_base_scenario_name))) {
          return false;
        }
      }
    }

    // The base scenario should exist now.
    new_base_scenario = app_.scenario_manager().GetScenario(new_base_scenario_name);

    for (int i = 2; i <= levels.max_level(); ++i) {
      std::string name = AddLevelSuffix(final_name.full_name(), i);

      if (is_rename) {
        // The playlist was renamed. Rename this level if it exists. It will be updated or created
        // as necessary later.
        std::string old_name = AddLevelSuffix(original_playlist_name_.full_name(), i);
        auto old_scenario = app_.scenario_manager().GetScenario(old_name);
        if (old_scenario.has_value()) {
          if (!app_.scenario_manager().RenameScenario(ResourceName::Parse(old_name),
                                                      ResourceName::Parse(name))) {
            return false;
          }
        }
      }

      // Create chained references.
      auto existing_scenario = app_.scenario_manager().GetScenario(name);
      ScenarioDef def;
      def.mutable_reference_def()->set_scenario_id(AddLevelSuffix(final_name.full_name(), i - 1));
      *def.mutable_overrides() = levels_def_->scenario_overrides();
      bool needs_save = true;
      if (existing_scenario) {
        if (google::protobuf::util::MessageDifferencer::Equivalent(
                def, existing_scenario->unevaluated_def)) {
          needs_save = false;
        }
      }
      if (needs_save) {
        if (!app_.scenario_manager().SaveScenario(ResourceName::Parse(name), def)) {
          return false;
        }
      }
    }

    // If old max_levels was higher, delete scenarios that should no longer exist.
    int old_max_levels = original_playlist_def_.scenario_levels_def().max_level();
    for (int i = levels.max_level() + 1; i <= old_max_levels; ++i) {
      std::string name = AddLevelSuffix(final_name.full_name(), i);
      app_.scenario_manager().DeleteScenario(
          ResourceName::Parse(AddLevelSuffix(final_name.full_name(), i)));
      if (is_rename) {
        app_.scenario_manager().DeleteScenario(
            ResourceName::Parse(AddLevelSuffix(original_playlist_name_.full_name(), i)));
      }
    }

    return true;
  }

  bool SavePlaylist() {
    PlaylistDef playlist;

    if (levels_def_) {
      *playlist.mutable_scenario_levels_def() = *levels_def_;
      // Did it change? We need to update and create scenarios if necessary.
    } else {
      playlist.mutable_items()->Add(scenario_items_.begin(), scenario_items_.end());
    }

    ResourceName final_name(bundle_name_, new_playlist_name_);
    bool name_changed = final_name != original_playlist_name_;
    if (name_changed) {
      // Need to move file.
      std::vector<std::string> taken_names =
          app_.playlist_manager().GetAllRelativeNamesInBundle(bundle_name_);
      final_name.set(bundle_name_, MakeUniqueName(new_playlist_name_, taken_names));
      if (!app_.playlist_manager().RenamePlaylist(original_playlist_name_, final_name)) {
        return false;
      }
      app_.history_manager().UpdateRecentView(ObjectType::PLAYLIST, final_name.full_name());
      std::shared_ptr<PlaylistRun> current_run = app_.playlist_manager().GetCurrentRun();
      if (current_run != nullptr && current_run->playlist.name == original_playlist_name_) {
        app_.playlist_manager().SetCurrentPlaylist(final_name.full_name());
      }
    }

    if (levels_def_) {
      if (!EnsureLevelsAreSaved(*levels_def_, playlist, final_name)) {
        return false;
      }
    }

    return app_.playlist_manager().SavePlaylist(final_name, playlist);
  }

  Application& app_;
  Screen& screen_;
  std::vector<PlaylistItem> scenario_items_;
  int dragging_i_ = -1;
  ResourceName original_playlist_name_;
  std::string bundle_name_;
  std::optional<ScenarioLevelsDef> levels_def_;
  std::string source_base_scenario_;

  std::string scenario_search_text_;
  std::string new_playlist_name_;
  float char_x_ = 0;
  PlaylistDef original_playlist_def_;
};

}  // namespace

std::unique_ptr<PlaylistEditorComponent> CreatePlaylistEditorComponent(
    const std::string& playlist_name, UiScreen* screen) {
  return std::make_unique<PlaylistEditorComponentImpl>(*screen, playlist_name);
}

}  // namespace aim
