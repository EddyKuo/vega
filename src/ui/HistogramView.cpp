#define NOMINMAX
#include "ui/HistogramView.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace vega {

// ─── sRGB gamma transfer function ──────────────────────────────────────────
static float linearToSRGB(float x)
{
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1.0f)
        return 1.0f;
    return (x <= 0.0031308f)
        ? x * 12.92f
        : 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
}

// ─── compute() — histogram from RGBA8 buffer ──────────────────────────────
void HistogramView::compute(const uint8_t* rgba_data, uint32_t width, uint32_t height)
{
    hist_r_.fill(0);
    hist_g_.fill(0);
    hist_b_.fill(0);
    hist_luma_.fill(0);
    clip_shadow_ = 0;
    clip_highlight_ = 0;
    total_pixels_ = width * height;

    const uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        uint8_t r = rgba_data[i * 4 + 0];
        uint8_t g = rgba_data[i * 4 + 1];
        uint8_t b = rgba_data[i * 4 + 2];

        hist_r_[r]++;
        hist_g_[g]++;
        hist_b_[b]++;

        // BT.601 luma
        uint8_t luma = static_cast<uint8_t>(
            std::clamp(0.299f * r + 0.587f * g + 0.114f * b, 0.0f, 255.0f));
        hist_luma_[luma]++;

        // Clipping detection: any channel at absolute min or max
        if (r == 0 || g == 0 || b == 0)
            clip_shadow_++;
        if (r == 255 || g == 255 || b == 255)
            clip_highlight_++;
    }

    // Find maximum bin count across all channels (for normalization)
    max_count_ = 1;
    for (int i = 0; i < NUM_BINS; ++i)
    {
        max_count_ = std::max(max_count_, hist_r_[i]);
        max_count_ = std::max(max_count_, hist_g_[i]);
        max_count_ = std::max(max_count_, hist_b_[i]);
        max_count_ = std::max(max_count_, hist_luma_[i]);
    }
}

// ─── computeFromFloat() — histogram from linear float RGB buffer ──────────
void HistogramView::computeFromFloat(const float* rgb_data, uint32_t width, uint32_t height)
{
    hist_r_.fill(0);
    hist_g_.fill(0);
    hist_b_.fill(0);
    hist_luma_.fill(0);
    clip_shadow_ = 0;
    clip_highlight_ = 0;
    total_pixels_ = width * height;

    const uint32_t pixel_count = width * height;
    for (uint32_t i = 0; i < pixel_count; ++i)
    {
        float r_lin = rgb_data[i * 3 + 0];
        float g_lin = rgb_data[i * 3 + 1];
        float b_lin = rgb_data[i * 3 + 2];

        // Apply sRGB gamma then quantize to [0, 255]
        uint8_t r = static_cast<uint8_t>(std::clamp(linearToSRGB(r_lin) * 255.0f + 0.5f, 0.0f, 255.0f));
        uint8_t g = static_cast<uint8_t>(std::clamp(linearToSRGB(g_lin) * 255.0f + 0.5f, 0.0f, 255.0f));
        uint8_t b = static_cast<uint8_t>(std::clamp(linearToSRGB(b_lin) * 255.0f + 0.5f, 0.0f, 255.0f));

        hist_r_[r]++;
        hist_g_[g]++;
        hist_b_[b]++;

        // BT.601 luma on the gamma-corrected values
        uint8_t luma = static_cast<uint8_t>(
            std::clamp(0.299f * r + 0.587f * g + 0.114f * b, 0.0f, 255.0f));
        hist_luma_[luma]++;

        // Clipping detection
        if (r == 0 || g == 0 || b == 0)
            clip_shadow_++;
        if (r == 255 || g == 255 || b == 255)
            clip_highlight_++;
    }

    max_count_ = 1;
    for (int i = 0; i < NUM_BINS; ++i)
    {
        max_count_ = std::max(max_count_, hist_r_[i]);
        max_count_ = std::max(max_count_, hist_g_[i]);
        max_count_ = std::max(max_count_, hist_b_[i]);
        max_count_ = std::max(max_count_, hist_luma_[i]);
    }
}

// ─── Helper: draw a single channel's histogram as a filled polygon ────────
static void drawChannelFilled(ImDrawList* draw_list,
                              const std::array<uint32_t, 256>& hist,
                              float log_max,
                              ImVec2 origin, float w, float h,
                              ImU32 fill_color, ImU32 line_color)
{
    const float bin_width = w / 256.0f;

    // Draw filled bars as thin vertical rectangles (AddConvexPolyFilled cannot
    // handle the concave histogram shape, so we rasterize bin-by-bin).
    for (int i = 0; i < 256; ++i)
    {
        float x0 = origin.x + static_cast<float>(i) * bin_width;
        float x1 = origin.x + static_cast<float>(i + 1) * bin_width;
        float normalized = (hist[i] > 0)
            ? std::log(1.0f + static_cast<float>(hist[i])) / log_max
            : 0.0f;
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        float bar_top = origin.y + h - normalized * h;
        float bar_bottom = origin.y + h;

        if (normalized > 0.0f)
        {
            draw_list->AddRectFilled(ImVec2(x0, bar_top), ImVec2(x1, bar_bottom), fill_color);
        }
    }

    // Draw the outline as a polyline on top for a professional look
    std::vector<ImVec2> line_points;
    line_points.reserve(258);
    line_points.push_back(ImVec2(origin.x, origin.y + h));

    for (int i = 0; i < 256; ++i)
    {
        float x = origin.x + (static_cast<float>(i) + 0.5f) * bin_width;
        float normalized = (hist[i] > 0)
            ? std::log(1.0f + static_cast<float>(hist[i])) / log_max
            : 0.0f;
        normalized = std::clamp(normalized, 0.0f, 1.0f);
        float y = origin.y + h - normalized * h;
        line_points.push_back(ImVec2(x, y));
    }

    line_points.push_back(ImVec2(origin.x + w, origin.y + h));

    if (line_points.size() >= 2)
    {
        draw_list->AddPolyline(line_points.data(),
                               static_cast<int>(line_points.size()),
                               line_color, ImDrawFlags_None, 1.0f);
    }
}

// ─── render() — draw the histogram in the current ImGui window ────────────
void HistogramView::render()
{
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 10.0f || avail.y < 10.0f)
        return;

    const float padding = 4.0f;
    const float hist_w = avail.x - padding * 2.0f;
    const float hist_h = avail.y - padding * 2.0f;

    ImVec2 cursor_screen = ImGui::GetCursorScreenPos();
    ImVec2 origin(cursor_screen.x + padding, cursor_screen.y + padding);

    ImDrawList* draw_list = ImGui::GetWindowDrawList();

    // Reserve the space in the ImGui layout
    ImGui::Dummy(avail);

    // Background
    draw_list->AddRectFilled(
        ImVec2(cursor_screen.x, cursor_screen.y),
        ImVec2(cursor_screen.x + avail.x, cursor_screen.y + avail.y),
        IM_COL32(20, 20, 20, 230),
        3.0f);

    // Border
    draw_list->AddRect(
        ImVec2(cursor_screen.x, cursor_screen.y),
        ImVec2(cursor_screen.x + avail.x, cursor_screen.y + avail.y),
        IM_COL32(60, 60, 60, 200),
        3.0f);

    // Logarithmic scale normalization factor
    float log_max = std::log(1.0f + static_cast<float>(max_count_));
    if (log_max < 1e-6f)
        log_max = 1.0f;

    // Clip to histogram area
    draw_list->PushClipRect(
        ImVec2(cursor_screen.x, cursor_screen.y),
        ImVec2(cursor_screen.x + avail.x, cursor_screen.y + avail.y),
        true);

    // Draw channels back-to-front: Luma first (background), then B, G, R on top
    if (show_luma)
    {
        drawChannelFilled(draw_list, hist_luma_, log_max, origin, hist_w, hist_h,
                          IM_COL32(200, 200, 200, 50),   // fill: semi-transparent white
                          IM_COL32(200, 200, 200, 140));  // line: brighter white
    }
    if (show_b)
    {
        drawChannelFilled(draw_list, hist_b_, log_max, origin, hist_w, hist_h,
                          IM_COL32(50, 50, 200, 70),     // fill
                          IM_COL32(80, 80, 220, 200));    // line
    }
    if (show_g)
    {
        drawChannelFilled(draw_list, hist_g_, log_max, origin, hist_w, hist_h,
                          IM_COL32(50, 200, 50, 70),
                          IM_COL32(80, 220, 80, 200));
    }
    if (show_r)
    {
        drawChannelFilled(draw_list, hist_r_, log_max, origin, hist_w, hist_h,
                          IM_COL32(200, 50, 50, 70),
                          IM_COL32(220, 80, 80, 200));
    }

    // ── Clipping indicators ──
    if (show_clipping && total_pixels_ > 0)
    {
        const float tri_size = 8.0f;

        // Shadow clipping (left side)
        if (clip_shadow_ > 0)
        {
            float pct = static_cast<float>(clip_shadow_) / static_cast<float>(total_pixels_) * 100.0f;

            // Red triangle at bottom-left
            ImVec2 p1(origin.x, origin.y + hist_h);
            ImVec2 p2(origin.x + tri_size, origin.y + hist_h);
            ImVec2 p3(origin.x, origin.y + hist_h - tri_size);
            draw_list->AddTriangleFilled(p1, p2, p3, IM_COL32(0, 120, 255, 220));

            // Percentage text
            char text[32];
            snprintf(text, sizeof(text), "%.1f%%", pct);
            draw_list->AddText(
                ImVec2(origin.x + 2.0f, origin.y + hist_h - tri_size - ImGui::GetTextLineHeight() - 2.0f),
                IM_COL32(0, 150, 255, 220), text);
        }

        // Highlight clipping (right side)
        if (clip_highlight_ > 0)
        {
            float pct = static_cast<float>(clip_highlight_) / static_cast<float>(total_pixels_) * 100.0f;

            // Red triangle at bottom-right
            ImVec2 p1(origin.x + hist_w, origin.y + hist_h);
            ImVec2 p2(origin.x + hist_w - tri_size, origin.y + hist_h);
            ImVec2 p3(origin.x + hist_w, origin.y + hist_h - tri_size);
            draw_list->AddTriangleFilled(p1, p2, p3, IM_COL32(255, 50, 50, 220));

            // Percentage text (right-aligned)
            char text[32];
            snprintf(text, sizeof(text), "%.1f%%", pct);
            ImVec2 text_size = ImGui::CalcTextSize(text);
            draw_list->AddText(
                ImVec2(origin.x + hist_w - text_size.x - 2.0f,
                       origin.y + hist_h - tri_size - ImGui::GetTextLineHeight() - 2.0f),
                IM_COL32(255, 80, 80, 220), text);
        }
    }

    draw_list->PopClipRect();
}

} // namespace vega
