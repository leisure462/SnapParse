#pragma once

#include <QString>

class AutoStartManager {
public:
    static bool isAutoStartEnabled();
    static bool setAutoStartEnabled(bool enabled);
};
