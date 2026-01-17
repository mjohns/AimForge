#include "bundle_ui.h"

#include "aim/common/files.h"
#include "aim/common/imgui_ext.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/proto/bundle.pb.h"
#include "imgui.h"

namespace aim {
namespace {

class AddBundleDialog {
 public:
  explicit AddBundleDialog(const std::string& id) : id_(id) {}

  void NotifyOpen(std::optional<std::string> bundle_to_copy) {
    open_ = true;
    bundle_to_copy_ = bundle_to_copy;
  }

  std::optional<std::string> Draw(Application& app) {
    ImGui::IdGuard cid("AddBundleDialogContent");
    std::optional<std::string> added_name;
    if (is_open_) {
      ImGui::SetNextWindowPos(
          ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
      if (ImGui::BeginPopupModal(id_.c_str(),
                                 &is_open_,
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove |
                                     ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar)) {
        ImGui::AlignTextToFramePadding();
        ImGui::Text("Bundle name");
        ImGui::SameLine();
        ImGui::InputText("##BundleNameInput", &bundle_name_);

        bool name_taken = app.bundle_manager().GetBundleInfo(bundle_name_).has_value();
        bool is_valid = IsValidBundleName(bundle_name_) && !name_taken;

        if (!is_valid) {
          ImGui::BeginDisabled();
        }

        if (!bundle_name_.empty() && !is_valid) {
          if (name_taken) {
            ImGui::TextFmt("Bundle name \"{}\" already exists", bundle_name_);
          } else {
            ImGui::TextFmt(
                "Bundle name \"{}\" is invalid. Can only contains letters, numbers, and _",
                bundle_name_);
          }
        }

        if (ImGui::Button(bundle_to_copy_ ? "Copy" : "Add")) {
          // TODO: Do copy
          BundleInfo info;
          info.set_bundle_name(bundle_name_);
          app.bundle_manager().UpdateBundleInfo(info);

          added_name = bundle_name_;
          ImGui::CloseCurrentPopup();
          is_open_ = false;
        }
        if (!is_valid) {
          ImGui::EndDisabled();
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
      bundle_name_ = "";
    }
    return added_name;
  }

 private:
  bool open_ = false;
  bool is_open_ = false;

  std::string bundle_name_;
  std::optional<std::string> bundle_to_copy_;
  std::string id_;
};

class BundleUiComponentImpl : public BundleUiComponent {
 public:
  explicit BundleUiComponentImpl(UiScreen& screen) : app_(screen.app()), screen_(screen) {}

  void Show() override {
    ImGui::IdGuard cid("BundleUiComponent");

    delete_confirmation_dialog_.Draw("Delete", [&](const std::string& bundle_name) {
      screen_.app().bundle_manager().DeleteBundle(bundle_name);
      selected_bundle_name_ = "";
    });

    auto added_name = add_dialog_.Draw(app_);
    if (added_name) {
      selected_bundle_name_ = *added_name;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_Resizable;
    if (ImGui::BeginTable("BundleColumns", 2, flags)) {
      ImGui::TableNextColumn();

      if (ImGui::Button(std::format("{} Add", kIconAdd))) {
        add_dialog_.NotifyOpen({});
      }

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
      OpenFolderInExplorer(app_.file_system()->GetUserDataPath("bundles"));
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
      add_dialog_.NotifyOpen(selected_bundle_name_);
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
  AddBundleDialog add_dialog_{"AddBundleDialog"};
};

}  // namespace

std::unique_ptr<BundleUiComponent> CreateBundleUiComponent(UiScreen* screen) {
  return std::make_unique<BundleUiComponentImpl>(*screen);
}

}  // namespace aim
