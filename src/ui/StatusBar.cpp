#define NOMINMAX
#include "ui/StatusBar.h"
#include <imgui.h>
#include <cstdio>

namespace vega {

void StatusBar::render()
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float bar_h = ImGui::GetFrameHeight();

    ImGui::SetNextWindowPos(ImVec2(vp->WorkPos.x,
                                    vp->WorkPos.y + vp->WorkSize.y - bar_h));
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, bar_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##StatusBar", nullptr, flags);

    float total_w = ImGui::GetWindowWidth();

    // ── Left section: file info ──
    if (!filename.empty())
    {
        ImGui::Text("%s", filename.c_str());

        if (!camera_info.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("%s", camera_info.c_str());
        }

        if (!resolution.empty())
        {
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("%s", resolution.c_str());
        }
    }
    else
    {
        ImGui::TextDisabled("Ready -- Ctrl+O to open a RAW file");
    }

    // ── Center section: pipeline timing ──
    if (pipeline_ms > 0.0)
    {
        char timing[64];
        snprintf(timing, sizeof(timing), "Pipeline: %.1f ms", pipeline_ms);
        float text_w = ImGui::CalcTextSize(timing).x;
        float center_x = total_w * 0.5f - text_w * 0.5f;

        ImGui::SameLine(center_x);
        ImGui::TextDisabled("%s", timing);
    }

    // ── Right section: zoom, undo, GPU/CPU ──
    {
        char right[128];
        snprintf(right, sizeof(right),
                 "Zoom: %.0f%%  |  Undo: %d/%d  |  %s",
                 zoom_pct, undo_current, undo_total,
                 use_gpu ? "GPU" : "CPU");

        float text_w = ImGui::CalcTextSize(right).x;
        float right_x = total_w - text_w - 12.0f;

        ImGui::SameLine(right_x);
        ImGui::Text("%s", right);
    }

    ImGui::End();
    ImGui::PopStyleVar();
}

} // namespace vega
