#include "playlist_ui.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor_screen.h"
#include "google/protobuf/util/message_differencer.h"
#include "imgui.h"

namespace aim {
namespace {

const char* kPlaylistViewTypeKey = "PlaylistViewType";

enum class PlaylistType {
  DEFAULT,
  LEVELS,
};

const std::vector<std::pair<PlaylistType, std::string>> kPlaylistTypes{
    {PlaylistType::DEFAULT, "Default"},
    {PlaylistType::LEVELS, "Levels"},
};

std::vector<std::string> GetAllRelativeNamesInBundle(const std::string& bundle_name,
                                                     Application* app) {
  std::vector<std::string> names;
  for (const Playlist& playlist : *app->playlist_manager().playlists()) {
    if (playlist.name.bundle_name() == bundle_name) {
      names.push_back(playlist.name.relative_name());
    }
  }
  return names;
}

struct CopyPlaylistOptions {
  std::string remove_prefix;
  std::string add_prefix;
  bool deep_copy = false;
  bool as_references = false;
  bool bake_references = false;
};

bool CopyPlaylist(Playlist source,
                  ResourceName new_playlist_name,
                  const CopyPlaylistOptions& opts,
                  Application& app) {
  // Copy all scenarios if necessary.
  auto taken_names = GetAllRelativeNamesInBundle(new_playlist_name.bundle_name(), &app);
  *new_playlist_name.mutable_relative_name() =
      MakeUniqueName(new_playlist_name.relative_name(), taken_names);

  PlaylistDef dest = source.def;
  if (opts.deep_copy) {
    std::unordered_map<std::string, ResourceName> new_name_map;
    std::unordered_map<std::string, ScenarioDef> new_scenario_map;
    dest.clear_items();
    for (const auto& source_item : source.def.items()) {
      auto source_scenario = app.scenario_manager().GetScenario(source_item.scenario());
      if (!source_scenario) {
        // Skip invalid scenarios.
        continue;
      }
      ResourceName new_scenario_name = source_scenario->name;
      *new_scenario_name.mutable_bundle_name() = new_playlist_name.bundle_name();

      std::string* relative_name = new_scenario_name.mutable_relative_name();
      if (opts.remove_prefix.size() > 0) {
        *relative_name = absl::StripLeadingAsciiWhitespace(
            absl::StripPrefix(*relative_name, opts.remove_prefix));
      }
      if (opts.add_prefix.size() > 0) {
        *relative_name = std::format("{} {}", opts.add_prefix, *relative_name);
      }

      ScenarioDef new_def;
      if (opts.as_references) {
        new_def.mutable_reference_def()->set_scenario_id(source_item.scenario());
      } else if (opts.bake_references) {
        new_def = source_scenario->evaluated_def;
      } else {
        new_def = source_scenario->unevaluated_def;
      }
      auto maybe_final_scenario_name =
          app.scenario_manager().SaveScenarioWithUniqueName(new_scenario_name, new_def);
      if (maybe_final_scenario_name) {
        new_name_map[source_item.scenario()] = *maybe_final_scenario_name;
        new_scenario_map[maybe_final_scenario_name->full_name()] = new_def;
        PlaylistItem item = source_item;
        item.set_scenario(maybe_final_scenario_name->full_name());
        *dest.add_items() = item;
      }
    }
    if (!opts.as_references) {
      // Make sure any copied scenarios which were references that pointed to other scenarios in the
      // playlist are updated to point to the version in the new playlist.
      for (const auto& item : dest.items()) {
        ScenarioDef& def = new_scenario_map[item.scenario()];
        std::string old_referenced_scenario = def.reference_def().scenario_id();
        if (old_referenced_scenario.size() > 0) {
          auto new_referenced_scenario = new_name_map.find(old_referenced_scenario);
          if (new_referenced_scenario != new_name_map.end()) {
            def.mutable_reference_def()->set_scenario_id(
                new_referenced_scenario->second.full_name());
            app.scenario_manager().SaveScenario(ResourceName::Parse(item.scenario()), def);
          }
        }
      }
    }
  }
  app.playlist_manager().SavePlaylist(new_playlist_name, dest);
  app.playlist_manager().SetCurrentPlaylist(new_playlist_name.full_name());
  app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, new_playlist_name.full_name());
  return true;
}

struct EditorResult {
  bool playlist_updated = false;
  bool editor_closed = false;
  std::string new_playlist_name;
};

class PlaylistEditorComponent {
 public:
  explicit PlaylistEditorComponent(Application& app,
                                   Screen& screen,
                                   const std::string& playlist_name)
      : app_(app), screen_(screen) {
    std::shared_ptr<PlaylistRun> run = app.playlist_manager().GetRun(playlist_name);
    if (run != nullptr) {
      new_playlist_name_ = run->playlist.name.relative_name();
      original_playlist_name_ = run->playlist.name;
      bundle_name_ = run->playlist.name.bundle_name();
      auto maybe_playlist = app_.playlist_manager().GetPlaylist(run->playlist.name);
      if (maybe_playlist) {
        for (auto& i : maybe_playlist->def.items()) {
          scenario_items_.push_back(i);
        }
        if (maybe_playlist->def.has_scenario_levels_def()) {
          levels_def_ = maybe_playlist->def.scenario_levels_def();
        }
      }
    }
  }

  void DrawPlaylistLevelsList() {
    for (auto& item : scenario_items_) {
      ImGui::Text(item.scenario());
    }
  }

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

  std::string GetScenarioLevelName(int i) {
    if (!levels_def_ || levels_def_->base_scenario().size() == 0) {
      return "";
    }

    std::string base_scenario = levels_def_->base_scenario();
    int level = 0;
    auto parsed_base_name = StripLevelSuffix(base_scenario, &level);
    if (parsed_base_name) {
      base_scenario = *parsed_base_name;
    }

    return AddLevelSuffix(base_scenario, i);
  }

  void DrawLevelsEditor() {
    if (!levels_def_) {
      return;
    }
    ScenarioLevelsDef& levels = *levels_def_;

    scenario_items_.clear();
    if (levels.base_scenario().size() > 5) {
      for (int i = 1; i <= levels.max_level(); ++i) {
        PlaylistItem item;
        item.set_num_plays(levels.num_plays_per_level());
        item.set_scenario(GetScenarioLevelName(i));
        scenario_items_.push_back(item);
      }
    }

    ImGui::IdGuard cid("LevelsEditor");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Base scenario");
    ImGui::SameLine();
    ImGui::HelpMarker("The first scenario level which the overrides will be applied to.");
    ImGui::SameLine();
    ImGui::InputText("##BaseScenario", levels.mutable_base_scenario());

    if (levels.base_scenario().size() > 0) {
      auto matching_scenario = app_.scenario_manager().GetScenario(levels.base_scenario());
      if (!matching_scenario) {
        // Show search results for scenarios.
        int num_matches = 0;
        auto search_words = GetSearchWords(levels.base_scenario());
        ImGui::Indent();
        for (const auto& scenario : *app_.scenario_manager().scenarios()) {
          if (StringMatchesSearch(scenario.id(), search_words)) {
            num_matches++;
            if (ImGui::Button(scenario.id())) {
              levels.set_base_scenario(scenario.id());
            }
          }
        }
        if (num_matches == 0) {
          ImGui::Text("No matching scenarios found");
        }
        ImGui::Unindent();
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

  void Show(EditorResult* result) {
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
        if (scenario_items_.size() > 0) {
          levels.set_base_scenario(scenario_items_[0].scenario());
        }
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

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::BeginChild("PlaylistScrollableContent");

    if (type == PlaylistType::DEFAULT) {
      DrawPlaylistScenariosEditor(result);
    }
    if (type == PlaylistType::LEVELS) {
      DrawPlaylistLevelsList();
    }

    ImGui::EndChild();
  }

 private:
  bool EnsureLevelsAreSaved(ScenarioLevelsDef& levels, const PlaylistDef& playlist) {
    if (playlist.items_size() == 0) {
      return false;
    }
    auto base_scenario = app_.scenario_manager().GetScenario(levels.base_scenario());
    if (!base_scenario) {
      return false;
    }
    for (int i = 0; i < playlist.items_size(); ++i) {
      const PlaylistItem& item = playlist.items(i);
      if (i == 0) {
        // Copy the base scenario to create the first level if necessary.
        if (base_scenario->id() != item.scenario()) {
          if (!app_.scenario_manager().SaveScenario(ResourceName::Parse(item.scenario()),
                                                    base_scenario->evaluated_def)) {
            return false;
          }
          levels_def_->set_base_scenario(item.scenario());
        }
      } else {
        // Create chained references.
        auto existing_scenario = app_.scenario_manager().GetScenario(item.scenario());
        ScenarioDef def;
        def.mutable_reference_def()->set_scenario_id(playlist.items(i - 1).scenario());
        *def.mutable_overrides() = levels_def_->scenario_overrides();
        bool needs_save = true;
        if (existing_scenario) {
          if (google::protobuf::util::MessageDifferencer::Equivalent(
                  def, existing_scenario->unevaluated_def)) {
            needs_save = false;
          }
        }

        if (needs_save) {
          if (!app_.scenario_manager().SaveScenario(ResourceName::Parse(item.scenario()), def)) {
            return false;
          }
        }
      }
    }

    return true;
  }

  bool SavePlaylist() {
    PlaylistDef playlist;
    playlist.mutable_items()->Add(scenario_items_.begin(), scenario_items_.end());

    if (levels_def_) {
      *playlist.mutable_scenario_levels_def() = *levels_def_;
      // Did it change? We need to update and create scenarios if necessary.
    }

    ResourceName final_name(bundle_name_, new_playlist_name_);
    bool name_changed = final_name != original_playlist_name_;
    if (name_changed) {
      // Need to move file.
      std::vector<std::string> taken_names = GetAllRelativeNamesInBundle(bundle_name_, &app_);
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
      if (!EnsureLevelsAreSaved(*levels_def_, playlist)) {
        return false;
      }
      *playlist.mutable_scenario_levels_def() = *levels_def_;
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

  std::string scenario_search_text_;
  std::string new_playlist_name_;
  float char_x_ = 0;
};

enum class PlaylistViewType : int {
  RECENT = 1,
  ALL = 2,
  STARRED = 3,
};

class PlaylistComponentImpl : public PlaylistComponent {
 public:
  explicit PlaylistComponentImpl(UiScreen& screen) : app_(screen.app()), screen_(screen) {}

  bool Show(const std::string& playlist_name, std::string* scenario_to_start) override {
    ImGui::IdGuard cid("PlaylistComponent");

    if (playlist_name != current_playlist_name_) {
      current_playlist_name_ = playlist_name;
      ResetForNewCurrentPlaylist();
    }

    if (showing_editor_) {
      if (!editor_component_) {
        editor_component_ = std::make_unique<PlaylistEditorComponent>(app_, screen_, playlist_name);
      }
      EditorResult editor_result;
      editor_component_->Show(&editor_result);
      if (editor_result.editor_closed) {
        editor_component_ = {};
        showing_editor_ = false;
      }
      return false;
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", playlist_name.c_str());
    ImGui::SameLine();
    if (ImGui::Button(kIconEdit)) {
      showing_editor_ = true;
    }
    ImGui::Spacing();
    ImGui::Spacing();
    PlaylistRunComponent("PlaylistRun", app_.playlist_manager().GetCurrentRun(), screen_);
    return false;
  }

 private:
  void ResetForNewCurrentPlaylist() {
    editor_component_ = {};
    showing_editor_ = false;
  }

  bool showing_editor_ = false;
  std::unique_ptr<PlaylistEditorComponent> editor_component_;
  std::string current_playlist_name_;
  UiScreen& screen_;
  Application& app_;
};

class PlaylistListComponentImpl : public PlaylistListComponent {
 public:
  explicit PlaylistListComponentImpl(UiScreen& screen) : screen_(screen), app_(screen.app()) {
    auto maybe_initial_view_type = app_.local_store().GetInt(kPlaylistViewTypeKey);
    if (maybe_initial_view_type) {
      view_type_ = static_cast<PlaylistViewType>(*maybe_initial_view_type);
    } else {
      view_type_ = PlaylistViewType::ALL;
    }
  }

  void Show(PlaylistListResult* result) override {
    delete_confirmation_dialog_.Draw("Delete", [=](const Playlist& playlist) {
      screen_.app().playlist_manager().DeletePlaylist(playlist.name);
      // result->reload_playlists = true;
    });

    if (copy_dialog_.Draw(app_)) {
      // result->reload_playlists = true;
    }
    if (add_dialog_.Draw(app_)) {
      // result->reload_playlists = true;
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##PlaylistSearchInput", kIconSearch, &playlist_search_text_);
    if (playlist_search_text_.size() > 0) {
      ImGui::SameLine();
      if (ImGui::Button(kIconCancel)) {
        playlist_search_text_ = "";
      }
    }

    ImGui::SetNextItemWidth(char_size.x * 8);
    if (ImGui::SimpleTypeDropdown("##PlaylistViewType",
                                  &view_type_,
                                  {
                                      {PlaylistViewType::RECENT, "Recent"},
                                      {PlaylistViewType::STARRED, "Starred"},
                                      {PlaylistViewType::ALL, "All"},
                                  })) {
      app_.local_store().PutInt(kPlaylistViewTypeKey, (int)view_type_);
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("{} Add", kIconAdd))) {
      add_dialog_.NotifyOpen();
    }
    ImGui::Spacing();
    ImGui::Spacing();

    ImGui::BeginChild("PlaylistsContent");

    auto search_words = GetSearchWords(playlist_search_text_);
    ImGui::LoopId loop_id;

    if (view_type_ == PlaylistViewType::RECENT) {
      for (const std::string& name : app_.history_manager().recent_playlists()) {
        auto id_guard = loop_id.Get();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    } else if (view_type_ == PlaylistViewType::STARRED) {
      auto items = app_.labels_manager().ListStarredItems(ObjectType::PLAYLIST);
      for (const std::string& name : items->items) {
        auto id_guard = loop_id.Get();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    } else {
      for (const auto& playlist : *app_.playlist_manager().playlists()) {
        auto id_guard = loop_id.Get();
        std::string name = playlist.name.full_name();
        if (StringMatchesSearch(name, search_words)) {
          DrawPlaylistItem(name, result);
        }
      }
    }

    ImGui::EndChild();
  }

  void DrawPlaylistItem(const std::string& playlist_name, PlaylistListResult* result) {
    auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
    if (!playlist) {
      return;
    }
    if (ImGui::Button(playlist_name.c_str())) {
      result->open_playlist = *playlist;
    }
    const char* menu_id = "PlaylistItemMenu";
    if (ImGui::BeginPopupContextItem(menu_id)) {
      if (ImGui::Selectable("Copy")) {
        auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
        if (playlist) {
          copy_dialog_.NotifyOpen(*playlist);
        }
      }
      if (ImGui::Selectable("Delete")) {
        auto playlist = app_.playlist_manager().GetPlaylist(playlist_name);
        if (playlist) {
          delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", playlist_name),
                                                 *playlist);
        }
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(menu_id, ImGuiPopupFlags_MouseButtonRight);

    ImGui::SameLine();
    if (app_.labels_manager().IsStarred(ObjectType::PLAYLIST, playlist_name)) {
      if (ImGui::Selectable(kIconStar, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_.labels_manager().UnstarItem(ObjectType::PLAYLIST, playlist_name);
      }
    } else {
      if (ImGui::Selectable(kIconStarOutline, false, 0, ImVec2(ImGui::GetTextLineHeight(), 0))) {
        app_.labels_manager().StarItem(ObjectType::PLAYLIST, playlist_name);
      }
    }
  }

 private:
  ImGui::ConfirmationDialog<Playlist> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::string playlist_search_text_;
  UiScreen& screen_;
  Application& app_;
  CopyPlaylistDialog copy_dialog_{"CopyPlaylistDialog"};
  AddPlaylistDialog add_dialog_{"AddPlaylistDialog"};
  PlaylistViewType view_type_ = PlaylistViewType::ALL;
};

}  // namespace

void PlaylistRunRightClickMenu(const std::string& scenario_id, PlaylistRun& run, Screen& screen) {
  const char* popup_id = "ScenarioItemMenu";
  if (ImGui::BeginPopupContextItem(popup_id)) {
    if (ImGui::Selectable("Edit")) {
      ScenarioEditorOptions opts;
      opts.scenario_id = scenario_id;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::Selectable("Edit new copy")) {
      ScenarioEditorOptions opts;
      opts.scenario_id = scenario_id;
      opts.is_new_copy = true;
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::Selectable("Add new copy")) {
      ScenarioEditorOptions opts;
      opts.scenario_id = scenario_id;
      opts.is_new_copy = true;
      opts.add_to_playlist = run.playlist_name();
      opts.force_bundle_name = ResourceName::Parse(opts.add_to_playlist).bundle_name();
      screen.PushNextScreen(CreateScenarioEditorScreen(opts, &screen.app()));
    }
    if (ImGui::BeginMenu("Add to")) {
      std::string selected_playlist;
      int playlist_count = 0;
      const auto& recent_playlists = screen.app().history_manager().recent_playlists();
      for (int i = 0; i < std::min<int>(6, recent_playlists.size()); ++i) {
        const std::string& playlist_name = recent_playlists[i];
        ImGui::IdGuard playlist_id(playlist_name, i);
        if (run.playlist_name() != playlist_name && ImGui::MenuItem(playlist_name.c_str())) {
          selected_playlist = playlist_name;
        }
      }
      if (selected_playlist.size() > 0) {
        screen.app().playlist_manager().AddScenarioToPlaylist(selected_playlist, scenario_id);
      }
      ImGui::EndMenu();
    }
    ImGui::EndPopup();
  }
  ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
}

void PlaylistRunComponent(const std::string& id, std::shared_ptr<PlaylistRun> run, Screen& screen) {
  ImGui::IdGuard cid(id);
  const PlaylistDef& playlist = run->playlist.def;
  ImGuiTableFlags flags = ImGuiTableFlags_RowBg;
  std::string sample_progress_text = "00/00";
  float progress_width = ImGui::CalcTextSize(sample_progress_text.c_str()).x;

  if (!ImGui::BeginTable("PlaylistRuns", 2, flags)) {
    return;
  }
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthStretch);
  ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, progress_width);

  std::vector<float> score_levels;
  bool has_score_level = false;
  for (const auto& item : playlist.items()) {
    auto stats = screen.app().stats_manager().GetAggregateStats(item.scenario());
    float level = 0;
    if (stats.total_runs > 0) {
      auto scenario = screen.app().scenario_manager().GetScenario(item.scenario());
      if (scenario) {
        level = GetScenarioScoreLevel(stats.high_score_stats.score, scenario->evaluated_def);
        if (level > 0) {
          has_score_level = true;
        }
      }
    }
    score_levels.push_back(level);
  }

  for (int i = 0; i < playlist.items_size(); ++i) {
    ImGui::TableNextRow();
    ImGui::IdGuard id(i);
    PlaylistItemProgress& progress = run->progress_list[i];
    PlaylistItem item = playlist.items(i);
    bool is_selected = i == run->current_index;

    ImGui::TableNextColumn();
    std::string label = item.scenario();
    if (score_levels[i] > 0) {
      label = std::format("{} -- {}", label, MaybeIntToString(score_levels[i], 1));
    }
    if (ImGui::Selectable(label.c_str(), is_selected)) {
      run->current_index = i;
      screen.state().scenario_run_option = ScenarioRunOption::START_CURRENT;
      screen.app().scenario_manager().SetCurrentScenario(item.scenario());
      screen.ReturnHome();
    }
    PlaylistRunRightClickMenu(item.scenario(), *run, screen);

    ImGui::TableNextColumn();
    std::string progress_text = std::format("{}/{}", progress.runs_done, item.num_plays());
    ImGui::Text(progress_text);
  }

  ImGui::EndTable();
}

std::unique_ptr<PlaylistComponent> CreatePlaylistComponent(UiScreen* screen) {
  return std::make_unique<PlaylistComponentImpl>(*screen);
}

std::unique_ptr<PlaylistListComponent> CreatePlaylistListComponent(UiScreen* screen) {
  return std::make_unique<PlaylistListComponentImpl>(*screen);
}

bool CopyPlaylistDialog::Draw(Application& app) {
  ImGui::IdGuard cid("CopyPlaylistDialogContent");
  bool did_copy = false;
  bool show_popup = source_.has_value();
  if (show_popup) {
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(id_.c_str(),
                               &show_popup,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
      ImGui::TextFmt("Copy \"{}\" to", source_->name.full_name());
      ImGui::SimpleDropdown("BundlePicker",
                            new_name_.mutable_bundle_name(),
                            bundle_names_,
                            ImGui::GetFrameHeight() * 9);
      ImGui::SameLine();
      ImGui::InputText("##RelativeNameInput", new_name_.mutable_relative_name());

      ImGui::Indent();
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Make new copies of all scenarios");
      ImGui::SameLine();
      ImGui::Checkbox("##DeepCopy", &deep_copy_);

      if (deep_copy_) {
        ImGui::Indent();

        ImGui::AlignTextToFramePadding();
        ImGui::Text("As references");
        ImGui::SameLine();
        ImGui::Checkbox("##AsReferences", &as_references_);
        ImGui::SameLine();
        ImGui::HelpMarker(
            "Versions in new playlist will have a different name (stats, settings, ..), but will "
            "change when the underlying scenario changes.");

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Add name prefix*");
        ImGui::SameLine();
        ImGui::InputText("##AddPrefix", &add_prefix_);
        ImGui::SameLine();
        ImGui::HelpMarker(
            "Adds the following prefix to all newly created scenario names after the bundle "
            "name");

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Remove name prefix");
        ImGui::SameLine();
        ImGui::InputText("##RemovePrefix", &remove_prefix_);
        ImGui::SameLine();
        ImGui::HelpMarker("Strips the following prefix from the name of scenarios being copied");

        ImGui::Unindent();
      }

      ImGui::Unindent();
      bool cant_copy = deep_copy_ && add_prefix_.size() == 0;
      if (cant_copy) {
        ImGui::BeginDisabled();
      }
      if (ImGui::Button("Copy")) {
        // Do copy
        CopyPlaylistOptions opts;
        opts.add_prefix = add_prefix_;
        opts.remove_prefix = remove_prefix_;
        opts.deep_copy = deep_copy_;
        opts.as_references = deep_copy_ && as_references_;
        CopyPlaylist(*source_, new_name_, opts, app);
        did_copy = true;
        ImGui::CloseCurrentPopup();
        source_ = {};
      }
      if (cant_copy) {
        ImGui::EndDisabled();
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        source_ = {};
        ImGui::CloseCurrentPopup();
      }
      ImGui::EndPopup();
    }
  }
  if (open_) {
    ImGui::OpenPopup(id_.c_str());
    open_ = false;
    deep_copy_ = false;
    remove_prefix_;
    add_prefix_;
    bundle_names_ = app.file_system()->GetBundleNames();
    new_name_ = source_->name;
    *new_name_.mutable_relative_name() += " Copy";
  }
  return did_copy;
}

bool AddPlaylistDialog::Draw(Application& app) {
  ImGui::IdGuard cid("AddPlaylistDialogContent");
  bool did_add = false;
  if (is_open_) {
    ImGui::SetNextWindowPos(
        ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal(id_.c_str(),
                               &is_open_,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
      ImGui::SimpleDropdown(
          "BundlePicker", name_.mutable_bundle_name(), bundle_names_, ImGui::GetFrameHeight() * 9);
      ImGui::SameLine();
      ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

      if (ImGui::Button("Add")) {
        auto taken_names = GetAllRelativeNamesInBundle(name_.bundle_name(), &app);
        *name_.mutable_relative_name() = MakeUniqueName(name_.relative_name(), taken_names);
        app.playlist_manager().SavePlaylist(name_, PlaylistDef());
        app.playlist_manager().SetCurrentPlaylist(name_.full_name());
        app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, name_.full_name());
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
    bundle_names_ = app.file_system()->GetBundleNames();
    name_.set("USER", "New playlist");
  }
  return did_add;
}

}  // namespace aim
