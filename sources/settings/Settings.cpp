//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#include <fstream>
#include "Settings.h"
#include "raylib.h"
#include "utils/json.hpp"


void ApplySettings(SettingsData& settings)
{
    settings.monitor = GetCurrentMonitor();

    switch (settings.displayMode) {
        case DisplayMode::Windowed: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            break;
        }

        case DisplayMode::Borderless: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            break;
        }
    }

    settings.originalDisplayMode = settings.displayMode;

    if (settings.fpsLock) {
        SetTargetFPS(60);
    } else {
        SetTargetFPS(0);
    }

    settings.needsApply = false;
}

void SaveSettings(const SettingsData& settings) {
    nlohmann::json j;
    j["displayMode"] = static_cast<int>(settings.displayMode);
    j["monitor"] = settings.monitor;
    j["showFPS"] = settings.showFPS;
    j["lockFPS"] = settings.fpsLock;
    j["vsync"] = settings.vsync;
    j["soundVolume"] = settings.soundVolume;
    j["musicVolume"] = settings.musicVolume;

    std::ofstream file(settings.filename);
    if (file) {
        file << j.dump(2);
    }
}

void InitSettings(SettingsData& data, const std::string &filename) {
    data.filename = filename;
    data.monitor = 0;

    std::ifstream file(filename);
    if (file) {
        nlohmann::json j;
        file >> j;

        const int savedDisplayMode = j.value("displayMode", 0);
        if (savedDisplayMode == 1 || savedDisplayMode == 2) {
            data.displayMode = DisplayMode::Borderless;
        } else {
            data.displayMode = DisplayMode::Windowed;
        }

        if (j.contains("monitor")) {
            j["monitor"].get_to(data.monitor);
        }
        if (j.contains("showFPS")) {
            j["showFPS"].get_to(data.showFPS);
        }
        if (j.contains("lockFPS")) {
            j["lockFPS"].get_to(data.fpsLock);
        }
        if (j.contains("vsync")) {
            j["vsync"].get_to(data.vsync);
        }
        if (j.contains("soundVolume")) {
            j["soundVolume"].get_to(data.soundVolume);
        }
        if (j.contains("musicVolume")) {
            j["musicVolume"].get_to(data.musicVolume);
        }
    }

    data.originalDisplayMode = data.displayMode;
}

