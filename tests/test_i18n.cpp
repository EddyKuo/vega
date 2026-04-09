#define NOMINMAX
#include <windows.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include <filesystem>

#include "core/i18n.h"

using namespace vega;
using Catch::Matchers::Equals;

// Helper: find language.db by searching from CWD and known paths
static std::filesystem::path findLanguageDb()
{
    namespace fs = std::filesystem;

    // Try multiple paths (CWD-relative, exe-relative)
    fs::path candidates[] = {
        "language.db",
        "tests/language.db",
        "out/build/windows-x64-debug/tests/language.db",
        "out/build/windows-x64-debug/src/language.db",
    };
    for (auto& p : candidates) {
        if (fs::exists(p)) return fs::canonical(p);
    }

    // Also try GetModuleFileName if available
    wchar_t buf[MAX_PATH]{};
    if (GetModuleFileNameW(nullptr, buf, MAX_PATH) > 0) {
        fs::path exe_dir = fs::path(buf).parent_path();
        fs::path exe_candidates[] = {
            exe_dir / "language.db",
            exe_dir / ".." / "src" / "language.db",
        };
        for (auto& p : exe_candidates) {
            if (fs::exists(p)) return fs::canonical(p);
        }
    }

    return {};
}

TEST_CASE("I18n opens language database", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    REQUIRE(i18n.openDatabase(db_path));
}

TEST_CASE("I18n loads English strings", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);
    i18n.setLanguage("en");

    SECTION("Menu strings") {
        CHECK_THAT(i18n.get("menu.file"), Equals("File"));
        CHECK_THAT(i18n.get("menu.open"), Equals("Open RAW..."));
        CHECK_THAT(i18n.get("menu.exit"), Equals("Exit"));
    }

    SECTION("Panel strings") {
        CHECK_THAT(i18n.get("panel.develop"), Equals("Develop"));
        CHECK_THAT(i18n.get("panel.histogram"), Equals("Histogram"));
    }

    SECTION("Tone strings") {
        CHECK_THAT(i18n.get("tone.exposure"), Equals("Exposure"));
        CHECK_THAT(i18n.get("tone.contrast"), Equals("Contrast"));
    }

    SECTION("Slider labels") {
        CHECK_THAT(i18n.get("Temperature"), Equals("Temperature"));
        CHECK_THAT(i18n.get("Amount##sharp"), Equals("Amount"));
    }
}

TEST_CASE("I18n loads Traditional Chinese strings", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);
    i18n.setLanguage("zh_tw");

    // These are UTF-8 encoded Traditional Chinese
    CHECK(std::string(i18n.get("menu.file")) != "menu.file");
    CHECK(std::string(i18n.get("menu.file")) != "File");
    CHECK(std::string(i18n.get("tone.exposure")) != "Exposure");
    CHECK(std::string(i18n.get("panel.develop")) != "Develop");

    // Slider labels should also be translated
    CHECK(std::string(i18n.get("Temperature")) != "Temperature");
}

TEST_CASE("I18n falls back to key for missing translation", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);
    i18n.setLanguage("en");

    // Non-existent key should return the key itself
    CHECK_THAT(i18n.get("nonexistent.key.xyz"), Equals("nonexistent.key.xyz"));
}

TEST_CASE("I18n switching languages reloads strings", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);

    i18n.setLanguage("en");
    std::string en_file = i18n.get("menu.file");
    CHECK_THAT(en_file, Equals("File"));

    i18n.setLanguage("zh_tw");
    std::string zh_file = i18n.get("menu.file");
    CHECK(zh_file != "File");
    CHECK(zh_file != "menu.file");

    // Switch back to English
    i18n.setLanguage("en");
    CHECK_THAT(std::string(i18n.get("menu.file")), Equals("File"));
}

TEST_CASE("I18n available languages", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);

    auto langs = i18n.availableLanguages();
    REQUIRE(langs.size() >= 2);

    bool has_en = false, has_zh = false;
    for (const auto& lang : langs) {
        if (lang.code == "en") has_en = true;
        if (lang.code == "zh_tw") has_zh = true;
    }
    CHECK(has_en);
    CHECK(has_zh);
}

TEST_CASE("I18n tr() global shorthand works", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    I18n::instance().openDatabase(db_path);
    I18n::instance().setLanguage("en");

    CHECK_THAT(std::string(tr("menu.file")), Equals("File"));
    CHECK_THAT(std::string(tr(S::MENU_OPEN)), Equals("Open RAW..."));
}

TEST_CASE("I18n string key constants match database", "[i18n]")
{
    auto db_path = findLanguageDb();
    REQUIRE(!db_path.empty());

    auto& i18n = I18n::instance();
    i18n.openDatabase(db_path);
    i18n.setLanguage("en");

    // Verify all S:: constants resolve to non-key values
    const char* keys[] = {
        S::MENU_FILE, S::MENU_OPEN, S::MENU_SAVE_RECIPE, S::MENU_EXPORT, S::MENU_EXIT,
        S::MENU_EDIT, S::MENU_UNDO, S::MENU_REDO, S::MENU_RESET_ALL,
        S::MENU_VIEW, S::MENU_FIT, S::MENU_BEFORE_AFTER, S::MENU_LANGUAGE,
        S::PANEL_DEVELOP, S::PANEL_VIEWPORT, S::PANEL_HISTOGRAM, S::PANEL_FOLDERS,
        S::FOLDER_ADD, S::FOLDER_REMOVE, S::FOLDER_ALL_PHOTOS,
        S::WB_HEADER, S::WB_TEMPERATURE, S::WB_TINT,
        S::TONE_HEADER, S::TONE_EXPOSURE, S::TONE_CONTRAST,
        S::TONE_HIGHLIGHTS, S::TONE_SHADOWS, S::TONE_WHITES, S::TONE_BLACKS,
        S::CURVE_HEADER,
        S::HSL_HEADER, S::HSL_HUE, S::HSL_SATURATION_TAB, S::HSL_LUMINANCE,
        S::DETAIL_HEADER, S::DETAIL_SHARPEN, S::DETAIL_NR,
        S::FX_HEADER, S::FX_VIBRANCE, S::FX_SATURATION,
        S::STATUS_READY, S::STATUS_OPEN_HINT,
        S::META_CAMERA, S::META_LENS, S::WB_EYEDROPPER,
    };

    for (const char* key : keys) {
        std::string val = i18n.get(key);
        INFO("Key: " << key);
        CHECK(val != key); // translation found, not falling back to key
    }
}
