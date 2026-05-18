//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#ifndef SANDBOX_SETTINGSDATA_H
#define SANDBOX_SETTINGSDATA_H

#include <string>

enum class DisplayMode {
    Windowed,
    Borderless
};

struct SettingsData {
    DisplayMode displayMode = DisplayMode::Windowed;
    DisplayMode originalDisplayMode = DisplayMode::Windowed;
    int monitor = 0;

    std::string filename;
    bool needsApply = false;
    bool showFPS = false;
    bool fpsLock = true;
    bool vsync = true;

    float soundVolume = 1.0f;
    float musicVolume = 0.7f;
};


#endif //SANDBOX_SETTINGSDATA_H
