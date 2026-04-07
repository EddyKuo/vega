#define NOMINMAX
#include "ui/GridView.h"
#include "core/Logger.h"

#include <imgui.h>
#include <d3d11.h>
#include <algorithm>
#include <cmath>

namespace vega {

// ---------------------------------------------------------------------------
// Color label palette (None, Red, Yellow, Green, Blue, Purple)
// ---------------------------------------------------------------------------

static const ImVec4 kColorLabels[] = {
    ImVec4(0.3f, 0.3f, 0.3f, 1.0f),  // 0: None (gray)
    ImVec4(1.0f, 0.2f, 0.2f, 1.0f),  // 1: Red
    ImVec4(1.0f, 0.9f, 0.1f, 1.0f),  // 2: Yellow
    ImVec4(0.2f, 0.9f, 0.2f, 1.0f),  // 3: Green
    ImVec4(0.2f, 0.5f, 1.0f, 1.0f),  // 4: Blue
    ImVec4(0.7f, 0.3f, 0.9f, 1.0f),  // 5: Purple
};
static constexpr int kNumColorLabels = 6;

// ---------------------------------------------------------------------------
// render
// ---------------------------------------------------------------------------

void GridView::render(Database& db, ThumbnailCache& cache)
{
    // Refresh visible photos if needed
    if (needs_refresh_) {
        // Check if filter has any active criteria
        bool has_filter = filter_.min_rating > 0 ||
                          filter_.color_label >= 0 ||
                          filter_.flag >= 0 ||
                          !filter_.camera_model.empty() ||
                          !filter_.lens_model.empty() ||
                          !filter_.date_from.empty() ||
                          !filter_.date_to.empty() ||
                          !filter_.search_text.empty();

        if (has_filter) {
            visible_photos_ = db.filter(filter_);
        } else {
            visible_photos_ = db.getAllPhotos();
        }
        needs_refresh_ = false;
    }

    // ----- Toolbar -----
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

    // Thumbnail size slider
    ImGui::AlignTextToFramePadding();
    ImGui::Text("Size:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    ImGui::SliderFloat("##ThumbSize", &thumb_size_, 80.0f, 400.0f, "%.0f");

    // Filter: minimum rating
    ImGui::SameLine();
    ImGui::Text("  Min Rating:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    int min_rating = filter_.min_rating;
    if (ImGui::SliderInt("##MinRating", &min_rating, 0, 5)) {
        filter_.min_rating = min_rating;
        needs_refresh_ = true;
    }

    // Filter: color label
    ImGui::SameLine();
    ImGui::Text("  Color:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    const char* color_items[] = {"Any", "Red", "Yellow", "Green", "Blue", "Purple"};
    int color_idx = filter_.color_label + 1;  // -1 -> 0, 0 -> 1, etc.
    if (ImGui::Combo("##ColorFilter", &color_idx, color_items, IM_ARRAYSIZE(color_items))) {
        filter_.color_label = color_idx - 1;
        needs_refresh_ = true;
    }

    // Filter: flag
    ImGui::SameLine();
    ImGui::Text("  Flag:");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(80.0f);
    const char* flag_items[] = {"Any", "Unflagged", "Flagged", "Rejected"};
    int flag_idx = filter_.flag + 1;
    if (ImGui::Combo("##FlagFilter", &flag_idx, flag_items, IM_ARRAYSIZE(flag_items))) {
        filter_.flag = flag_idx - 1;
        needs_refresh_ = true;
    }

    // Photo count
    ImGui::SameLine();
    ImGui::Text("  %d photos", static_cast<int>(visible_photos_.size()));

    ImGui::PopStyleVar();

    ImGui::Separator();

    // ----- Grid -----
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 1.0f || avail.y < 1.0f) return;

    float cell_size = thumb_size_ + 8.0f;  // thumbnail + padding
    float cell_height = cell_size + 40.0f;  // extra space for info below thumbnail

    int columns = std::max(1, static_cast<int>(std::floor(avail.x / cell_size)));
    int total_rows = (static_cast<int>(visible_photos_.size()) + columns - 1) / columns;

    // Use a child window with scrolling for virtualization
    ImGui::BeginChild("##PhotoGrid", avail, false,
                      ImGuiWindowFlags_AlwaysVerticalScrollbar);

    float scroll_y = ImGui::GetScrollY();
    float visible_height = ImGui::GetWindowHeight();

    // Calculate which rows are visible (+ 2 buffer rows above and below)
    int first_visible_row = std::max(0, static_cast<int>(std::floor(scroll_y / cell_height)) - 2);
    int last_visible_row = std::min(total_rows - 1,
        static_cast<int>(std::ceil((scroll_y + visible_height) / cell_height)) + 2);

    // Set cursor position for the first visible row (creates virtual space above)
    if (first_visible_row > 0) {
        ImGui::SetCursorPosY(first_visible_row * cell_height);
        ImGui::Dummy(ImVec2(0, 0));
    }

    // Render visible rows
    for (int row = first_visible_row; row <= last_visible_row; ++row) {
        for (int col = 0; col < columns; ++col) {
            int idx = row * columns + col;
            if (idx >= static_cast<int>(visible_photos_.size())) break;

            if (col > 0) ImGui::SameLine();

            ImGui::BeginGroup();
            renderPhotoCell(visible_photos_[idx], cache, db, thumb_size_);
            ImGui::EndGroup();
        }
    }

    // Set dummy space after last visible row to maintain correct scrollbar size
    if (total_rows > 0) {
        float total_content_height = total_rows * cell_height;
        float current_y = ImGui::GetCursorPosY();
        if (current_y < total_content_height) {
            ImGui::Dummy(ImVec2(0, total_content_height - current_y));
        }
    }

    ImGui::EndChild();
}

// ---------------------------------------------------------------------------
// renderPhotoCell
// ---------------------------------------------------------------------------

void GridView::renderPhotoCell(const PhotoRecord& photo, ThumbnailCache& cache,
                                Database& db, float cell_size)
{
    ImGui::PushID(static_cast<int>(photo.id));

    bool is_selected = (photo.id == selected_id_);

    // Cell background
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    ImVec2 cell_max = ImVec2(cursor.x + cell_size, cursor.y + cell_size + 36.0f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    if (is_selected) {
        draw_list->AddRectFilled(cursor, cell_max,
                                  IM_COL32(60, 100, 180, 180), 4.0f);
    }

    // Thumbnail image
    ID3D11ShaderResourceView* srv = cache.getThumbnail(
        photo.uuid,
        std::filesystem::path(photo.file_path),
        ThumbnailCache::Level::Small);

    ImVec2 thumb_pos = ImVec2(cursor.x + 4.0f, cursor.y + 4.0f);
    float thumb_draw_size = cell_size - 8.0f;

    if (srv) {
        // Get actual texture dimensions to preserve aspect ratio
        ID3D11Resource* res = nullptr;
        srv->GetResource(&res);
        ID3D11Texture2D* tex2d = nullptr;
        float img_w = thumb_draw_size, img_h = thumb_draw_size;
        if (res && SUCCEEDED(res->QueryInterface(__uuidof(ID3D11Texture2D),
                             reinterpret_cast<void**>(&tex2d)))) {
            D3D11_TEXTURE2D_DESC desc;
            tex2d->GetDesc(&desc);
            float aspect = static_cast<float>(desc.Width) / static_cast<float>(desc.Height);
            if (aspect > 1.0f) {
                img_w = thumb_draw_size;
                img_h = thumb_draw_size / aspect;
            } else {
                img_h = thumb_draw_size;
                img_w = thumb_draw_size * aspect;
            }
            tex2d->Release();
        }
        if (res) res->Release();

        // Center the image within the cell
        float offset_x = (thumb_draw_size - img_w) * 0.5f;
        float offset_y = (thumb_draw_size - img_h) * 0.5f;
        ImGui::SetCursorScreenPos(ImVec2(thumb_pos.x + offset_x, thumb_pos.y + offset_y));
        ImGui::Image(reinterpret_cast<ImTextureID>(srv), ImVec2(img_w, img_h));

        // Ensure cursor advances by the full cell size
        ImGui::SetCursorScreenPos(thumb_pos);
        ImGui::Dummy(ImVec2(thumb_draw_size, thumb_draw_size));
    } else {
        // Placeholder rectangle
        draw_list->AddRectFilled(
            thumb_pos,
            ImVec2(thumb_pos.x + thumb_draw_size, thumb_pos.y + thumb_draw_size),
            IM_COL32(40, 40, 40, 255), 2.0f);
        draw_list->AddRect(
            thumb_pos,
            ImVec2(thumb_pos.x + thumb_draw_size, thumb_pos.y + thumb_draw_size),
            IM_COL32(80, 80, 80, 255), 2.0f);

        // Show filename as placeholder text
        ImVec2 text_size = ImGui::CalcTextSize(photo.file_name.c_str());
        float max_text_w = thumb_draw_size - 8.0f;
        ImVec2 text_pos = ImVec2(
            thumb_pos.x + (thumb_draw_size - std::min(text_size.x, max_text_w)) * 0.5f,
            thumb_pos.y + (thumb_draw_size - text_size.y) * 0.5f);

        draw_list->AddText(text_pos, IM_COL32(120, 120, 120, 255),
                           photo.file_name.c_str());

        // Advance cursor past the placeholder
        ImGui::SetCursorScreenPos(thumb_pos);
        ImGui::Dummy(ImVec2(thumb_draw_size, thumb_draw_size));
    }

    // Click to select, double-click to open
    if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(0)) {
        selected_id_ = photo.id;
    }
    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0)) {
        selected_id_ = photo.id;
        if (on_double_click_) on_double_click_(photo.id, photo.file_path);
    }

    // ----- Info below thumbnail -----
    float info_y = cursor.y + cell_size - 4.0f;

    // Rating stars
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 4.0f, info_y));
    int new_rating = renderRatingStars(photo.rating, photo.id);
    if (new_rating >= 0) {
        db.setRating(photo.id, new_rating);
        needs_refresh_ = true;
    }

    // Color label dot
    ImGui::SameLine();
    int new_label = renderColorLabel(photo.color_label);
    if (new_label >= 0) {
        db.setColorLabel(photo.id, new_label);
        needs_refresh_ = true;
    }

    // Flag icon
    ImGui::SameLine();
    int new_flag = renderFlagIcon(photo.flag);
    if (new_flag >= 0) {
        db.setFlag(photo.id, new_flag);
        needs_refresh_ = true;
    }

    // Filename (truncated)
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + 4.0f, info_y + 18.0f));
    ImGui::PushTextWrapPos(cursor.x + cell_size - 4.0f);
    ImGui::TextDisabled("%s", photo.file_name.c_str());
    ImGui::PopTextWrapPos();

    // Set cursor past the whole cell
    ImGui::SetCursorScreenPos(ImVec2(cursor.x + cell_size, cursor.y));
    ImGui::Dummy(ImVec2(0, cell_size + 36.0f));

    ImGui::PopID();
}

// ---------------------------------------------------------------------------
// renderRatingStars
// ---------------------------------------------------------------------------

int GridView::renderRatingStars(int current_rating, int64_t photo_id)
{
    int result = -1;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(1, 0));

    for (int i = 1; i <= 5; ++i) {
        if (i > 1) ImGui::SameLine();

        bool filled = (i <= current_rating);
        const char* label = filled ? "*" : ".";

        ImVec4 color = filled ? ImVec4(1.0f, 0.85f, 0.0f, 1.0f)
                              : ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

        char btn_id[32];
        snprintf(btn_id, sizeof(btn_id), "%s##star%d_%lld", label, i,
                 static_cast<long long>(photo_id));

        if (ImGui::SmallButton(btn_id)) {
            // Click on current rating to clear it, otherwise set new rating
            result = (i == current_rating) ? 0 : i;
        }

        ImGui::PopStyleColor(4);
    }

    ImGui::PopStyleVar();
    return result;
}

// ---------------------------------------------------------------------------
// renderColorLabel
// ---------------------------------------------------------------------------

int GridView::renderColorLabel(int current_label)
{
    int result = -1;

    int label_clamped = std::clamp(current_label, 0, kNumColorLabels - 1);
    ImVec4 color = kColorLabels[label_clamped];

    ImVec2 pos = ImGui::GetCursorScreenPos();
    float radius = 5.0f;
    ImVec2 center = ImVec2(pos.x + radius + 2.0f, pos.y + ImGui::GetTextLineHeight() * 0.5f);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddCircleFilled(center, radius,
                                ImGui::ColorConvertFloat4ToU32(color));

    // Invisible button for interaction
    ImGui::InvisibleButton("##colorlabel", ImVec2(radius * 2 + 4, ImGui::GetTextLineHeight()));

    if (ImGui::IsItemClicked()) {
        // Cycle through color labels
        result = (current_label + 1) % kNumColorLabels;
    }

    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Color label: %d (click to cycle)", current_label);
    }

    return result;
}

// ---------------------------------------------------------------------------
// renderFlagIcon
// ---------------------------------------------------------------------------

int GridView::renderFlagIcon(int current_flag)
{
    int result = -1;

    const char* flag_text;
    ImVec4 flag_color;

    switch (current_flag) {
        case 1:  // Flagged (pick)
            flag_text = "P";
            flag_color = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
            break;
        case 2:  // Rejected
            flag_text = "X";
            flag_color = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
            break;
        default: // Unflagged
            flag_text = "-";
            flag_color = ImVec4(0.4f, 0.4f, 0.4f, 1.0f);
            break;
    }

    ImGui::PushStyleColor(ImGuiCol_Text, flag_color);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.4f, 0.4f, 0.4f, 0.5f));

    if (ImGui::SmallButton(flag_text)) {
        // Cycle: 0 -> 1 -> 2 -> 0
        result = (current_flag + 1) % 3;
    }

    ImGui::PopStyleColor(4);

    if (ImGui::IsItemHovered()) {
        const char* tooltip;
        switch (current_flag) {
            case 1:  tooltip = "Flagged (click to reject)"; break;
            case 2:  tooltip = "Rejected (click to unflag)"; break;
            default: tooltip = "Unflagged (click to flag)"; break;
        }
        ImGui::SetTooltip("%s", tooltip);
    }

    return result;
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

int64_t GridView::selectedPhotoId() const
{
    return selected_id_;
}

void GridView::setFilter(const Database::FilterCriteria& filter)
{
    filter_ = filter;
    needs_refresh_ = true;
}

void GridView::refresh()
{
    needs_refresh_ = true;
}

} // namespace vega
