#define NOMINMAX
#include "ui/Toolbar.h"
#include <imgui.h>

namespace vega {

bool Toolbar::iconButton(const char* label, const char* tooltip, bool enabled)
{
    if (!enabled)
    {
        ImGui::BeginDisabled();
    }

    // Uniform button size for a clean toolbar look
    ImVec2 btn_size(ImGui::GetFrameHeight() * 2.0f, ImGui::GetFrameHeight());
    bool pressed = ImGui::Button(label, btn_size);

    if (!enabled)
    {
        ImGui::EndDisabled();
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) && tooltip)
    {
        ImGui::SetTooltip("%s", tooltip);
    }

    return pressed;
}

void Toolbar::render(bool has_image, bool can_undo, bool can_redo,
                     bool is_develop_mode)
{
    ImGuiViewport* vp = ImGui::GetMainViewport();
    float toolbar_h = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;

    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, toolbar_h));

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoDocking |
        ImGuiWindowFlags_NoSavedSettings;

    ImGui::Begin("##Toolbar", nullptr, flags);

    // ── File section ──
    if (iconButton("Open", "Open RAW file (Ctrl+O)"))
    {
        if (callbacks_.on_open)
            callbacks_.on_open();
    }

    ImGui::SameLine();
    if (iconButton("Save", "Save recipe (Ctrl+S)", has_image))
    {
        if (callbacks_.on_save_recipe)
            callbacks_.on_save_recipe();
    }

    ImGui::SameLine();
    if (iconButton("Export", "Export image (Ctrl+Shift+E)", has_image))
    {
        if (callbacks_.on_export)
            callbacks_.on_export();
    }

    // ── Separator ──
    ImGui::SameLine();
    ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
    ImGui::SameLine();

    // ── Edit section ──
    if (iconButton("Undo", "Undo (Ctrl+Z)", has_image && can_undo))
    {
        if (callbacks_.on_undo)
            callbacks_.on_undo();
    }

    ImGui::SameLine();
    if (iconButton("Redo", "Redo (Ctrl+Y)", has_image && can_redo))
    {
        if (callbacks_.on_redo)
            callbacks_.on_redo();
    }

    // ── Separator ──
    ImGui::SameLine();
    ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
    ImGui::SameLine();

    // ── Zoom section ──
    if (iconButton("Fit", "Fit to window (F)", has_image))
    {
        if (callbacks_.on_zoom_fit)
            callbacks_.on_zoom_fit();
    }

    ImGui::SameLine();
    if (iconButton("100%", "Zoom 100% (1)", has_image))
    {
        if (callbacks_.on_zoom_100)
            callbacks_.on_zoom_100();
    }

    ImGui::SameLine();
    if (iconButton("200%", "Zoom 200% (2)", has_image))
    {
        if (callbacks_.on_zoom_200)
            callbacks_.on_zoom_200();
    }

    // ── Separator ──
    ImGui::SameLine();
    ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
    ImGui::SameLine();

    // ── View toggles ──
    if (iconButton("B/A", "Before / After toggle", has_image))
    {
        if (callbacks_.on_toggle_before_after)
            callbacks_.on_toggle_before_after();
    }

    // ── Separator ──
    ImGui::SameLine();
    ImGui::SameLine(); ImGui::Text("|"); ImGui::SameLine();
    ImGui::SameLine();

    // ── Mode switch (right-aligned cluster) ──
    {
        // Highlight the active mode
        if (is_develop_mode)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        }

        if (iconButton("Dev", "Develop mode"))
        {
            if (callbacks_.on_mode_develop)
                callbacks_.on_mode_develop();
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        if (!is_develop_mode)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_Button));
        }

        if (iconButton("Grid", "Grid / Library mode"))
        {
            if (callbacks_.on_mode_grid)
                callbacks_.on_mode_grid();
        }
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::PopStyleVar(2);
}

} // namespace vega
