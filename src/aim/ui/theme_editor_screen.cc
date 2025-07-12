#include "theme_editor_screen.h"

#include <format>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/core/camera.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"

namespace aim {
namespace {

const float kBackgroundAlpha = 0.6;
constexpr const char* kSolidColorItem = "Solid color";
constexpr const char* kTextureItem = "Texture";

Room GetDefaultRoom() {
  Room r;
  r.mutable_simple_room()->set_height(130);
  r.mutable_simple_room()->set_width(150);
  *r.mutable_camera_position() = ToStoredVec3(0, -100, 0);
  return r;
}

class ThemeEditorScreen : public UiScreen {
 public:
  explicit ThemeEditorScreen(Application& app)
      : UiScreen(app), default_room_(GetDefaultRoom()), target_manager_(default_room_) {
    texture_names_ = app.settings_manager().ListTextures();
    LoadThemeList();

    projection_ = GetPerspectiveTransformation(app_.screen_info());
    CameraParams cameraParams(default_room_);
    Camera camera(cameraParams);
    look_at_ = camera.GetLookAt();

    Target t;
    t.radius = 3;
    t.wall_position = glm::vec2(20, 20);
    t.health_seconds = 3;
    t.AddTestDamage();

    Target g = t;
    g.is_ghost = true;
    g.wall_position = glm::vec2(20, -20);
    g.health_seconds = 3;
    g.AddTestDamage();

    target_manager_.AddTarget(t);
    target_manager_.AddTarget(g);
  }

 protected:
  void DrawTopBar() {
    float width = char_x_ * 13.6;
    float middle = app_.screen_info().width / 2.0;
    // ImGui::SetNextWindowBgAlpha();
    ImGui::SetNextWindowPos(ImVec2(middle - width / 2.0, char_x_ / 3.0));
    ImGui::SetNextWindowSize(ImVec2(width, -1));
    ImGui::Begin("TopBar", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);

    if (ImGui::Button(std::format("{} Save", kIconSave))) {
      if (SaveCurrentTheme()) {
        BackToThemeList();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(std::format("{} Back", kIconArrowBack))) {
      BackToThemeList();
    }

    ImGui::End();
  }

  void Line() {
    ImGui::Spacing();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::Spacing();
  }

  bool SaveCurrentTheme() {
    bool theme_exists = app_.settings_manager().ThemeExists(current_theme_name_);
    bool is_rename = !is_new_theme_ && current_theme_name_ != original_theme_name_;
    if (is_new_theme_ || is_rename) {
      if (theme_exists) {
        notification_popup_.NotifyOpen(
            std::format("Theme \"{}\" already exists", current_theme_name_));
        return false;
      }
    }

    if (is_rename) {
      app_.settings_manager().RenameTheme(original_theme_name_, current_theme_name_);
    }

    if (!app_.settings_manager().SaveTheme(current_theme_name_, current_theme_)) {
      notification_popup_.NotifyOpen(
          std::format("Unable to save theme \"{}\"", current_theme_name_));
      return false;
    }
    LoadThemeList();
    return true;
  }

  bool BeginMainWindow(const std::string& name) {
    float width = app_.screen_info().width * 0.6;
    float height = app_.screen_info().height * 0.8;

    ImGui::SetNextWindowPos(ImVec2((app_.screen_info().width - width) / 2.0,
                                   (app_.screen_info().height - height) / 2.0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    return ImGui::Begin(
        name.c_str(), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
  }

  void DrawScreen() override {
    const ScreenInfo& screen = app_.screen_info();
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    if (current_theme_name_.size() == 0) {
      BeginMainWindow("ThemeList");
      notification_popup_.Draw();
      delete_confirmation_dialog_.Draw("Delete", [=](const std::string& to_delete) {
        app_.settings_manager().DeleteTheme(to_delete);
        LoadThemeList();
      });
      DrawThemeListEditor();

      ImGui::End();
      return;
    }

    DrawTopBar();

    float width = char_x_ * 40;
    float height = app_.screen_info().height * 0.95;
    ImGui::SetNextWindowBgAlpha(kBackgroundAlpha);
    ImGui::SetNextWindowPos(ImVec2(char_x_ * 0.3, (app_.screen_info().height - height) / 2.0));
    ImGui::SetNextWindowSize(ImVec2(width, height));
    ImGui::Begin("ThemeEditor",
                 nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize);
    notification_popup_.Draw();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Theme");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 20);
    ImGui::InputText("##NameInput", &current_theme_name_);

    bool is_reference = current_theme_.has_reference();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Reference");
    ImGui::SameLine();
    ImGui::Checkbox("##ReferenceCheck", &is_reference);

    Line();

    if (is_reference) {
      std::string original_name = current_theme_.name();
      std::string original_reference = current_theme_.reference();

      current_theme_.Clear();
      current_theme_.set_name(original_name);
      current_theme_.set_reference(original_reference);

      std::string* reference = current_theme_.mutable_reference();
      if (reference->size() == 0) {
        *reference = theme_names_[0];
      }
      ImGui::SimpleDropdown("ReferenceThemeSelector", reference, theme_names_, char_x_ * 20);

      if (ImGui::Button("Edit referenced theme")) {
        std::string referenced_copy = *reference;
        BackToThemeList();
        OpenExistingTheme(referenced_copy);
      }

      ImGui::End();
    } else {
      current_theme_.clear_reference();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Targets");
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Color");
      ImGui::SameLine();
      DrawStoredColorEditor("TargetColor", current_theme_.mutable_target_color());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Ghost color");
      ImGui::SameLine();
      DrawStoredColorEditor("GhostTargetColor", current_theme_.mutable_ghost_target_color());

      ImGui::Unindent();

      Line();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Crosshair");
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Color");
      ImGui::SameLine();
      DrawStoredColorEditor("CrosshairColor", current_theme_.mutable_crosshair()->mutable_color());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Outline color");
      ImGui::SameLine();
      DrawStoredColorEditor("OutlineCrosshairColor",
                            current_theme_.mutable_crosshair()->mutable_outline_color());

      ImGui::Unindent();

      Line();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Health bar");
      HealthBarAppearance& health_bar = *current_theme_.mutable_health_bar();
      ImGui::Indent();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Health color");
      ImGui::SameLine();
      DrawStoredColorEditor("HealthColor", health_bar.mutable_health_color());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Health alpha");
      ImGui::SameLine();
      bool has_health_alpha = health_bar.has_health_alpha();
      float health_alpha = health_bar.health_alpha();
      ImGui::OptionalInputFloat(
          "HealthAlpha", &has_health_alpha, &health_alpha, 0.05, 0.2, "%.2f", char_x_ * 9);
      if (has_health_alpha) {
        health_bar.set_health_alpha(health_alpha);
      } else {
        health_bar.clear_health_alpha();
      }

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Background color");
      ImGui::SameLine();
      DrawStoredColorEditor("HealthBackgroundColor", health_bar.mutable_background_color());

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Background alpha");
      ImGui::SameLine();
      bool has_background_alpha = health_bar.has_background_alpha();
      float background_alpha = health_bar.background_alpha();
      ImGui::OptionalInputFloat("BackgroundAlpha",
                                &has_background_alpha,
                                &background_alpha,
                                0.05,
                                0.2,
                                "%.2f",
                                char_x_ * 9);
      if (has_background_alpha) {
        health_bar.set_background_alpha(background_alpha);
      } else {
        health_bar.clear_background_alpha();
      }
      ImGui::Unindent();

      Line();

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Walls");
      ImGui::Indent();
      DrawWallAppearanceEditor("Front", current_theme_.mutable_front_appearance());
      DrawWallAppearanceEditor("Sides", current_theme_.mutable_side_appearance());
      DrawWallAppearanceEditor("Floor", current_theme_.mutable_floor_appearance());
      DrawWallAppearanceEditor("Roof", current_theme_.mutable_roof_appearance());
      DrawWallAppearanceEditor("Back", current_theme_.mutable_back_appearance());
      ImGui::Unindent();

      ImGui::Spacing();
      ImGui::Spacing();

      Crosshair crosshair;
      crosshair.add_layers()->mutable_dot()->set_outline_thickness(2);

      ImGui::End();

      ImGui::SetNextWindowPos(
          ImVec2(app_.screen_info().center.x - 30, app_.screen_info().center.y - 30));
      ImGui::SetNextWindowSize(ImVec2(60, 60));
      ImGui::SetNextWindowBgAlpha(0);
      ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
      ImGui::Begin(
          "CrosshairWindow", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
      ImDrawList* draw_list = ImGui::GetWindowDrawList();
      app_.crosshair_manager().Draw(crosshair, 25, current_theme_, app_.screen_info().center);
      ImGui::End();
      ImGui::PopStyleVar();
    }
  }

  void DrawThemeListEditor() {
    ImGui::IdGuard cid("ThemeListEditor");
    ImGui::LoopId loop_id;
    if (ImGui::Button(std::format("{} Back", kIconArrowBack))) {
      PopSelf();
    }
    Line();
    if (ImGui::Button(std::format("{} New theme", kIconAdd))) {
      OpenNewTheme();
    }
    Line();
    ImGui::BeginChild("ThemesListContent");
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Themes");
    ImGui::Indent();
    for (const std::string& name : theme_names_) {
      auto lid = loop_id.Get();
      if (ImGui::Button(name)) {
        OpenExistingTheme(name);
      }
      const char* popup_id = "ThemeItemMenu";
      if (ImGui::BeginPopupContextItem(popup_id)) {
        if (ImGui::Selectable("Copy")) {
          OpenThemeCopy(name);
        }
        if (ImGui::Selectable("Edit")) {
          OpenExistingTheme(name);
        }
        if (ImGui::Selectable("Delete")) {
          delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", name), name);
        }
        ImGui::EndPopup();
      }
      ImGui::OpenPopupOnItemClick(popup_id, ImGuiPopupFlags_MouseButtonRight);
    }
    ImGui::Unindent();
    ImGui::EndChild();
  }

  void OpenExistingTheme(const std::string& name) {
    current_theme_name_ = name;
    original_theme_name_ = name;
    is_new_theme_ = false;
    current_theme_ = app_.settings_manager().GetThemeNoReferenceFollow(name);
  }

  void OpenThemeCopy(const std::string& name) {
    current_theme_name_ = MakeUniqueName(name + " Copy", theme_names_);
    original_theme_name_ = current_theme_name_;
    is_new_theme_ = true;
    current_theme_ = app_.settings_manager().GetThemeNoReferenceFollow(name);
  }

  void OpenNewTheme() {
    current_theme_name_ = MakeUniqueName("New theme", theme_names_);
    original_theme_name_ = current_theme_name_;
    is_new_theme_ = true;
    current_theme_ = GetDefaultTheme();
  }

  void BackToThemeList() {
    current_theme_ = {};
    current_theme_name_ = "";
    is_new_theme_ = false;
  }

  void DrawStoredColorEditor(const std::string& id, StoredColor* stored_color) {
    ImGui::IdGuard cid(id);

    float color[3];
    StoredRgb c = ToStoredRgb(*stored_color);
    color[0] = c.r() / 255.0;
    color[1] = c.g() / 255.0;
    color[2] = c.b() / 255.0;
    if (ImGui::ColorEdit3(
            "##ColorEditor", color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
      StoredRgb result = FloatToStoredRgb(color[0], color[1], color[2]);
      if (stored_color->has_hex()) {
        stored_color->set_hex(ToHexString(result));
        stored_color->clear_r();
        stored_color->clear_b();
        stored_color->clear_g();
      } else {
        stored_color->set_r(result.r());
        stored_color->set_g(result.g());
        stored_color->set_b(result.b());
      }
    }

    ImGui::SameLine();
    ImGui::Text(kIconClose);
    ImGui::HelpTooltip("Multiply color by value");

    ImGui::SameLine();
    ImGui::InputFloat(ImGui::InputFloatParams("ColorMultiplier")
                          .set_step(0.01, 0.2)
                          .set_precision(2)
                          .set_max(2)
                          .set_width(char_x_ * 10)
                          .set_zero_is_unset(),
                      PROTO_FLOAT_FIELD(StoredColor, stored_color, multiplier));
  }

  void DrawWallAppearanceEditor(const std::string& header, WallAppearance* appearance) {
    ImGui::IdGuard cid("WallAppearance" + header);

    ImGui::SetNextItemWidth(char_x_ * 20);
    ImGui::Text(header);
    ImGui::Indent();
    std::string selected_type = kSolidColorItem;
    if (appearance->has_texture()) {
      selected_type = kTextureItem;
    }

    std::vector<std::string> types = {kSolidColorItem, kTextureItem};
    ImGui::SimpleDropdown("WallTypeDropdown", &selected_type, types, char_x_ * 20);

    if (selected_type == kSolidColorItem) {
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Color");
      ImGui::SameLine();
      ImGui::InputStoredColor("##Color", appearance->mutable_color(), char_x_);
    }
    if (selected_type == kTextureItem) {
      WallTexture* texture = appearance->mutable_texture();
      ImGui::SimpleDropdown(
          "TextureNameDropdown", texture->mutable_texture_name(), texture_names_, char_x_ * 20);

      ImGui::AlignTextToFramePadding();
      ImGui::Text("Scale");
      ImGui::SameLine();
      ImGui::SetNextItemWidth(char_x_ * 9);
      float scale = texture->scale();
      ImGui::InputFloat("##TextureScale", &scale, 0.1, 1, "%.1f");
      if (scale > 0) {
        texture->set_scale(scale);
      } else {
        texture->clear_scale();
      }
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Mix percent");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_x_ * 9);
    float mix_percent = appearance->mix_percent();
    ImGui::InputFloat("##MixPercent", &mix_percent, 0.02, 0.2, "%.2f");
    if (mix_percent > 0) {
      appearance->set_mix_percent(mix_percent);
      ImGui::AlignTextToFramePadding();
      ImGui::Text("Mix color");
      ImGui::SameLine();
      DrawStoredColorEditor("MixColor", appearance->mutable_mix_color());
    } else {
      appearance->clear_mix_percent();
    }
    ImGui::Unindent();
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {}

  void Render() override {
    if (current_theme_name_.size() == 0) {
      UiScreen::Render();
      return;
    }

    RenderContext ctx;
    Stopwatch stopwatch;
    FrameTimes frame_times;
    if (app_.StartRender(&ctx)) {
      HealthBarSettings health_bar;
      health_bar.set_show(true);
      health_bar.set_width(8);
      health_bar.set_height(2);
      app_.renderer()->DrawScenario(projection_,
                                    default_room_,
                                    current_theme_,
                                    health_bar,
                                    target_manager_.GetTargets(),
                                    look_at_,
                                    &ctx,
                                    stopwatch,
                                    &frame_times);
      app_.FinishRender(&ctx);
    }
  }

 private:
  void UpdateCurrentTheme(const std::string& theme_name) {
    current_theme_name_ = theme_name;
    current_theme_ = app_.settings_manager().GetTheme(current_theme_name_);
  }

  void LoadThemeList() {
    theme_names_ = app_.settings_manager().ListThemes();
    std::sort(theme_names_.begin(),
              theme_names_.end(),
              [](const std::string& lhs, const std::string& rhs) { return lhs < rhs; });
  }

  Room default_room_;
  TargetManager target_manager_;
  glm::mat4 projection_;
  LookAtInfo look_at_;

  std::vector<std::string> theme_names_;
  std::vector<std::string> texture_names_;

  Theme current_theme_;
  std::string original_theme_name_;
  std::string current_theme_name_;
  bool is_new_theme_ = false;

  float char_x_;

  ImGui::NotificationPopup notification_popup_{"Notification"};
  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
};

}  // namespace

std::unique_ptr<UiScreen> CreateThemeEditorScreen(Application* app) {
  return std::make_unique<ThemeEditorScreen>(*app);
}

}  // namespace aim
