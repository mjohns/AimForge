#include "copy_playlist_dialog.h"

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/name_util.h"
#include "aim/common/search.h"
#include "aim/core/bundle_manager.h"
#include "aim/ui/scenario_editor_screen.h"
#include "imgui.h"

namespace aim {

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
      ImGui::TextFmt("Copy \"{}\" to", source_->name);
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
        std::optional<std::string> final_name = app.playlist_manager().CopyPlaylist(
            source_->name, new_name_.full_name(), &app.scenario_manager(), opts);
        if (final_name) {
          new_name_ = ResourceName::Parse(*final_name);
          did_copy = app.bundle_manager().SaveDirtyBundles();
        }
        if (did_copy) {
          app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, new_name_.full_name());
          app.playlist_manager().SetCurrentPlaylist(new_name_.full_name());
        }
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
    bundle_names_ = app.bundle_manager().GetBundleNames();
    new_name_ = ResourceName::Parse(source_->name);
    *new_name_.mutable_relative_name() += " Copy";
  }
  return did_copy;
}

}  // namespace aim
