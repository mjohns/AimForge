#include "scenario_editor_screen.h"

#include <format>
#include <functional>
#include <optional>

#include "absl/strings/ascii.h"
#include "aim/common/field.h"
#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/name_util.h"
#include "aim/common/resource_name.h"
#include "aim/common/search.h"
#include "aim/common/util.h"
#include "aim/common/wall.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/camera.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/core/settings_manager.h"
#include "aim/editor/profile_list_editor.h"
#include "aim/editor/room_editor.h"
#include "aim/editor/scenario_editor_common.h"
#include "aim/editor/scenario_type_editor.h"
#include "aim/editor/target_editor.h"
#include "aim/graphics/crosshair.h"
#include "aim/graphics/renderer.h"
#include "aim/scenario/scenario.h"
#include "aim/scenario/scenario_factory.h"
#include "aim/scenario/scenario_overrides.h"
#include "imgui/misc/cpp/imgui_stdlib.h"

namespace aim {
namespace {

class ScenarioEditorScreen : public UiScreen {
 public:
  explicit ScenarioEditorScreen(const ScenarioEditorOptions& opts, Application& app)
      : UiScreen(app),
        target_manager_(GetDefaultSimpleRoom()),
        add_to_playlist_(opts.add_to_playlist) {
    auto themes = app_.settings_manager().ListThemes();
    if (themes.size() > 0) {
      theme_ = app_.settings_manager().GetTheme(themes[0]);
    } else {
      theme_ = GetDefaultTheme();
    }
    settings_ = app_.settings_manager().GetCurrentSettings();
    *def_.mutable_room() = GetDefaultSimpleRoom();
    bundle_names_ = app_.bundle_manager().GetBundleNames();

    auto initial_scenario = app_.scenario_manager().GetScenario(opts.scenario_name);
    if (initial_scenario.has_value()) {
      def_ = initial_scenario->unevaluated_def;
      name_ = ResourceName::Parse(initial_scenario->name);
      // Strip any dynamic suffixes
      NameInfo name_info = GetScenarioNameInfo(name_.full_name());
      name_ = ResourceName::Parse(name_info.base_name);

      if (opts.force_bundle_name.size() > 0) {
        *name_.mutable_bundle_name() = opts.force_bundle_name;
      }
      if (opts.is_new_copy) {
        std::string final_name = MakeUniqueName(
            name_.relative_name() + " Copy",
            app_.scenario_manager().GetAllRelativeNamesInBundle(name_.bundle_name()));
        *name_.mutable_relative_name() = final_name;
      } else {
        original_name_ = name_;
      }
    }
  }

 protected:
  void DrawTopBar() {
    float width = char_x_ * 70;
    float middle = app_.screen_info().width / 2.0;
    ImGui::SetNextWindowPos(ImVec2(middle - width / 2.0, char_x_ / 3.0));
    ImGui::SetNextWindowSize(ImVec2(width, -1));
    if (!ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
      ImGui::End();
      return;
    }

    notification_popup_.Draw();

    if (ImGui::Button(kIconPlayCircle)) {
      PlayScenario();
    }
    ImGui::HelpTooltip("Try playing the edited version of the scenario.");

    ImGui::SameLine();
    ImGui::SimpleDropdown("BundlePicker", name_.mutable_bundle_name(), bundle_names_, char_x_ * 11);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 40);
    ImGui::InputText("##RelativeNameInput", name_.mutable_relative_name());

    ImGui::SameLine();
    bool is_new_scenario = !original_name_.has_value();
    std::string save_text =
        is_new_scenario ? std::format("{} Create", kIconAdd) : std::format("{} Update", kIconSave);
    if (ImGui::Button(save_text, ImVec2(char_x_ * 8, 0))) {
      if (SaveScenario()) {
        PopSelf();
      }
    }
    ImGui::SameLine();
    ImGui::SetButtonCursorAtRight("Cancel");
    if (ImGui::Button("Cancel")) {
      PopSelf();
    }

    ImGui::End();
  }

  void DrawDescriptionEditor() {
    if (ImGui::Button(std::format("{} Back to editor", kIconArrowBack))) {
      editing_description_ = false;
    }
    ImGui::InputTextMultiline("##DescriptionInput",
                              def_.mutable_description(),
                              ImGui::GetContentRegionAvail(),
                              ImGuiInputTextFlags_AllowTabInput);
  }

  void DrawMainEditor() {
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("MainEditorColumns", 3, flags)) {
      // TODO: make each column default size to 1/3
      ImGui::TableNextColumn();
      DrawDetailsEditor();

      ImGui::TableNextColumn();
      ImGui::BeginChild("SecondColumnContainer", ImVec2(0, 0));
      std::string error_message;
      DrawScenarioTypeEditor(def_, &error_message);
      ImGui::EndChild();

      ImGui::TableNextColumn();
      DrawTargetEditor(def_);

      ImGui::EndTable();
    }
  }

  void DrawScreen() override {
    ImGui::IdGuard cid("ScenarioEditor");
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_size_ = char_size;
    char_x_ = char_size_.x;

    DrawTopBar();

    if (def_.room().type_case() == Room::TYPE_NOT_SET) {
      *def_.mutable_room() = GetDefaultSimpleRoom();
    }

    if (editing_room_) {
      DrawRoomEditor();
      return;
    }

    float padding = char_x_ * 0.3;
    float editor_start_y = ImGui::GetCursorPosY() + ImGui::GetTextLineHeight() * 1;
    float editor_end_y = app_.screen_info().height - padding;

    if (editing_description_) {
      if (BeginMainWindow("DescriptionEditor", 0.6)) {
        DrawDescriptionEditor();
      }
      ImGui::End();
      return;
    }

    if (def_.has_reference_def()) {
      if (BeginMainWindow("ReferenceEditor", 0.6)) {
        DrawReferenceEditor();
      }
      ImGui::End();
      return;
    }

    if (BeginMainWindow("MainEditor", 0.9)) {
      DrawMainEditor();
    }
    ImGui::End();

    if (comparison_window_open_) {
      if (ImGui::Begin("Compare", &comparison_window_open_)) {
        DrawComparisonWindow();
      }
      ImGui::End();
    }
  }

  void DrawComparisonWindow() {
    ImGui::IdGuard cid("ComparisonWindow");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario");
    ImGui::SameLine();
    ImGui::InputText("##CompareScenario", &comparison_scenario_);
    auto matching_scenario = app_.scenario_manager().GetScenario(comparison_scenario_);
    if (!matching_scenario && comparison_scenario_.size() > 0) {
      // Show search results for scenarios.
      int num_matches = 0;
      auto search_words = GetSearchWords(comparison_scenario_);
      ImGui::Indent();
      for (const std::string& scenario_name : *app_.scenario_manager().scenario_names()) {
        if (num_matches > 15) {
          break;
        }
        if (StringMatchesSearch(scenario_name, search_words)) {
          num_matches++;
          if (ImGui::Button(scenario_name)) {
            comparison_scenario_ = scenario_name;
          }
        }
      }
      if (num_matches == 0) {
        ImGui::Text("No matching scenarios found");
      }
      ImGui::Unindent();
    }

    auto maybe_compare_def = app_.scenario_manager().GetEvaluatedScenarioDef(comparison_scenario_);
    if (!maybe_compare_def) {
      return;
    }

    if (!ImGui::BeginTabBar("CompareTabBar")) {
      return;
    }
    ScenarioDef compare_def = *maybe_compare_def;

    if (ImGui::BeginTabItem("Scenario type")) {
      ImGui::Spacing();
      std::string error_message;
      DrawScenarioTypeEditor(compare_def, &error_message);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Targets")) {
      ImGui::Spacing();
      DrawTargetEditor(compare_def);
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }

  void DrawDetailsEditor() {
    float duration_seconds = FirstGreaterThanZero(def_.duration_seconds(), 60);
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Duration");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 12);
    ImGui::InputFloat("##DurationSeconds", &duration_seconds, 15, 1, "%.0f");
    def_.set_duration_seconds(duration_seconds);

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Score range");
    ImGui::SameLine();
    ImGui::InputFloat(ImGui::InputFloatParams("StartScore")
                          .set_is_optional()
                          .set_zero_is_unset()
                          .set_min(0)
                          .set_step(0.1, 2)
                          .set_width(char_x_ * 12),
                      PROTO_FLOAT_FIELD(ScenarioDef, &def_, start_score));
    if (def_.start_score() > 0) {
      ImGui::SameLine();
      ImGui::Text("to");
      ImGui::SameLine();
      ImGui::InputFloat(ImGui::InputFloatParams("EndScore")
                            .set_zero_is_unset()
                            .set_min(0)
                            .set_step(0.1, 2)
                            .set_width(char_x_ * 12),
                        PROTO_FLOAT_FIELD(ScenarioDef, &def_, end_score));
      if (def_.end_score() < def_.start_score()) {
        def_.set_end_score(def_.start_score());
      }
    } else {
      def_.clear_end_score();
    }

    ImGui::SpacedSeparator();

    ImGui::Text("Overrides");
    ImGui::Indent();
    DrawOverridesEditor("Overrides", def_.mutable_overrides());
    if (ImGui::Button("Bake")) {
      def_ = ApplyScenarioOverrides(def_);
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Apply and remove the overrides.");
    ImGui::Unindent();

    ImGui::SpacedSeparator();

    ImGui::Text("Level overrides");
    ImGui::Indent();
    DrawOverridesEditor("LevelOverrides", def_.mutable_level_overrides(), /*is_levels=*/true);
    ImGui::Unindent();

    ImGui::SpacedSeparator();

    if (ImGui::Button("Edit room")) {
      editing_room_ = true;
    }

    if (ImGui::Button("Edit description")) {
      editing_description_ = true;
    }

    ImGui::SpacedSeparator();

    if (original_name_.has_value()) {
      if (ImGui::Button("Make changes in new copy")) {
        original_name_ = {};
        *name_.mutable_relative_name() = name_.relative_name() + " Copy";
      }
      ImGui::HelpTooltip(
          "Save the current changes in a new copy of the scenario leaving the original "
          "unchanged.");
    }

    if (ImGui::Button("View Json")) {
      SetErrorMessage(MessageToJson(def_, 6));
    }

    if (ImGui::Button("Compare")) {
      comparison_window_open_ = !comparison_window_open_;
    }
    ImGui::SameLine();
    ImGui::HelpMarker(
        "Open a window displaying values from another scenario. Useful if you want to copy the "
        "strafe patterns from another scenario.");
  }

  // Returns whether the screen should close
  bool SaveScenario() {
    if (name_.bundle_name().size() == 0 || name_.relative_name().size() == 0) {
      SetErrorMessage("Missing scenario name");
      return false;
    }

    NameInfo name_info = GetScenarioNameInfo(name_.full_name());
    if (name_info.HasDynamicSuffix()) {
      SetErrorMessage(
          "Unable to save scenario with name ending in 'L#' or '#cm'. These scenarios are "
          "automatically defined.");
      return false;
    }

    auto& mgr = app_.scenario_manager();

    bool is_new_file =
        !original_name_.has_value() || original_name_->full_name() != name_.full_name();
    if (is_new_file) {
      auto existing_scenario_with_name = mgr.GetScenario(name_.full_name());
      if (existing_scenario_with_name.has_value()) {
        SetErrorMessage(std::format("Scenario \"{}\" already exists", name_.full_name()));
        return false;
      }

      if (original_name_.has_value()) {
        mgr.RenameScenario(original_name_->full_name(), name_.full_name());
      }
    }

    if (add_to_playlist_.size() > 0) {
      app_.playlist_manager().AddScenarioToPlaylist(add_to_playlist_, name_.full_name());
    }

    mgr.UpdateScenario(name_.full_name(), def_);
    if (!app_.bundle_manager().SaveDirtyBundles()) {
      SetErrorMessage("Unable to save bundle to disk.");
      return false;
    }

    app_.scenario_manager().SetCurrentScenario(name_.full_name());
    app_.history_manager().UpdateRecentView(ObjectType::SCENARIO, name_.full_name());
    return true;
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

  void DrawReferenceEditor() {
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario type");
    ImGui::SameLine();

    auto scenario_type = def_.type_case();
    ImGui::SimpleTypeDropdown("ScenarioTypeDropdown", &scenario_type, kScenarioTypes, char_x_ * 15);
    InitializeScenarioType(def_, scenario_type);

    if (scenario_type != ScenarioDef::kReferenceDef) {
      return;
    }

    ImGui::SpacedSeparator();

    // Make sure only the appropriate fields are set on the def.
    ScenarioDef old_def = def_;

    def_ = {};
    def_.set_description(old_def.description());
    *def_.mutable_overrides() = old_def.overrides();
    *def_.mutable_reference_def() = old_def.reference_def();

    ReferenceScenarioDef& r = *def_.mutable_reference_def();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Scenario");
    ImGui::SameLine();
    ImGui::HelpMarker("The name of the scenario to reference");
    ImGui::SameLine();
    ImGui::InputText("##ScenarioReference", r.mutable_scenario_name());

    if (r.scenario_name().size() > 0) {
      auto matching_scenario = app_.scenario_manager().GetScenario(r.scenario_name());
      if (!matching_scenario) {
        // Show search results for scenarios.
        int num_matches = 0;
        auto search_words = GetSearchWords(r.scenario_name());
        ImGui::Indent();
        for (const std::string& scenario_name : *app_.scenario_manager().scenario_names()) {
          if (StringMatchesSearch(scenario_name, search_words)) {
            num_matches++;
            if (ImGui::Button(scenario_name)) {
              r.set_scenario_name(scenario_name);
            }
          }
        }
        if (num_matches == 0) {
          ImGui::Text("No matching scenarios found");
        }
        ImGui::Unindent();
      }
    }

    ImGui::SpacedSeparator();

    ImGui::Text("Overrides");
    ImGui::Indent();
    DrawOverridesEditor("ReferenceOverrides", def_.mutable_overrides());
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Spacing();

    if (ImGui::Button("Bake")) {
      auto parent = app_.scenario_manager().GetEvaluatedScenarioDef(r.scenario_name());
      if (parent) {
        auto overrides = def_.overrides();
        def_ = ApplyScenarioOverrides(*parent);
        *def_.mutable_overrides() = overrides;
        def_ = ApplyScenarioOverrides(def_);
      } else {
        SetErrorMessage(std::format("Referenced scenario \"{}\" is invalid.", r.scenario_name()));
      }
    }
    ImGui::SameLine();
    ImGui::HelpMarker("Expand and remove the reference. Will now be an equivalent normal scenario");
  }

  void DrawOverridesEditor(const char* id, ScenarioOverrides* overrides, bool is_levels = false) {
    ImGui::IdGuard cid(id);
    ImGui::InputFloat(ImGui::InputFloatParams("TargetRadiusMult")
                          .set_label("Target radius multiplier")
                          .set_step(0.01, 0.25)
                          .set_min(0.01)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(ScenarioOverrides, overrides, target_radius_multiplier));

    ImGui::InputFloat(ImGui::InputFloatParams("SpeedMult")
                          .set_label("Speed multiplier")
                          .set_step(0.01, 0.25)
                          .set_min(0.01)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(ScenarioOverrides, overrides, speed_multiplier));

    ImGui::InputFloat(ImGui::InputFloatParams("AccelMult")
                          .set_label("Acceleration multiplier")
                          .set_step(0.01, 0.25)
                          .set_min(0.01)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(ScenarioOverrides, overrides, acceleration_multiplier));

    ImGui::InputFloat(ImGui::InputFloatParams("TimeScale")
                          .set_label("Time scale multiplier")
                          .set_step(0.01, 0.25)
                          .set_min(0.01)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(ScenarioOverrides, overrides, time_scale_multiplier));
    ImGui::InputFloat(ImGui::InputFloatParams("Distance")
                          .set_label("Distance multiplier")
                          .set_step(0.01, 0.25)
                          .set_min(0.01)
                          .set_default(1)
                          .set_is_optional()
                          .set_width(char_x_ * 10),
                      PROTO_FLOAT_FIELD(ScenarioOverrides, overrides, distance_multiplier));
  }

  void DrawRoomEditor() {
    ImGui::IdGuard cid("RoomEditor");
    ImGui::SetNextWindowBgAlpha(0.4f);
    float width = char_x_ * 25;
    float height = app_.screen_info().height * 0.75;

    ImGui::SetNextWindowPos(ImVec2(char_x_ * 0.3, (app_.screen_info().height - height) / 2.0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    if (ImGui::Begin("Room", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove)) {
      if (ImGui::Button(std::format("{} Back to editor", kIconArrowBack))) {
        editing_room_ = false;
      }
      ImGui::SpacedSeparator();
      DrawRoomEditorInputs(*def_.mutable_room());
    }
    ImGui::End();
  }

  void Render() override {
    if (!editing_room_) {
      UiScreen::Render();
      return;
    }

    target_manager_.UpdateRoom(def_.room());
    CameraParams camera_params(def_.room());
    Camera camera(camera_params);
    auto look_at = camera.GetLookAt();

    RenderContext ctx;
    Stopwatch stopwatch;
    FrameTimes frame_times;
    if (app_.StartRender(&ctx)) {
      auto projection =
          GetPerspectiveTransformation(app_.screen_info(), def_.room().horizontal_fov());
      app_.renderer()->DrawScenario(projection,
                                    def_.room(),
                                    theme_,
                                    settings_.health_bar(),
                                    target_manager_.GetTargets(),
                                    look_at,
                                    &ctx,
                                    stopwatch,
                                    &frame_times);
      app_.FinishRender(&ctx);
    }
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {
    if (user_is_typing) {
      return;
    }
    if (IsMappableKeyDownEvent(event)) {
      std::string event_name = absl::AsciiStrToLower(GetKeyNameForEvent(event));
      bool is_restart = KeyMappingMatchesEvent(
          event_name, app_.settings_manager().GetCurrentSettings().keybinds().restart_scenario());
      bool is_next = KeyMappingMatchesEvent(
          event_name, app_.settings_manager().GetCurrentSettings().keybinds().next_scenario());
      if (is_restart || is_next) {
        PlayScenario();
      }
    }
  }

 private:
  void PlayScenario() {
    CreateScenarioParams params;
    if (def_.has_reference_def()) {
      auto base_scenario =
          app_.scenario_manager().GetEvaluatedScenarioDef(def_.reference_def().scenario_name());
      if (!base_scenario) {
        SetErrorMessage(std::format("Unable to find referenced scenario \"{}\"",
                                    def_.reference_def().scenario_name()));
        return;
      }
      params.def = ApplyScenarioOverrides(*base_scenario);
      if (def_.has_overrides()) {
        *params.def.mutable_overrides() = def_.overrides();
      }
    } else {
      params.def = def_;
    }
    params.def.set_duration_seconds(1000000);
    params.name = name_.full_name();
    params.force_start_immediately = true;
    params.from_scenario_editor = true;
    PushNextScreen(CreateScenario(params, &app_));
  }

  ScenarioDef def_;
  TargetManager target_manager_;
  Theme theme_;
  float char_x_ = 0;
  ImVec2 char_size_{};

  std::vector<std::string> bundle_names_;
  std::optional<ResourceName> original_name_;
  ResourceName name_;
  Settings settings_;

  std::string add_to_playlist_;

  ImGui::NotificationPopup notification_popup_{"Notification"};

  bool editing_room_ = false;
  bool editing_description_ = false;

  bool comparison_window_open_ = false;
  std::string comparison_scenario_;
};

}  // namespace

std::unique_ptr<UiScreen> CreateScenarioEditorScreen(const ScenarioEditorOptions& options,
                                                     Application* app) {
  return std::make_unique<ScenarioEditorScreen>(options, *app);
}

}  // namespace aim
