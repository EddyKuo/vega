#include "core/i18n.h"

namespace vega {

I18n& I18n::instance()
{
    static I18n inst;
    return inst;
}

I18n::I18n()
{
    initEN();
    initZH();
}

void I18n::setLanguage(Lang lang)
{
    lang_ = lang;
}

const char* I18n::get(const char* key) const
{
    auto& map = current();
    auto it = map.find(key);
    if (it != map.end())
        return it->second.c_str();
    return key;
}

const std::unordered_map<std::string, std::string>& I18n::current() const
{
    return (lang_ == Lang::ZH_TW) ? strings_zh_ : strings_en_;
}

void I18n::initEN()
{
    auto& m = strings_en_;

    // Menu
    m["menu.file"]          = "File";
    m["menu.open"]          = "Open RAW...";
    m["menu.save_recipe"]   = "Save Recipe";
    m["menu.export"]        = "Export...";
    m["menu.exit"]          = "Exit";
    m["menu.edit"]          = "Edit";
    m["menu.undo"]          = "Undo";
    m["menu.redo"]          = "Redo";
    m["menu.reset_all"]     = "Reset All";
    m["menu.view"]          = "View";
    m["menu.fit"]           = "Fit to Window";
    m["menu.before_after"]  = "Before/After";
    m["menu.language"]      = "Language";

    // Panels
    m["panel.develop"]      = "Develop";
    m["panel.viewport"]     = "Viewport";
    m["panel.histogram"]    = "Histogram";
    m["panel.metadata"]     = "Metadata";
    m["panel.folders"]      = "Folders";
    m["panel.grid"]         = "Library";
    m["folder.add"]         = "+ Add Folder";
    m["folder.remove"]      = "Remove Folder";
    m["folder.all_photos"]  = "All Photos";
    m["import.progress"]    = "Importing: %d / %d";

    // White Balance
    m["wb.header"]          = "White Balance";
    m["wb.temperature"]     = "Temperature";
    m["wb.tint"]            = "Tint";
    m["wb.eyedropper"]      = "WB Eyedropper";

    // Tone
    m["tone.header"]        = "Tone";
    m["tone.exposure"]      = "Exposure";
    m["tone.contrast"]      = "Contrast";
    m["tone.highlights"]    = "Highlights";
    m["tone.shadows"]       = "Shadows";
    m["tone.whites"]        = "Whites";
    m["tone.blacks"]        = "Blacks";

    // Tone Curve
    m["curve.header"]       = "Tone Curve";

    // HSL
    m["hsl.header"]         = "HSL / Color";
    m["hsl.hue"]            = "Hue";
    m["hsl.saturation"]     = "Saturation";
    m["hsl.luminance"]      = "Luminance";
    m["hsl.red"]            = "Red";
    m["hsl.orange"]         = "Orange";
    m["hsl.yellow"]         = "Yellow";
    m["hsl.green"]          = "Green";
    m["hsl.aqua"]           = "Aqua";
    m["hsl.blue"]           = "Blue";
    m["hsl.purple"]         = "Purple";
    m["hsl.magenta"]        = "Magenta";

    // Detail
    m["detail.header"]      = "Detail";
    m["detail.sharpen"]     = "Sharpening";
    m["detail.amount"]      = "Amount";
    m["detail.radius"]      = "Radius";
    m["detail.detail"]      = "Detail";
    m["detail.masking"]     = "Masking";
    m["detail.nr"]          = "Noise Reduction";
    m["detail.nr_luma"]     = "Luminance";
    m["detail.nr_color"]    = "Color";
    m["detail.nr_detail"]   = "Detail";

    // Effects
    m["fx.header"]          = "Effects";
    m["fx.vibrance"]        = "Vibrance";
    m["fx.saturation"]      = "Saturation";

    // Status
    m["status.ready"]       = "Ready";
    m["status.open_hint"]   = "Ctrl+O or File > Open RAW to get started";
    m["status.zoom"]        = "Zoom";

    // Metadata
    m["meta.camera"]        = "Camera";
    m["meta.lens"]          = "Lens";

    // Slider labels (used directly by vegaSlider)
    m["Temperature"]        = "Temperature";
    m["Tint"]               = "Tint";
    m["Exposure"]           = "Exposure";
    m["Contrast"]           = "Contrast";
    m["Highlights"]         = "Highlights";
    m["Shadows"]            = "Shadows";
    m["Whites"]             = "Whites";
    m["Blacks"]             = "Blacks";
    m["Red"]                = "Red";
    m["Orange"]             = "Orange";
    m["Yellow"]             = "Yellow";
    m["Green"]              = "Green";
    m["Aqua"]               = "Aqua";
    m["Blue"]               = "Blue";
    m["Purple"]             = "Purple";
    m["Magenta"]            = "Magenta";
    m["Amount##sharp"]      = "Amount";
    m["Radius##sharp"]      = "Radius";
    m["Detail##sharp"]      = "Detail";
    m["Masking##sharp"]     = "Masking";
    m["Luminance##nr"]      = "Luminance";
    m["Color##nr"]          = "Color";
    m["Detail##nr"]         = "Detail";
    m["Vibrance"]           = "Vibrance";
    m["Saturation##effect"] = "Saturation";
    m["Hue"]                = "Hue";
    m["Saturation"]         = "Saturation";
    m["Luminance"]          = "Luminance";
    m["Metadata"]           = "Metadata";
    m["Sharpening"]         = "Sharpening";
    m["Noise Reduction"]    = "Noise Reduction";
}

void I18n::initZH()
{
    auto& m = strings_zh_;

    // Menu
    m["menu.file"]          = "\u6a94\u6848";           // 檔案
    m["menu.open"]          = "\u958b\u555f RAW...";     // 開啟 RAW...
    m["menu.save_recipe"]   = "\u5132\u5b58\u8a2d\u5b9a"; // 儲存設定
    m["menu.export"]        = "\u532f\u51fa...";         // 匯出...
    m["menu.exit"]          = "\u7d50\u675f";           // 結束
    m["menu.edit"]          = "\u7de8\u8f2f";           // 編輯
    m["menu.undo"]          = "\u5fa9\u539f";           // 復原
    m["menu.redo"]          = "\u91cd\u505a";           // 重做
    m["menu.reset_all"]     = "\u91cd\u8a2d\u5168\u90e8"; // 重設全部
    m["menu.view"]          = "\u6aa2\u8996";           // 檢視
    m["menu.fit"]           = "\u9069\u5408\u8996\u7a97"; // 適合視窗
    m["menu.before_after"]  = "\u524d\u5f8c\u5c0d\u6bd4"; // 前後對比
    m["menu.language"]      = "\u8a9e\u8a00";           // 語言

    // Panels
    m["panel.develop"]      = "\u7de8\u8f2f";           // 編輯
    m["panel.viewport"]     = "\u9810\u89bd";           // 預覽
    m["panel.histogram"]    = "\u76f4\u65b9\u5716";     // 直方圖
    m["panel.metadata"]     = "\u5143\u8cc7\u6599";     // 元資料
    m["panel.folders"]      = "\u8cc7\u6599\u593e";     // 資料夾
    m["panel.grid"]         = "\u5716\u5eab";           // 圖庫
    m["folder.add"]         = "+ \u65b0\u589e\u8cc7\u6599\u593e";  // + 新增資料夾
    m["folder.remove"]      = "\u79fb\u9664\u8cc7\u6599\u593e";     // 移除資料夾
    m["folder.all_photos"]  = "\u6240\u6709\u7167\u7247";           // 所有照片
    m["import.progress"]    = "\u532f\u5165\u4e2d: %d / %d";        // 匯入中: %d / %d

    // White Balance
    m["wb.header"]          = "\u767d\u5e73\u8861";     // 白平衡
    m["wb.temperature"]     = "\u8272\u6eab";           // 色溫
    m["wb.tint"]            = "\u8272\u8abf";           // 色調
    m["wb.eyedropper"]      = "\u767d\u5e73\u8861\u5438\u7ba1"; // 白平衡吸管

    // Tone
    m["tone.header"]        = "\u8272\u8abf";           // 色調
    m["tone.exposure"]      = "\u66dd\u5149\u5ea6";     // 曝光度
    m["tone.contrast"]      = "\u5c0d\u6bd4";           // 對比
    m["tone.highlights"]    = "\u4eae\u90e8";           // 亮部
    m["tone.shadows"]       = "\u6697\u90e8";           // 暗部
    m["tone.whites"]        = "\u767d\u8272";           // 白色
    m["tone.blacks"]        = "\u9ed1\u8272";           // 黑色

    // Tone Curve
    m["curve.header"]       = "\u8272\u8abf\u66f2\u7dda"; // 色調曲線

    // HSL
    m["hsl.header"]         = "HSL / \u8272\u5f69";    // HSL / 色彩
    m["hsl.hue"]            = "\u8272\u76f8";           // 色相
    m["hsl.saturation"]     = "\u98fd\u548c\u5ea6";     // 飽和度
    m["hsl.luminance"]      = "\u660e\u5ea6";           // 明度
    m["hsl.red"]            = "\u7d05\u8272";           // 紅色
    m["hsl.orange"]         = "\u6a59\u8272";           // 橙色
    m["hsl.yellow"]         = "\u9ec3\u8272";           // 黃色
    m["hsl.green"]          = "\u7da0\u8272";           // 綠色
    m["hsl.aqua"]           = "\u6c34\u8272";           // 水色
    m["hsl.blue"]           = "\u85cd\u8272";           // 藍色
    m["hsl.purple"]         = "\u7d2b\u8272";           // 紫色
    m["hsl.magenta"]        = "\u6d0b\u7d05";           // 洋紅

    // Detail
    m["detail.header"]      = "\u7d30\u7bc0";           // 細節
    m["detail.sharpen"]     = "\u92b3\u5229\u5316";     // 銳利化
    m["detail.amount"]      = "\u7e3d\u91cf";           // 總量
    m["detail.radius"]      = "\u534a\u5f91";           // 半徑
    m["detail.detail"]      = "\u7d30\u7bc0";           // 細節
    m["detail.masking"]     = "\u906e\u7f69";           // 遮罩
    m["detail.nr"]          = "\u964d\u566a";           // 降噪
    m["detail.nr_luma"]     = "\u4eae\u5ea6";           // 亮度
    m["detail.nr_color"]    = "\u8272\u5f69";           // 色彩
    m["detail.nr_detail"]   = "\u7d30\u7bc0";           // 細節

    // Effects
    m["fx.header"]          = "\u6548\u679c";           // 效果
    m["fx.vibrance"]        = "\u81ea\u7136\u98fd\u548c\u5ea6"; // 自然飽和度
    m["fx.saturation"]      = "\u98fd\u548c\u5ea6";     // 飽和度

    // Status
    m["status.ready"]       = "\u5c31\u7dd2";           // 就緒
    m["status.open_hint"]   = "Ctrl+O \u6216 \u6a94\u6848 > \u958b\u555f RAW"; // Ctrl+O 或 檔案 > 開啟 RAW
    m["status.zoom"]        = "\u7e2e\u653e";           // 縮放

    // Metadata
    m["meta.camera"]        = "\u76f8\u6a5f";           // 相機
    m["meta.lens"]          = "\u93e1\u982d";           // 鏡頭

    // Slider labels (used directly by vegaSlider)
    m["Temperature"]        = "\u8272\u6eab";           // 色溫
    m["Tint"]               = "\u8272\u8abf";           // 色調
    m["Exposure"]           = "\u66dd\u5149\u5ea6";     // 曝光度
    m["Contrast"]           = "\u5c0d\u6bd4";           // 對比
    m["Highlights"]         = "\u4eae\u90e8";           // 亮部
    m["Shadows"]            = "\u6697\u90e8";           // 暗部
    m["Whites"]             = "\u767d\u8272";           // 白色
    m["Blacks"]             = "\u9ed1\u8272";           // 黑色
    m["Red"]                = "\u7d05\u8272";           // 紅色
    m["Orange"]             = "\u6a59\u8272";           // 橙色
    m["Yellow"]             = "\u9ec3\u8272";           // 黃色
    m["Green"]              = "\u7da0\u8272";           // 綠色
    m["Aqua"]               = "\u6c34\u8272";           // 水色
    m["Blue"]               = "\u85cd\u8272";           // 藍色
    m["Purple"]             = "\u7d2b\u8272";           // 紫色
    m["Magenta"]            = "\u6d0b\u7d05";           // 洋紅
    m["Amount##sharp"]      = "\u7e3d\u91cf";           // 總量
    m["Radius##sharp"]      = "\u534a\u5f91";           // 半徑
    m["Detail##sharp"]      = "\u7d30\u7bc0";           // 細節
    m["Masking##sharp"]     = "\u906e\u7f69";           // 遮罩
    m["Luminance##nr"]      = "\u4eae\u5ea6";           // 亮度
    m["Color##nr"]          = "\u8272\u5f69";           // 色彩
    m["Detail##nr"]         = "\u7d30\u7bc0";           // 細節
    m["Vibrance"]           = "\u81ea\u7136\u98fd\u548c\u5ea6"; // 自然飽和度
    m["Saturation##effect"] = "\u98fd\u548c\u5ea6";     // 飽和度
    m["Hue"]                = "\u8272\u76f8";           // 色相
    m["Saturation"]         = "\u98fd\u548c\u5ea6";     // 飽和度
    m["Luminance"]          = "\u660e\u5ea6";           // 明度
    m["Metadata"]           = "\u5143\u8cc7\u6599";     // 元資料
    m["Sharpening"]         = "\u92b3\u5229\u5316";     // 銳利化
    m["Noise Reduction"]    = "\u964d\u566a";           // 降噪
}

} // namespace vega
