#include "theme_editor_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/core/camera.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"

namespace aim {
namespace {

constexpr const char* kSolidColorItem = "Solid color";
constexpr const char* kTextureItem = "Texture";

struct StoredColorEditor {
  StoredColorEditor(std::string label, std::string id) : label(label), id(id) {}

  std::string label;
  std::string id;
  StoredColor* stored_color = nullptr;
  float color[3];
  float multiplier = 0;

  void Draw(const ImVec2& char_size) {
    if (stored_color == nullptr) {
      return;
    }
    ImGui::IdGuard cid(id);

    ImGui::AlignTextToFramePadding();
    ImGui::Text(label);
    ImGui::SameLine();
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

    multiplier = stored_color->multiplier();
    ImGui::SameLine();
    ImGui::Text("multiplier");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(char_size.x * 9);
    ImGui::InputFloat("##MultiplierInput", &multiplier, 0.01, 0.2, "%.2f");
    if (multiplier > 0) {
      stored_color->set_multiplier(multiplier);
    } else {
      stored_color->clear_multiplier();
    }
  }
};

struct WallAppearanceEditor {
  WallAppearanceEditor(std::string label, std::string id)
      : label(label),
        id(id),
        color_editor("Color", "ColorEditor" + id),
        mix_color_editor("MixColor", "MixColorEditor" + id) {}

  std::string label;
  std::string id;
  WallAppearance* appearance = nullptr;

  StoredColorEditor color_editor;
  StoredColorEditor mix_color_editor;

  std::string texture_name;

  void Draw(const std::string& header,
            const std::vector<std::string>& texture_names,
            const ImVec2& char_size) {
    if (appearance == nullptr) {
      return;
    }
    ImGui::IdGuard cid(id);

    ImGui::SetNextItemWidth(char_size.x * 20);
    bool opened = ImGui::TreeNode(header.c_str());
    if (opened) {
      std::string selected_type = kSolidColorItem;
      if (appearance->has_texture()) {
        selected_type = kTextureItem;
      }

      std::vector<std::string> types = {kSolidColorItem, kTextureItem};
      ImGui::SimpleDropdown("WallTypeDropdown", &selected_type, types, char_size.x * 20);

      if (selected_type == kSolidColorItem) {
        color_editor.stored_color = appearance->mutable_color();
        color_editor.Draw(char_size);
      }
      if (selected_type == kTextureItem) {
        WallTexture* texture = appearance->mutable_texture();
        ImGui::SimpleDropdown("TextureNameDropdown",
                              texture->mutable_texture_name(),
                              texture_names,
                              char_size.x * 20);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("Scale");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(char_size.x * 9);
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
      ImGui::SetNextItemWidth(char_size.x * 9);
      float mix_percent = appearance->mix_percent();
      ImGui::InputFloat("##MixPercent", &mix_percent, 0.02, 0.2, "%.2f");
      if (mix_percent > 0) {
        appearance->set_mix_percent(mix_percent);
        mix_color_editor.stored_color = appearance->mutable_mix_color();
        mix_color_editor.Draw(char_size);
      } else {
        appearance->clear_mix_percent();
      }
      ImGui::TreePop();
    }
  }
};

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
    theme_names_ = app.settings_manager().ListThemes();
    if (theme_names_.size() > 0) {
      UpdateCurrentTheme(theme_names_[0]);
    }

    projection_ = GetPerspectiveTransformation(app_.screen_info());
    CameraParams cameraParams(default_room_);
    Camera camera(cameraParams);
    look_at_ = camera.GetLookAt();

    Target t;
    t.radius = 3;
    t.wall_position = glm::vec2(20, 20);
    t.health_seconds = 3;
    t.hit_timer.AddElapsedSeconds(1);

    Target g = t;
    g.is_ghost = true;
    g.wall_position = glm::vec2(20, -20);
    g.health_seconds = 3;
    g.hit_timer.AddElapsedSeconds(1);

    target_manager_.AddTarget(t);
    target_manager_.AddTarget(g);
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_.screen_info();
    ImVec2 char_size = ImGui::CalcTextSize("A");
    char_x_ = char_size.x;

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Theme");
    ImGui::SameLine();
    if (ImGui::SimpleDropdown(
            "ThemeDropdown", &current_theme_name_, theme_names_, char_size.x * 20)) {
      UpdateCurrentTheme(current_theme_name_);
      app_.history_manager().UpdateRecentView(RecentViewType::THEME, current_theme_name_);
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Targets");
    ImGui::Indent();
    target_color_.Draw(char_size);
    ghost_target_color_.Draw(char_size);
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Crosshair");
    ImGui::Indent();
    crosshair_color_.Draw(char_size);
    crosshair_outline_color_.Draw(char_size);
    ImGui::Unindent();

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Health bar");
    HealthBarAppearance& health_bar = *current_theme_.mutable_health_bar();
    ImGui::Indent();
    health_color_.Draw(char_size);

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

    health_background_color_.Draw(char_size);

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

    ImGui::AlignTextToFramePadding();
    ImGui::Text("Walls");
    ImGui::Indent();
    front_.Draw("Front", texture_names_, char_size);
    sides_.Draw("Sides", texture_names_, char_size);
    roof_.Draw("Roof", texture_names_, char_size);
    floor_.Draw("Floor", texture_names_, char_size);
    back_.Draw("Back", texture_names_, char_size);
    ImGui::Unindent();

    ImGui::Spacing();
    ImGui::Spacing();

    {
      ImVec2 sz = ImVec2(char_size.x * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
        app_.settings_manager().SaveThemeToDisk(current_theme_name_, current_theme_);
        PopSelf();
      }
    }
    {
      ImGui::SameLine();
      ImVec2 sz = ImVec2(0, 0.0f);
      if (ImGui::Button("Cancel", sz)) {
        PopSelf();
      }
    }

    Crosshair crosshair;
    crosshair.add_layers()->mutable_dot()->set_outline_thickness(2);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    DrawCrosshair(crosshair, 25, current_theme_, app_.screen_info().center);
  }

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {}

  void Render() override {
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

    crosshair_color_.stored_color = current_theme_.mutable_crosshair()->mutable_color();
    crosshair_outline_color_.stored_color =
        current_theme_.mutable_crosshair()->mutable_outline_color();

    target_color_.stored_color = current_theme_.mutable_target_color();
    ghost_target_color_.stored_color = current_theme_.mutable_ghost_target_color();

    health_color_.stored_color = current_theme_.mutable_health_bar()->mutable_health_color();
    health_background_color_.stored_color =
        current_theme_.mutable_health_bar()->mutable_background_color();

    front_.appearance = current_theme_.mutable_front_appearance();
    sides_.appearance = current_theme_.mutable_side_appearance();
    back_.appearance = current_theme_.mutable_back_appearance();
    floor_.appearance = current_theme_.mutable_floor_appearance();
    roof_.appearance = current_theme_.mutable_roof_appearance();
  }

  Room default_room_;
  TargetManager target_manager_;
  std::vector<std::string> theme_names_;
  std::string current_theme_name_;
  glm::mat4 projection_;

  Theme current_theme_;
  LookAtInfo look_at_;

  StoredColorEditor target_color_{"Target color", "TargetColorEditor"};
  StoredColorEditor ghost_target_color_{"Ghost target color", "GhostTargetColorEditor"};

  StoredColorEditor crosshair_color_{"Crosshair color", "CrosshairColorEditor"};
  StoredColorEditor crosshair_outline_color_{"Crosshair outline color",
                                             "CrosshairOutlineColorEditor"};

  StoredColorEditor health_color_{"Health color", "HealthColorEditor"};
  StoredColorEditor health_background_color_{"Background color", "HealthBackgroundColorEditor"};

  WallAppearanceEditor front_{"Front", "FrontEditor"};
  WallAppearanceEditor sides_{"Sides", "SidesEditor"};
  WallAppearanceEditor roof_{"Roof", "RoofEditor"};
  WallAppearanceEditor floor_{"Floor", "FloorEditor"};
  WallAppearanceEditor back_{"Back", "BackEditor"};

  std::vector<std::string> texture_names_;

  float char_x_;
};
}  // namespace

std::unique_ptr<UiScreen> CreateThemeEditorScreen(Application* app) {
  return std::make_unique<ThemeEditorScreen>(*app);
}

}  // namespace aim
