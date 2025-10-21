#include "copy_playlist_dialog.h"

#include <algorithm>

#include "absl/strings/strip.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/search.h"
#include "aim/ui/scenario_editor_screen.h"
#include "google/protobuf/util/message_differencer.h"
#include "imgui.h"

namespace aim {
namespace {

struct CopyPlaylistOptions {
  std::string remove_prefix;
  std::string add_prefix;
  bool deep_copy = false;
  bool as_references = false;
};

bool CopyPlaylist(Playlist source,
                  ResourceName new_playlist_name,
                  const CopyPlaylistOptions& opts,
                  Application& app) {
  // Copy all scenarios if necessary.
  auto taken_names =
      app.playlist_manager().GetAllRelativeNamesInBundle(new_playlist_name.bundle_name());
  *new_playlist_name.mutable_relative_name() =
      MakeUniqueName(new_playlist_name.relative_name(), taken_names);

  PlaylistDef dest = source.def();
  dest.clear_scenario_levels_def();
  if (opts.deep_copy) {
    std::unordered_map<std::string, ResourceName> new_name_map;
    std::unordered_map<std::string, ScenarioDef> new_scenario_map;
    dest.clear_items();
    for (const auto& source_item : source.items()) {
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
  } else {
    // Not a deep copy. If it was a levels scenario copy the items over as is.
    if (source.def().has_scenario_levels_def()) {
      for (const auto& source_item : source.items()) {
        *dest.add_items() = source_item;
      }
    }
  }
  app.playlist_manager().SavePlaylist(new_playlist_name, dest);
  app.playlist_manager().SetCurrentPlaylist(new_playlist_name.full_name());
  app.history_manager().UpdateRecentView(ObjectType::PLAYLIST, new_playlist_name.full_name());
  return true;
}

}  // namespace

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

}  // namespace aim
