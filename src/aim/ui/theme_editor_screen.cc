#include "theme_editor_screen.h"

#include <backends/imgui_impl_sdl3.h>
#include <misc/cpp/imgui_stdlib.h>

#include <format>
#include <optional>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/core/camera.h"
#include "aim/core/navigation_event.h"
#include "aim/core/settings_manager.h"
#include "aim/graphics/crosshair.h"

namespace aim {
namespace {

struct StoredColorEditor {
  StoredColorEditor(std::string label, std::string id) : label(label), id(id) {}

  std::string label;
  std::string id;
  StoredColor* stored_color = nullptr;
  float color[3];
  std::string multiplier;

  void Draw(const ImVec2& char_size) {
    if (stored_color == nullptr) {
      return;
    }

    ImGui::TextFmt("{}", label);
    ImGui::SameLine();
    StoredRgb c = ToStoredRgb(*stored_color);
    color[0] = c.r() / 255.0;
    color[1] = c.g() / 255.0;
    color[2] = c.b() / 255.0;
    if (ImGui::ColorEdit3(
            id.c_str(), color, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel)) {
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

    // Multiplier
    multiplier =
        stored_color->has_multiplier() ? std::format("{}", stored_color->multiplier()) : "";
    ImGui::SameLine();
    ImGui::Text("mult");
    ImGui::SameLine();
    std::string mult_id = "##Multiplier" + id;
    ImGui::PushItemWidth(char_size.x * 5);
    ImGui::InputText(mult_id.c_str(), &multiplier, ImGuiInputTextFlags_CharsDecimal);
    ImGui::PopItemWidth();

    if (multiplier.size() > 0) {
      stored_color->set_multiplier(ParseFloat(multiplier));
    } else {
      stored_color->clear_multiplier();
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
  explicit ThemeEditorScreen(Application* app)
      : UiScreen(app), default_room_(GetDefaultRoom()), target_manager_(default_room_) {
    theme_names_ = app->settings_manager()->ListThemes();
    if (theme_names_.size() > 0) {
      UpdateCurrentTheme(theme_names_[0]);
    }

    projection_ = GetPerspectiveTransformation(app_->screen_info());
    CameraParams cameraParams(default_room_);
    Camera camera(cameraParams);
    look_at_ = camera.GetLookAt();

    Target t;
    t.radius = 3;
    t.wall_position = glm::vec2(20, 20);

    Target g = t;
    g.is_ghost = true;
    g.wall_position = glm::vec2(20, -20);

    target_manager_.AddTarget(t);
    target_manager_.AddTarget(g);
  }

 protected:
  void DrawScreen() override {
    const ScreenInfo& screen = app_->screen_info();
    ImVec2 char_size = ImGui::CalcTextSize("A");
    ImGuiComboFlags combo_flags = 0;
    ImGui::Text("Theme");
    ImGui::SameLine();
    ImGui::PushItemWidth(char_size.x * 20);
    if (ImGui::BeginCombo("##theme_combo", current_theme_name_.c_str(), combo_flags)) {
      for (int i = 0; i < theme_names_.size(); ++i) {
        auto& theme_name = theme_names_[i];
        bool is_selected = theme_name == current_theme_name_;
        if (ImGui::Selectable(std::format("{}##{}theme_name", theme_name, i).c_str(),
                              is_selected)) {
          UpdateCurrentTheme(theme_name);
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }
    ImGui::PopItemWidth();

    target_color_.Draw(char_size);
    ghost_target_color_.Draw(char_size);
    crosshair_color_.Draw(char_size);
    crosshair_outline_color_.Draw(char_size);

    {
      ImVec2 sz = ImVec2(char_size.x * 14, 0.0f);
      if (ImGui::Button("Save", sz)) {
        app_->settings_manager()->SaveThemeToDisk(current_theme_name_, current_theme_);
        ScreenDone();
      }
    }
    {
      ImGui::SameLine();
      ImVec2 sz = ImVec2(0, 0.0f);
      if (ImGui::Button("Cancel", sz)) {
        ScreenDone();
      }
    }

    Crosshair crosshair;
    crosshair.mutable_dot()->set_draw_outline(true);
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    DrawCrosshair(crosshair, 25, current_theme_, app_->screen_info(), draw_list);
  }

  void WallAppearanceEditor(WallAppearance* appearance) {}

  void OnEvent(const SDL_Event& event, bool user_is_typing) override {}

  void Render() override {
    RenderContext ctx;
    Stopwatch stopwatch;
    FrameTimes frame_times;
    if (app_->StartRender(&ctx)) {
      app_->renderer()->DrawScenario(projection_,
                                     default_room_,
                                     current_theme_,
                                     target_manager_.GetTargets(),
                                     look_at_.transform,
                                     &ctx,
                                     stopwatch,
                                     &frame_times);
      app_->FinishRender(&ctx);
    }
  }

 private:
  void UpdateCurrentTheme(const std::string& theme_name) {
    current_theme_name_ = theme_name;
    current_theme_ = app_->settings_manager()->GetTheme(current_theme_name_);

    crosshair_color_.stored_color = current_theme_.mutable_crosshair()->mutable_color();
    crosshair_outline_color_.stored_color =
        current_theme_.mutable_crosshair()->mutable_outline_color();

    target_color_.stored_color = current_theme_.mutable_target_color();
    ghost_target_color_.stored_color = current_theme_.mutable_ghost_target_color();
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
};
}  // namespace

std::unique_ptr<UiScreen> CreateThemeEditorScreen(Application* app) {
  return std::make_unique<ThemeEditorScreen>(app);
}

}  // namespace aim
