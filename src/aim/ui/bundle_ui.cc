#include "bundle_ui.h"

#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/proto/bundle.pb.h"
#include "imgui.h"

namespace aim {
namespace {

class BundleUiComponentImpl : public BundleUiComponent {
 public:
  explicit BundleUiComponentImpl(UiScreen& screen) : app_(screen.app()), screen_(screen) {}

  void Show() override {
    ImGui::IdGuard cid("BundleUiComponent");

    delete_confirmation_dialog_.Draw("Delete", [=](const std::string& bundle_name) {
      screen_.app().bundle_manager().DeleteBundle(bundle_name);
    });

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("BundleColumns", 2, flags)) {
      ImGui::TableNextColumn();
      DrawBundlesList();

      ImGui::TableNextColumn();
      DrawRightPanel();

      ImGui::EndTable();
    }
  }

 private:
  void DrawRightPanel() {
    if (!selected_bundle_name_.empty()) {
      DrawSelectedBundle();
    }

    if (ImGui::Button("Open bundles folder")) {
      OpenFileInExplorer(app_.file_system()->GetUserDataPath("bundles/bundles.json"));
    }
  }

  void DrawSelectedBundle() {
    auto maybe_bundle_info = app_.bundle_manager().GetBundleInfo(selected_bundle_name_);
    if (!maybe_bundle_info) {
      ImGui::Text("Could not find bundle: %s", selected_bundle_name_);
      return;
    }
    BundleInfo info = *maybe_bundle_info;

    ImGui::Text("Bundle name: %s", selected_bundle_name_.c_str());

    bool need_update = false;

    bool readonly = info.readonly();
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Readonly");
    ImGui::SameLine();
    if (ImGui::Checkbox("##ReadonlyInput", &readonly)) {
      info.set_readonly(readonly);
      need_update = true;
    }

    if (ImGui::Button(std::format("{} Copy", kIconContentCopy))) {
    }

    if (ImGui::Button(std::format("{} Delete", kIconDelete))) {
      delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", selected_bundle_name_),
                                             selected_bundle_name_);
    }

    if (need_update) {
      app_.bundle_manager().UpdateBundleInfo(info);
    }
  }

  void DrawBundlesList() {
    ImGui::LoopId loop_id;
    auto bundle_infos = app_.bundle_manager().GetBundleInfos();
    for (const BundleInfo& bundle : bundle_infos) {
      auto id_guard = loop_id.Get();
      if (ImGui::Selectable(bundle.bundle_name().c_str(),
                            bundle.bundle_name() == selected_bundle_name_)) {
        selected_bundle_name_ = bundle.bundle_name();
      }
    }
  }
  UiScreen& screen_;
  Application& app_;

  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::string selected_bundle_name_;
};

}  // namespace

std::unique_ptr<BundleUiComponent> CreateBundleUiComponent(UiScreen* screen) {
  return std::make_unique<BundleUiComponentImpl>(*screen);
}

}  // namespace aim
