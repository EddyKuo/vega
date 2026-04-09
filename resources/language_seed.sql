-- Vega i18n language database seed
-- This file is used at build time to generate language.db

CREATE TABLE IF NOT EXISTS languages (
    code    TEXT PRIMARY KEY,
    name    TEXT NOT NULL,
    native  TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS translations (
    lang    TEXT NOT NULL,
    key     TEXT NOT NULL,
    value   TEXT NOT NULL,
    PRIMARY KEY (lang, key),
    FOREIGN KEY (lang) REFERENCES languages(code)
);

CREATE INDEX IF NOT EXISTS idx_translations_lang ON translations(lang);

-- Supported languages
INSERT INTO languages VALUES ('en',    'English',             'English');
INSERT INTO languages VALUES ('zh_tw', 'Traditional Chinese', '繁體中文');

-- ============================================================================
-- English (en)
-- ============================================================================

-- Menu
INSERT INTO translations VALUES ('en', 'menu.file',         'File');
INSERT INTO translations VALUES ('en', 'menu.open',         'Open RAW...');
INSERT INTO translations VALUES ('en', 'menu.save_recipe',  'Save Recipe');
INSERT INTO translations VALUES ('en', 'menu.export',       'Export...');
INSERT INTO translations VALUES ('en', 'menu.exit',         'Exit');
INSERT INTO translations VALUES ('en', 'menu.edit',         'Edit');
INSERT INTO translations VALUES ('en', 'menu.undo',         'Undo');
INSERT INTO translations VALUES ('en', 'menu.redo',         'Redo');
INSERT INTO translations VALUES ('en', 'menu.reset_all',    'Reset All');
INSERT INTO translations VALUES ('en', 'menu.view',         'View');
INSERT INTO translations VALUES ('en', 'menu.fit',          'Fit to Window');
INSERT INTO translations VALUES ('en', 'menu.before_after', 'Before/After');
INSERT INTO translations VALUES ('en', 'menu.language',     'Language');

-- Panels
INSERT INTO translations VALUES ('en', 'panel.develop',     'Develop');
INSERT INTO translations VALUES ('en', 'panel.viewport',    'Viewport');
INSERT INTO translations VALUES ('en', 'panel.histogram',   'Histogram');
INSERT INTO translations VALUES ('en', 'panel.metadata',    'Metadata');
INSERT INTO translations VALUES ('en', 'panel.folders',     'Folders');
INSERT INTO translations VALUES ('en', 'panel.grid',        'Library');

-- Folder
INSERT INTO translations VALUES ('en', 'folder.add',        '+ Add Folder');
INSERT INTO translations VALUES ('en', 'folder.remove',     'Remove Folder');
INSERT INTO translations VALUES ('en', 'folder.all_photos', 'All Photos');
INSERT INTO translations VALUES ('en', 'import.progress',   'Importing: %d / %d');

-- White Balance
INSERT INTO translations VALUES ('en', 'wb.header',         'White Balance');
INSERT INTO translations VALUES ('en', 'wb.temperature',    'Temperature');
INSERT INTO translations VALUES ('en', 'wb.tint',           'Tint');
INSERT INTO translations VALUES ('en', 'wb.eyedropper',     'WB Eyedropper');

-- Tone
INSERT INTO translations VALUES ('en', 'tone.header',       'Tone');
INSERT INTO translations VALUES ('en', 'tone.exposure',     'Exposure');
INSERT INTO translations VALUES ('en', 'tone.contrast',     'Contrast');
INSERT INTO translations VALUES ('en', 'tone.highlights',   'Highlights');
INSERT INTO translations VALUES ('en', 'tone.shadows',      'Shadows');
INSERT INTO translations VALUES ('en', 'tone.whites',       'Whites');
INSERT INTO translations VALUES ('en', 'tone.blacks',       'Blacks');

-- Tone Curve
INSERT INTO translations VALUES ('en', 'curve.header',      'Tone Curve');

-- HSL
INSERT INTO translations VALUES ('en', 'hsl.header',        'HSL / Color');
INSERT INTO translations VALUES ('en', 'hsl.hue',           'Hue');
INSERT INTO translations VALUES ('en', 'hsl.saturation',    'Saturation');
INSERT INTO translations VALUES ('en', 'hsl.luminance',     'Luminance');
INSERT INTO translations VALUES ('en', 'hsl.red',           'Red');
INSERT INTO translations VALUES ('en', 'hsl.orange',        'Orange');
INSERT INTO translations VALUES ('en', 'hsl.yellow',        'Yellow');
INSERT INTO translations VALUES ('en', 'hsl.green',         'Green');
INSERT INTO translations VALUES ('en', 'hsl.aqua',          'Aqua');
INSERT INTO translations VALUES ('en', 'hsl.blue',          'Blue');
INSERT INTO translations VALUES ('en', 'hsl.purple',        'Purple');
INSERT INTO translations VALUES ('en', 'hsl.magenta',       'Magenta');

-- Detail
INSERT INTO translations VALUES ('en', 'detail.header',     'Detail');
INSERT INTO translations VALUES ('en', 'detail.sharpen',    'Sharpening');
INSERT INTO translations VALUES ('en', 'detail.amount',     'Amount');
INSERT INTO translations VALUES ('en', 'detail.radius',     'Radius');
INSERT INTO translations VALUES ('en', 'detail.detail',     'Detail');
INSERT INTO translations VALUES ('en', 'detail.masking',    'Masking');
INSERT INTO translations VALUES ('en', 'detail.nr',         'Noise Reduction');
INSERT INTO translations VALUES ('en', 'detail.nr_luma',    'Luminance');
INSERT INTO translations VALUES ('en', 'detail.nr_color',   'Color');
INSERT INTO translations VALUES ('en', 'detail.nr_detail',  'Detail');

-- Effects
INSERT INTO translations VALUES ('en', 'fx.header',         'Effects');
INSERT INTO translations VALUES ('en', 'fx.vibrance',       'Vibrance');
INSERT INTO translations VALUES ('en', 'fx.saturation',     'Saturation');

-- Status
INSERT INTO translations VALUES ('en', 'status.ready',      'Ready');
INSERT INTO translations VALUES ('en', 'status.open_hint',  'Ctrl+O or File > Open RAW to get started');
INSERT INTO translations VALUES ('en', 'status.zoom',       'Zoom');

-- Metadata
INSERT INTO translations VALUES ('en', 'meta.camera',       'Camera');
INSERT INTO translations VALUES ('en', 'meta.lens',         'Lens');

-- Slider labels (used by vegaSlider with ImGui ## IDs)
INSERT INTO translations VALUES ('en', 'Temperature',       'Temperature');
INSERT INTO translations VALUES ('en', 'Tint',              'Tint');
INSERT INTO translations VALUES ('en', 'Exposure',          'Exposure');
INSERT INTO translations VALUES ('en', 'Contrast',          'Contrast');
INSERT INTO translations VALUES ('en', 'Highlights',        'Highlights');
INSERT INTO translations VALUES ('en', 'Shadows',           'Shadows');
INSERT INTO translations VALUES ('en', 'Whites',            'Whites');
INSERT INTO translations VALUES ('en', 'Blacks',            'Blacks');
INSERT INTO translations VALUES ('en', 'Red',               'Red');
INSERT INTO translations VALUES ('en', 'Orange',            'Orange');
INSERT INTO translations VALUES ('en', 'Yellow',            'Yellow');
INSERT INTO translations VALUES ('en', 'Green',             'Green');
INSERT INTO translations VALUES ('en', 'Aqua',              'Aqua');
INSERT INTO translations VALUES ('en', 'Blue',              'Blue');
INSERT INTO translations VALUES ('en', 'Purple',            'Purple');
INSERT INTO translations VALUES ('en', 'Magenta',           'Magenta');
INSERT INTO translations VALUES ('en', 'Amount##sharp',     'Amount');
INSERT INTO translations VALUES ('en', 'Radius##sharp',     'Radius');
INSERT INTO translations VALUES ('en', 'Detail##sharp',     'Detail');
INSERT INTO translations VALUES ('en', 'Masking##sharp',    'Masking');
INSERT INTO translations VALUES ('en', 'Luminance##nr',     'Luminance');
INSERT INTO translations VALUES ('en', 'Color##nr',         'Color');
INSERT INTO translations VALUES ('en', 'Detail##nr',        'Detail');
INSERT INTO translations VALUES ('en', 'Vibrance',          'Vibrance');
INSERT INTO translations VALUES ('en', 'Saturation##effect','Saturation');
INSERT INTO translations VALUES ('en', 'Hue',               'Hue');
INSERT INTO translations VALUES ('en', 'Saturation',        'Saturation');
INSERT INTO translations VALUES ('en', 'Luminance',         'Luminance');
INSERT INTO translations VALUES ('en', 'Metadata',          'Metadata');
INSERT INTO translations VALUES ('en', 'Sharpening',        'Sharpening');
INSERT INTO translations VALUES ('en', 'Noise Reduction',   'Noise Reduction');

-- ============================================================================
-- Traditional Chinese (zh_tw)
-- ============================================================================

-- Menu
INSERT INTO translations VALUES ('zh_tw', 'menu.file',         '檔案');
INSERT INTO translations VALUES ('zh_tw', 'menu.open',         '開啟 RAW...');
INSERT INTO translations VALUES ('zh_tw', 'menu.save_recipe',  '儲存設定');
INSERT INTO translations VALUES ('zh_tw', 'menu.export',       '匯出...');
INSERT INTO translations VALUES ('zh_tw', 'menu.exit',         '結束');
INSERT INTO translations VALUES ('zh_tw', 'menu.edit',         '編輯');
INSERT INTO translations VALUES ('zh_tw', 'menu.undo',         '復原');
INSERT INTO translations VALUES ('zh_tw', 'menu.redo',         '重做');
INSERT INTO translations VALUES ('zh_tw', 'menu.reset_all',    '重設全部');
INSERT INTO translations VALUES ('zh_tw', 'menu.view',         '檢視');
INSERT INTO translations VALUES ('zh_tw', 'menu.fit',          '適合視窗');
INSERT INTO translations VALUES ('zh_tw', 'menu.before_after', '前後對比');
INSERT INTO translations VALUES ('zh_tw', 'menu.language',     '語言');

-- Panels
INSERT INTO translations VALUES ('zh_tw', 'panel.develop',     '編輯');
INSERT INTO translations VALUES ('zh_tw', 'panel.viewport',    '預覽');
INSERT INTO translations VALUES ('zh_tw', 'panel.histogram',   '直方圖');
INSERT INTO translations VALUES ('zh_tw', 'panel.metadata',    '元資料');
INSERT INTO translations VALUES ('zh_tw', 'panel.folders',     '資料夾');
INSERT INTO translations VALUES ('zh_tw', 'panel.grid',        '圖庫');

-- Folder
INSERT INTO translations VALUES ('zh_tw', 'folder.add',        '+ 新增資料夾');
INSERT INTO translations VALUES ('zh_tw', 'folder.remove',     '移除資料夾');
INSERT INTO translations VALUES ('zh_tw', 'folder.all_photos', '所有照片');
INSERT INTO translations VALUES ('zh_tw', 'import.progress',   '匯入中: %d / %d');

-- White Balance
INSERT INTO translations VALUES ('zh_tw', 'wb.header',         '白平衡');
INSERT INTO translations VALUES ('zh_tw', 'wb.temperature',    '色溫');
INSERT INTO translations VALUES ('zh_tw', 'wb.tint',           '色調');
INSERT INTO translations VALUES ('zh_tw', 'wb.eyedropper',     '白平衡吸管');

-- Tone
INSERT INTO translations VALUES ('zh_tw', 'tone.header',       '色調');
INSERT INTO translations VALUES ('zh_tw', 'tone.exposure',     '曝光度');
INSERT INTO translations VALUES ('zh_tw', 'tone.contrast',     '對比');
INSERT INTO translations VALUES ('zh_tw', 'tone.highlights',   '亮部');
INSERT INTO translations VALUES ('zh_tw', 'tone.shadows',      '暗部');
INSERT INTO translations VALUES ('zh_tw', 'tone.whites',       '白色');
INSERT INTO translations VALUES ('zh_tw', 'tone.blacks',       '黑色');

-- Tone Curve
INSERT INTO translations VALUES ('zh_tw', 'curve.header',      '色調曲線');

-- HSL
INSERT INTO translations VALUES ('zh_tw', 'hsl.header',        'HSL / 色彩');
INSERT INTO translations VALUES ('zh_tw', 'hsl.hue',           '色相');
INSERT INTO translations VALUES ('zh_tw', 'hsl.saturation',    '飽和度');
INSERT INTO translations VALUES ('zh_tw', 'hsl.luminance',     '明度');
INSERT INTO translations VALUES ('zh_tw', 'hsl.red',           '紅色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.orange',        '橙色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.yellow',        '黃色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.green',         '綠色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.aqua',          '水色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.blue',          '藍色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.purple',        '紫色');
INSERT INTO translations VALUES ('zh_tw', 'hsl.magenta',       '洋紅');

-- Detail
INSERT INTO translations VALUES ('zh_tw', 'detail.header',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'detail.sharpen',    '銳利化');
INSERT INTO translations VALUES ('zh_tw', 'detail.amount',     '總量');
INSERT INTO translations VALUES ('zh_tw', 'detail.radius',     '半徑');
INSERT INTO translations VALUES ('zh_tw', 'detail.detail',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'detail.masking',    '遮罩');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr',         '降噪');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_luma',    '亮度');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_color',   '色彩');
INSERT INTO translations VALUES ('zh_tw', 'detail.nr_detail',  '細節');

-- Effects
INSERT INTO translations VALUES ('zh_tw', 'fx.header',         '效果');
INSERT INTO translations VALUES ('zh_tw', 'fx.vibrance',       '自然飽和度');
INSERT INTO translations VALUES ('zh_tw', 'fx.saturation',     '飽和度');

-- Status
INSERT INTO translations VALUES ('zh_tw', 'status.ready',      '就緒');
INSERT INTO translations VALUES ('zh_tw', 'status.open_hint',  'Ctrl+O 或 檔案 > 開啟 RAW');
INSERT INTO translations VALUES ('zh_tw', 'status.zoom',       '縮放');

-- Metadata
INSERT INTO translations VALUES ('zh_tw', 'meta.camera',       '相機');
INSERT INTO translations VALUES ('zh_tw', 'meta.lens',         '鏡頭');

-- Slider labels
INSERT INTO translations VALUES ('zh_tw', 'Temperature',       '色溫');
INSERT INTO translations VALUES ('zh_tw', 'Tint',              '色調');
INSERT INTO translations VALUES ('zh_tw', 'Exposure',          '曝光度');
INSERT INTO translations VALUES ('zh_tw', 'Contrast',          '對比');
INSERT INTO translations VALUES ('zh_tw', 'Highlights',        '亮部');
INSERT INTO translations VALUES ('zh_tw', 'Shadows',           '暗部');
INSERT INTO translations VALUES ('zh_tw', 'Whites',            '白色');
INSERT INTO translations VALUES ('zh_tw', 'Blacks',            '黑色');
INSERT INTO translations VALUES ('zh_tw', 'Red',               '紅色');
INSERT INTO translations VALUES ('zh_tw', 'Orange',            '橙色');
INSERT INTO translations VALUES ('zh_tw', 'Yellow',            '黃色');
INSERT INTO translations VALUES ('zh_tw', 'Green',             '綠色');
INSERT INTO translations VALUES ('zh_tw', 'Aqua',              '水色');
INSERT INTO translations VALUES ('zh_tw', 'Blue',              '藍色');
INSERT INTO translations VALUES ('zh_tw', 'Purple',            '紫色');
INSERT INTO translations VALUES ('zh_tw', 'Magenta',           '洋紅');
INSERT INTO translations VALUES ('zh_tw', 'Amount##sharp',     '總量');
INSERT INTO translations VALUES ('zh_tw', 'Radius##sharp',     '半徑');
INSERT INTO translations VALUES ('zh_tw', 'Detail##sharp',     '細節');
INSERT INTO translations VALUES ('zh_tw', 'Masking##sharp',    '遮罩');
INSERT INTO translations VALUES ('zh_tw', 'Luminance##nr',     '亮度');
INSERT INTO translations VALUES ('zh_tw', 'Color##nr',         '色彩');
INSERT INTO translations VALUES ('zh_tw', 'Detail##nr',        '細節');
INSERT INTO translations VALUES ('zh_tw', 'Vibrance',          '自然飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Saturation##effect','飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Hue',               '色相');
INSERT INTO translations VALUES ('zh_tw', 'Saturation',        '飽和度');
INSERT INTO translations VALUES ('zh_tw', 'Luminance',         '明度');
INSERT INTO translations VALUES ('zh_tw', 'Metadata',          '元資料');
INSERT INTO translations VALUES ('zh_tw', 'Sharpening',        '銳利化');
INSERT INTO translations VALUES ('zh_tw', 'Noise Reduction',   '降噪');
