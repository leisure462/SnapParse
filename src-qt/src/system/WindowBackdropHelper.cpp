#include "WindowBackdropHelper.h"
#include <dwmapi.h>
#include <versionhelpers.h>

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// Windows 10 SetWindowCompositionAttribute Structures
enum ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
    ACCENT_INVALID_STATE = 5
};

struct ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
};

struct WINCOMPATTRDATA {
    DWORD dwAttribute;
    PVOID pvData;
    ULONG cbData;
};

typedef BOOL(WINAPI* pfnSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);

static bool applyWin10Acrylic(HWND hwnd, bool isDark) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return false;

    pfnSetWindowCompositionAttribute SetWindowCompositionAttribute =
        (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!SetWindowCompositionAttribute) return false;

    DWORD gradientColor = isDark ? 0xEE202020 : 0xEEF0F0F0;
    ACCENT_POLICY policy = { ACCENT_ENABLE_ACRYLICBLURBEHIND, 0, gradientColor, 0 };
    WINCOMPATTRDATA data = { 19, &policy, sizeof(policy) };

    return SetWindowCompositionAttribute(hwnd, &data) != FALSE;
}

static bool applyWin10Disabled(HWND hwnd) {
    HMODULE hUser = GetModuleHandleW(L"user32.dll");
    if (!hUser) return false;

    pfnSetWindowCompositionAttribute SetWindowCompositionAttribute =
        (pfnSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
    if (!SetWindowCompositionAttribute) return false;

    ACCENT_POLICY policy = { ACCENT_DISABLED, 0, 0, 0 };
    WINCOMPATTRDATA data = { 19, &policy, sizeof(policy) };

    return SetWindowCompositionAttribute(hwnd, &data) != FALSE;
}

bool WindowBackdropHelper::enableBackdrop(HWND hwnd, BackdropType type, bool isDarkMode) {
    if (!hwnd || !IsWindow(hwnd)) return false;

    // 1. Extend Frame into Client Area for full DWM blur & shadow
    MARGINS margins = { -1, -1, -1, -1 };
    DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 2. Set Dark Mode attribute for DWM
    BOOL dark = isDarkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // 3. Set Rounded Corners (10px smooth anti-aliased)
    DWORD corner = 2; // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    // 4. Set Windows 11 System Backdrop (Acrylic / Mica / None)
    DWORD backdrop = static_cast<DWORD>(type);
    HRESULT hr = DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop, sizeof(backdrop));
    if (SUCCEEDED(hr)) {
        return true;
    }

    // 5. Fallback for Windows 10 (Build 1809+)
    if (type == Acrylic) {
        return applyWin10Acrylic(hwnd, isDarkMode);
    } else if (type == None) {
        return applyWin10Disabled(hwnd);
    }

    return false;
}

bool WindowBackdropHelper::enableAcrylic(HWND hwnd, bool isDarkMode) {
    return enableBackdrop(hwnd, Acrylic, isDarkMode);
}

bool WindowBackdropHelper::enableMica(HWND hwnd, bool isDarkMode) {
    return enableBackdrop(hwnd, Mica, isDarkMode);
}

void WindowBackdropHelper::updateTheme(HWND hwnd, bool isDarkMode) {
    if (!hwnd || !IsWindow(hwnd)) return;
    BOOL dark = isDarkMode ? TRUE : FALSE;
    DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

void WindowBackdropHelper::enableRoundedCorners(HWND hwnd) {
    if (!hwnd || !IsWindow(hwnd)) return;
    DWORD corner = 2;
    DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));
}
