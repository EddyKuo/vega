#define NOMINMAX
#include "core/WindowsIntegration.h"
#include "core/Logger.h"

#include <dwmapi.h>
#include <shellapi.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <algorithm>

#pragma comment(lib, "dwmapi.lib")

// DWMWA_USE_IMMERSIVE_DARK_MODE was introduced as undocumented attribute 19
// in Windows 10 1809, then officially as attribute 20 in later builds.
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

namespace vega {

void WindowsIntegration::enableHighDPI()
{
    // Try the modern per-monitor v2 awareness first (Windows 10 1703+).
    // This gives the best results with mixed-DPI monitor setups.
    if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
    {
        VEGA_LOG_WARN("Per-monitor DPI v2 not available, falling back to system aware");
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
    }
}

void WindowsIntegration::setDarkTitleBar(HWND hwnd)
{
    BOOL use_dark = TRUE;
    HRESULT hr = DwmSetWindowAttribute(
        hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
        &use_dark, sizeof(use_dark));

    if (FAILED(hr))
    {
        // Try the older undocumented attribute 19 for earlier Windows 10 builds
        hr = DwmSetWindowAttribute(
            hwnd, 19,
            &use_dark, sizeof(use_dark));
    }

    if (FAILED(hr))
        VEGA_LOG_WARN("Failed to set dark title bar (HRESULT: 0x{:08X})", static_cast<unsigned>(hr));
    else
        VEGA_LOG_DEBUG("Dark title bar enabled");
}

void WindowsIntegration::enableDragDrop(HWND hwnd)
{
    DragAcceptFiles(hwnd, TRUE);
    VEGA_LOG_DEBUG("Drag-and-drop enabled");
}

void WindowsIntegration::setTaskbarProgress(HWND hwnd, float progress)
{
    Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
    HRESULT hr = CoCreateInstance(
        CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&taskbar));

    if (FAILED(hr) || !taskbar)
        return;

    hr = taskbar->HrInit();
    if (FAILED(hr))
        return;

    progress = std::clamp(progress, 0.0f, 1.0f);
    ULONGLONG completed = static_cast<ULONGLONG>(progress * 10000.0f);
    taskbar->SetProgressState(hwnd, TBPF_NORMAL);
    taskbar->SetProgressValue(hwnd, completed, 10000ULL);
}

void WindowsIntegration::clearTaskbarProgress(HWND hwnd)
{
    Microsoft::WRL::ComPtr<ITaskbarList3> taskbar;
    HRESULT hr = CoCreateInstance(
        CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&taskbar));

    if (FAILED(hr) || !taskbar)
        return;

    hr = taskbar->HrInit();
    if (FAILED(hr))
        return;

    taskbar->SetProgressState(hwnd, TBPF_NOPROGRESS);
}

} // namespace vega
