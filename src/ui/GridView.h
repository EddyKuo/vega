#pragma once
#include "catalog/Database.h"
#include "catalog/ThumbnailCache.h"
#include <imgui.h>
#include <vector>
#include <cstdint>

namespace vega {

class GridView {
public:
    void render(Database& db, ThumbnailCache& cache);

    int64_t selectedPhotoId() const;
    void setFilter(const Database::FilterCriteria& filter);
    void refresh();

private:
    float thumb_size_ = 200.0f;
    int64_t selected_id_ = -1;
    Database::FilterCriteria filter_;
    std::vector<PhotoRecord> visible_photos_;
    bool needs_refresh_ = true;

    // Render individual photo cell: thumbnail, rating stars, color dot, flag
    void renderPhotoCell(const PhotoRecord& photo, ThumbnailCache& cache,
                         Database& db, float cell_size);

    // Render rating stars widget. Returns new rating if changed, -1 otherwise.
    int renderRatingStars(int current_rating, int64_t photo_id);

    // Render color label selector. Returns new label if changed, -1 otherwise.
    int renderColorLabel(int current_label);

    // Render flag toggle. Returns new flag if changed, -1 otherwise.
    int renderFlagIcon(int current_flag);
};

} // namespace vega
