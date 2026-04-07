# Library Mode Implementation

Adds Lightroom-style folder browsing and photo library management to Vega.

## Architecture

### Mode Switching
- `AppMode::Library` / `AppMode::Develop` enum controls which panels are visible
- Toolbar Dev/Grid buttons toggle the mode
- Double-clicking a photo in GridView switches to Develop mode with that photo loaded
- Pressing G key returns to Library mode

### New Component: FolderPanel
Left-side panel (`src/ui/FolderPanel.h/.cpp`) showing:
- List of added folder paths with RAW file counts
- "Add Folder" button (Win32 IFileDialog for folder selection)
- "Remove" button on each folder entry
- Clicking a folder filters GridView to show only photos from that folder

### Integration Flow
```
User clicks "Add Folder"
  -> Win32 folder dialog
  -> FolderPanel stores path
  -> Background thread: ImportManager::scanDirectory() + import()
  -> Database populated with PhotoRecords
  -> ThumbnailCache generates thumbnails
  -> GridView refreshes from Database
  -> User sees thumbnails in grid

User double-clicks photo in GridView
  -> Load RAW via RawDecoder::decode()
  -> Switch to Develop mode
  -> Existing pipeline processes the image
```

### Data Flow
```
FolderPanel -> ImportManager::scanDirectory() -> ImportManager::import()
                                                    |
                                              Database (SQLite)
                                                    |
                                              GridView::render() <- ThumbnailCache
```

## Modified Files

| File | Change |
|------|--------|
| `src/ui/FolderPanel.h/cpp` | New: folder list panel with add/remove |
| `src/ui/GridView.h/cpp` | Add double-click callback, folder path display |
| `src/catalog/Database.h/cpp` | Add folder_path to FilterCriteria |
| `src/core/Settings.h/cpp` | Add library_folders persistence |
| `src/core/i18n.h/cpp` | Add i18n strings for Library mode |
| `src/main.cpp` | Mode switching, DB init, import wiring, layout |
| `src/CMakeLists.txt` | Add FolderPanel.cpp to vega_ui |

## Keyboard Shortcuts
- `G` - Switch to Grid/Library mode
- `D` - Switch to Develop mode
- `Ctrl+Shift+I` - Add folder (import)
- Double-click thumbnail - Open in Develop mode
