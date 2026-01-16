#include "bundle_ui.h"

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
      //      screen_.app().playlist_manager().DeleteBundle(playlist.name);
    });

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

 private:
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
