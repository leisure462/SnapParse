#pragma once

#include <windows.h>

class WindowBackdropHelper {
public:
    enum BackdropType {
        None = 1,
        Mica = 2,
        Acrylic = 3,
        MicaAlt = 4
    };

    static bool enableBackdrop(HWND hwnd, BackdropType type, bool isDarkMode);
    static bool enableAcrylic(HWND hwnd, bool isDarkMode);
    static bool enableMica(HWND hwnd, bool isDarkMode);
    static void updateTheme(HWND hwnd, bool isDarkMode);
    static void enableRoundedCorners(HWND hwnd);
};
