#include "object_browser.h"

#include "absl/cleanup/cleanup.h"
#include "aim/common/imgui_ext.h"
#include "aim/common/mat_icons.h"
#include "aim/common/object_type.h"
#include "aim/common/resource_name.h"
#include "aim/common/search.h"
#include "aim/core/application.h"
#include "aim/core/bundle_manager.h"
#include "aim/core/guide_manager.h"
#include "aim/core/history_manager.h"
#include "aim/core/labels_manager.h"
#include "aim/core/local_store.h"
#include "aim/core/playlist_manager.h"
#include "aim/core/scenario_manager.h"
#include "aim/ui/search_selector.h"
#include "aim/ui/ui_app.h"
#include "imgui.h"

namespace aim {
namespace {

enum class ViewType : int {
  RECENT = 1,
  ALL = 2,
  STARRED = 3,
};

static std::string GetViewTypeKey(ObjectType type) {
  switch (type) {
    case ObjectType::SCENARIO:
      return "ScenarioViewType";
    case ObjectType::PLAYLIST:
      return "PlaylistViewType";
    case ObjectType::GUIDE:
      return "GuideViewType";
    case ObjectType::THEME:
    case ObjectType::CROSSHAIR:
      break;
  }
  assert(false && "Unsupported object for ViewType");
  return "UnknownViewType";
}

class ObjectBrowserImpl : public ObjectBrowser {
 public:
  explicit ObjectBrowserImpl(ObjectType type) : type_(type) {
    auto maybe_initial_view_type = app_.local_store().GetInt(GetViewTypeKey(type));
    if (maybe_initial_view_type) {
      view_type_ = static_cast<ViewType>(*maybe_initial_view_type);
    }
  }

  // std::function<void(const std::string& name, ObjectBrowserResult*)> draw_item,
  void Draw(Result* result) override {
    ImGui::IdGuard cid(type_name_ + "SearchList");

    auto to_delete = delete_confirmation_dialog_.Draw("Delete");
    if (to_delete) {
      if (type_ == ObjectType::GUIDE) {
        app_.guide_manager().DeleteGuide(*to_delete);
      } else if (type_ == ObjectType::PLAYLIST) {
        app_.playlist_manager().DeletePlaylist(*to_delete);
      } else if (type_ == ObjectType::SCENARIO) {
        app_.scenario_manager().DeleteScenario(*to_delete);
      }
      app_.bundle_manager().SaveDirtyBundles();
    }

    ImVec2 char_size = ImGui::CalcTextSize("A");

    auto recents = app_.history_manager().GetCachedRecentNames(type_);
    if (recents->size() > 0) {
      ImGui::IdGuard cid("QuickAccess");

      // Draw the 10 most recent items.
      ImGui::LoopId loop_id;
      int i = 0;
      for (const std::string& name : *recents) {
        i++;
        if (i >= 10) {
          break;
        }
        auto id_guard = loop_id.Get();
        DrawItem(name, result);
      }

      ImGui::SpacedSeparator();
    }

    ImGui::AlignTextToFramePadding();
    ImGui::Text("%s", icons::kFilterList);
    ImGui::SameLine();
    if (ImGui::ChipSelector("##ViewType",
                            &view_type_,
                            {
                                {ViewType::ALL, "All"},
                                {ViewType::RECENT, "Recent"},
                                {ViewType::STARRED, "Starred"},
                            })) {
      app_.local_store().PutInt(GetViewTypeKey(type_), (int)view_type_);
    }

    ImGui::SetNextItemWidth(char_size.x * 30);
    ImGui::InputTextWithHint("##SearchInput", icons::kSearch, &search_text_);
    if (search_text_.size() > 0) {
      ImGui::SameLine();
      if (ImGui::ClearButton()) {
        search_text_ = "";
      }
    }

    ImGui::BeginChild("SearchContent");
    auto child_cleanup = absl::MakeCleanup([] { ImGui::EndChild(); });

    auto search_words = GetSearchWords(search_text_);

    auto new_names = GetNames();
    if (new_names == nullptr) {
      return;
    }
    if (new_names != all_names_) {
      all_names_ = new_names;

      if (search_text_.empty()) {
        filtered_names_ = all_names_;
      } else {
        // Refilter
        filtered_names_.clear();
        filtered_names_.reserve(all_names_.size());

      for (const std::string& name : *names) {
        if (StringMatchesSearch(name, search_words)) {
          // TODO: Check if it exists?
          filtered_names_.push_back(name);
        }
      }
    }

    ImGui::LoopId loop_id;
    for (const std::string_view& name : filtered_names_) {
      auto id_guard = loop_id.Get();
      DrawItem(name, result);
    }
  }

 private:
  void DrawItem(std::string_view name, Result* result) {
    if (ImGui::Button(name)) {
      result->selected_object_name = name;
    }

    const char* menu_id = "ItemMenu";
    if (ImGui::BeginPopupContextItem(menu_id)) {
      auto resource_name = ResourceName::Parse(name);
      bool is_readonly = app_.bundle_manager().IsBundleReadonly(resource_name.bundle_name());

      if (ImGui::Selectable(std::format("{} Copy", icons::kContentCopy))) {
        result->copy_object_name = name;
      }
      if (ImGui::Selectable(std::format("{} Recents", icons::kClose))) {
        app_.history_manager().DeleteRecentView(type_, name);
      }

      if (!is_readonly) {
        ImGui::SpacedSeparator();
        if (ImGui::Selectable(std::format("{} Delete", icons::kDelete))) {
          delete_confirmation_dialog_.NotifyOpen(std::format("Delete \"{}\"?", name), name);
        }
      }
      ImGui::EndPopup();
    }
    ImGui::OpenPopupOnItemClick(menu_id, ImGuiPopupFlags_MouseButtonRight);

    ImGui::SameLine();
    if (app_.labels_manager().IsStarred(type_, name)) {
      if (ImGui::IconButton(icons::kStar)) {
        app_.labels_manager().UnstarItem(type_, name);
      }
    } else {
      if (ImGui::IconButton(icons::kStarOutline)) {
        app_.labels_manager().StarItem(type_, name);
      }
    }
  }

  std::shared_ptr<std::vector<std::string>> GetNames() {
    if (view_type_ == ViewType::RECENT) {
      return app_.history_manager().GetCachedRecentNames(type_);
    }
    if (view_type_ == ViewType::STARRED) {
      auto items = app_.labels_manager().ListStarredItems(type_);
      return std::shared_ptr<std::vector<std::string>>(items, &items->items);
    }
    switch (type_) {
      case ObjectType::SCENARIO:
        return app_.scenario_manager().scenario_names();
      case ObjectType::PLAYLIST:
        return app_.playlist_manager().playlist_names();
      case ObjectType::GUIDE:
        return app_.guide_manager().guide_names();
      case ObjectType::THEME:
      case ObjectType::CROSSHAIR:
        break;
    }
    assert(false && "Unsupported object type");
    return nullptr;
  }

  Application& app_ = GetUiApp();
  std::string search_text_;
  const ObjectType type_;
  const std::string type_name_ = ObjectTypeToString(type_);
  ViewType view_type_ = ViewType::ALL;
  ImGui::ConfirmationDialog<std::string> delete_confirmation_dialog_{"DeleteConfirmationDialog"};
  std::shared_ptr<std::vector<std::string>> all_names_;
  std::shared_ptr<std::vector<std::string>> filtered_names_;
};

}  // namespace

std::unique_ptr<ObjectBrowser> CreateObjectBrowser(ObjectType type) {
  return std::make_unique<ObjectBrowserImpl>(type);
}

}  // namespace aim
