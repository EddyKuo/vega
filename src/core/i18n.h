#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <filesystem>

struct sqlite3;

namespace vega {

enum class Lang { EN, ZH_TW };

// All translatable string keys (unchanged, keeps compile-time safety)
namespace S {
    // Menu
    inline constexpr const char* MENU_FILE          = "menu.file";
    inline constexpr const char* MENU_OPEN          = "menu.open";
    inline constexpr const char* MENU_SAVE_RECIPE   = "menu.save_recipe";
    inline constexpr const char* MENU_EXPORT        = "menu.export";
    inline constexpr const char* MENU_EXIT          = "menu.exit";
    inline constexpr const char* MENU_EDIT          = "menu.edit";
    inline constexpr const char* MENU_UNDO          = "menu.undo";
    inline constexpr const char* MENU_REDO          = "menu.redo";
    inline constexpr const char* MENU_RESET_ALL     = "menu.reset_all";
    inline constexpr const char* MENU_VIEW          = "menu.view";
    inline constexpr const char* MENU_FIT           = "menu.fit";
    inline constexpr const char* MENU_BEFORE_AFTER  = "menu.before_after";
    inline constexpr const char* MENU_LANGUAGE      = "menu.language";

    // Panels
    inline constexpr const char* PANEL_DEVELOP      = "panel.develop";
    inline constexpr const char* PANEL_VIEWPORT     = "panel.viewport";
    inline constexpr const char* PANEL_HISTOGRAM    = "panel.histogram";
    inline constexpr const char* PANEL_METADATA     = "panel.metadata";
    inline constexpr const char* PANEL_FOLDERS      = "panel.folders";
    inline constexpr const char* PANEL_GRID         = "panel.grid";

    // Folder panel
    inline constexpr const char* FOLDER_ADD         = "folder.add";
    inline constexpr const char* FOLDER_REMOVE      = "folder.remove";
    inline constexpr const char* FOLDER_ALL_PHOTOS  = "folder.all_photos";
    inline constexpr const char* IMPORT_PROGRESS    = "import.progress";

    // Crop & Straighten
    inline constexpr const char* CROP_HEADER        = "crop.header";

    // White Balance
    inline constexpr const char* WB_HEADER          = "wb.header";
    inline constexpr const char* WB_TEMPERATURE     = "wb.temperature";
    inline constexpr const char* WB_TINT            = "wb.tint";

    // Tone
    inline constexpr const char* TONE_HEADER        = "tone.header";
    inline constexpr const char* TONE_EXPOSURE      = "tone.exposure";
    inline constexpr const char* TONE_CONTRAST      = "tone.contrast";
    inline constexpr const char* TONE_HIGHLIGHTS    = "tone.highlights";
    inline constexpr const char* TONE_SHADOWS       = "tone.shadows";
    inline constexpr const char* TONE_WHITES        = "tone.whites";
    inline constexpr const char* TONE_BLACKS        = "tone.blacks";

    // Tone Curve
    inline constexpr const char* CURVE_HEADER       = "curve.header";

    // HSL
    inline constexpr const char* HSL_HEADER         = "hsl.header";
    inline constexpr const char* HSL_HUE            = "hsl.hue";
    inline constexpr const char* HSL_SATURATION_TAB = "hsl.saturation";
    inline constexpr const char* HSL_LUMINANCE      = "hsl.luminance";
    inline constexpr const char* HSL_RED            = "hsl.red";
    inline constexpr const char* HSL_ORANGE         = "hsl.orange";
    inline constexpr const char* HSL_YELLOW         = "hsl.yellow";
    inline constexpr const char* HSL_GREEN          = "hsl.green";
    inline constexpr const char* HSL_AQUA           = "hsl.aqua";
    inline constexpr const char* HSL_BLUE           = "hsl.blue";
    inline constexpr const char* HSL_PURPLE         = "hsl.purple";
    inline constexpr const char* HSL_MAGENTA        = "hsl.magenta";

    // Detail
    inline constexpr const char* DETAIL_HEADER      = "detail.header";
    inline constexpr const char* DETAIL_SHARPEN     = "detail.sharpen";
    inline constexpr const char* DETAIL_AMOUNT      = "detail.amount";
    inline constexpr const char* DETAIL_RADIUS      = "detail.radius";
    inline constexpr const char* DETAIL_DETAIL      = "detail.detail";
    inline constexpr const char* DETAIL_MASKING     = "detail.masking";
    inline constexpr const char* DETAIL_NR          = "detail.nr";
    inline constexpr const char* DETAIL_NR_LUMA     = "detail.nr_luma";
    inline constexpr const char* DETAIL_NR_COLOR    = "detail.nr_color";
    inline constexpr const char* DETAIL_NR_DETAIL   = "detail.nr_detail";

    // Effects
    inline constexpr const char* FX_HEADER          = "fx.header";
    inline constexpr const char* FX_VIBRANCE        = "fx.vibrance";
    inline constexpr const char* FX_SATURATION      = "fx.saturation";

    // Status
    inline constexpr const char* STATUS_READY       = "status.ready";
    inline constexpr const char* STATUS_OPEN_HINT   = "status.open_hint";
    inline constexpr const char* STATUS_ZOOM        = "status.zoom";

    // Metadata
    inline constexpr const char* META_CAMERA        = "meta.camera";
    inline constexpr const char* META_LENS          = "meta.lens";

    // WB Eyedropper
    inline constexpr const char* WB_EYEDROPPER      = "wb.eyedropper";
}

struct LanguageInfo {
    std::string code;
    std::string name;
    std::string native;
};

class I18n {
public:
    static I18n& instance();

    // Open language database (call once at startup)
    bool openDatabase(const std::filesystem::path& db_path);
    void closeDatabase();

    // Set current language by code (e.g. "en", "zh_tw")
    void setLanguage(const std::string& lang_code);

    // Legacy enum support
    void setLanguage(Lang lang);
    Lang language() const { return lang_enum_; }

    // Get translated string. Returns key itself if not found.
    const char* get(const char* key) const;
    const char* operator()(const char* key) const { return get(key); }

    // Get list of available languages from DB
    std::vector<LanguageInfo> availableLanguages() const;

    // Current language code
    const std::string& languageCode() const { return lang_code_; }

private:
    I18n();
    ~I18n();

    sqlite3* db_ = nullptr;
    Lang lang_enum_ = Lang::EN;
    std::string lang_code_ = "en";
    std::unordered_map<std::string, std::string> strings_;

    void loadStrings(const std::string& lang_code);
};

// Global shorthand: tr("key")
inline const char* tr(const char* key) { return I18n::instance()(key); }

} // namespace vega
