#include "perf_ui.h"

#include <algorithm>
#include <cassert>
#include <utility>

#include "aim/common/imgui_ext.h"
#include "aim/common/util.h"
#include "aim/core/perf.h"
#include "imgui.h"

namespace aim {
namespace {

void DumpFrameTimeline(const FrameTimes& t) {
  std::vector<std::pair<i64, std::string>> time_points;
  auto push_time_span = [&](const TimeSpan& span, const std::string& label) {
    time_points.push_back({span.start, label + "_start"});
    time_points.push_back({span.end, label + "_end"});
  };

#define AIM_PUSH_TIME_SPAN(name) push_time_span(t.name, #name)
#define AIM_PUSH_TIME(name) time_points.push_back({t.name, #name})

  AIM_PUSH_TIME(events_start);
  AIM_PUSH_TIME(update_start);
  AIM_PUSH_TIME(render.start);
  AIM_PUSH_TIME_SPAN(start_render);
  AIM_PUSH_TIME_SPAN(build_draw_data);
  AIM_PUSH_TIME_SPAN(pack_instance_data);
  AIM_PUSH_TIME_SPAN(upload_instance_data);
  AIM_PUSH_TIME_SPAN(upload_instance_data_copy_pass);
  AIM_PUSH_TIME_SPAN(upload_instance_data_memcpy);
  AIM_PUSH_TIME_SPAN(render_draw_data);
  AIM_PUSH_TIME_SPAN(finish_render);

  AIM_PUSH_TIME(begin_render_pass);
  AIM_PUSH_TIME(end_render_pass);
  AIM_PUSH_TIME(imgui_begin_render_pass);
  AIM_PUSH_TIME(imgui_end_render_pass);
  AIM_PUSH_TIME(imgui_render_draw_data);
  AIM_PUSH_TIME(imgui_prepare_draw_data);
  AIM_PUSH_TIME(finish_render_submit_command_buffer);
  AIM_PUSH_TIME(draw_crosshair);
  AIM_PUSH_TIME(acquire_swapchain);
  AIM_PUSH_TIME(submit_swapchain_command_buffer);

  // AIM_PUSH_TIME(start);
  // AIM_PUSH_TIME(end);

  std::erase_if(time_points, [](const auto& p) { return p.first <= 0; });
  std::stable_sort(time_points.begin(), time_points.end());

  if (time_points.empty()) {
    return;
  }

  i64 last_time = time_points[0].first;
  std::string last_label = time_points[0].second;
  for (int i = 1; i < time_points.size(); ++i) {
    i64 time = time_points[i].first;
    const std::string& label = time_points[i].second;

    i64 duration_micros = time - last_time;

    ImGui::TextFmt("{:.2f}ms: {} -> {}", duration_micros / 1000.0f, last_label, label);
    last_time = time;
    last_label = label;
  }
}

}  // namespace

void DrawPerformanceStats(const RunPerformanceStats& stats) {
  auto& worst_times = stats.worst_times;
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Worst frame");
  ImGui::SameLine();
  ImGui::InfoMarker(std::format("At time {:.1f}s. Frame {:L}",
                                stats.worst_times_micros / 1000000.0f,
                                worst_times.frame_number));

  float total_ms = (worst_times.end - worst_times.start) / 1000.0;
  ImGui::Indent();
  ImGui::AlignTextToFramePadding();
  ImGui::Text("Projected fps");
  ImGui::SameLine();
  ImGui::Button(MaybeIntToString(1000 / total_ms));
  ImGui::HelpTooltip("Projected fps if all frames were this bad.");
  ImGui::TextFmt("Total time: {:.2f}ms", total_ms);
  ImGui::TextFmt("Process events: {:.2f}ms",
                 (worst_times.events_end - worst_times.events_start) / 1000.0);
  ImGui::TextFmt("Event count: {} (mouse={}, max_seen={})",
                 worst_times.events_count,
                 worst_times.mouse_events_count,
                 stats.top_events_count);
  ImGui::TextFmt("Update time: {:.2f}ms",
                 (worst_times.update_end - worst_times.update_start) / 1000.0);
  if (worst_times.render.start > 0) {
    ImGui::TextFmt("Render time: {:.2f}ms", worst_times.render.GetSeconds() * 1000.0);
    ImGui::Indent();
    ImGui::TextFmt("Build draw data: {:.2f}ms", worst_times.build_draw_data.GetSeconds() * 1000.0);
    ImGui::TextFmt("Pack instance data: {:.2f}ms",
                   worst_times.pack_instance_data.GetSeconds() * 1000.0);
    ImGui::TextFmt("Upload instance data: {:.2f}ms",
                   worst_times.upload_instance_data.GetSeconds() * 1000.0);
    ImGui::TextFmt("Upload instance data (copy pass): {:.2f}ms",
                   worst_times.upload_instance_data_copy_pass.GetSeconds() * 1000.0);
    ImGui::TextFmt("Upload instance data (memcpy): {:.2f}ms",
                   worst_times.upload_instance_data_memcpy.GetSeconds() * 1000.0);
    ImGui::TextFmt("Render draw data: {:.2f}ms",
                   worst_times.render_draw_data.GetSeconds() * 1000.0);
    ImGui::TextFmt("Start render: {:.2f}ms", worst_times.start_render.GetSeconds() * 1000.0);
    ImGui::TextFmt("Finish render: {:.2f}ms", worst_times.finish_render.GetSeconds() * 1000.0);
    ImGui::Unindent();
  }
  ImGui::Unindent();

  if (ImGui::TreeNode("Frame timeline")) {
    DumpFrameTimeline(worst_times);
    ImGui::TreePop();
  }

  ImGui::Spacing();
  ImGui::Separator();
  ImGui::Spacing();

  ImGui::Text("Total Times (ms)");
  ImGui::Indent();
  DumpHistogram(stats.total_time_histogram);
  ImGui::Unindent();

  if (worst_times.render.start > 0) {
    ImGui::SpacedSeparator();

    ImGui::Text("Render Times (ms)");
    ImGui::Indent();
    DumpHistogram(stats.render_time_histogram);
    ImGui::Unindent();
  }

  ImGui::SpacedSeparator();
  ImGui::Text("Update Times (ms)");
  ImGui::Indent();
  DumpHistogram(stats.update_time_histogram);
  ImGui::Unindent();

  ImGui::SpacedSeparator();
  ImGui::Text("Event Times (ms)");
  ImGui::Indent();
  DumpHistogram(stats.events_time_histogram);
  ImGui::Unindent();
}

}  // namespace aim
