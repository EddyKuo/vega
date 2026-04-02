#pragma once
#include <windows.h>

namespace vega {

class WindowsIntegration {
public:
    // High DPI support
    static void enableHighDPI();

    // Dark title bar (Windows 10/11)
    static void setDarkTitleBar(HWND hwnd);

    // File drag-drop support
    static void enableDragDrop(HWND hwnd);

    // Taskbar progress bar
    static void setTaskbarProgress(HWND hwnd, float progress);  // 0-1
    static void clearTaskbarProgress(HWND hwnd);
};

} // namespace vega
