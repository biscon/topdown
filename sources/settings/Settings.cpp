//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#include <fstream>
#include "Settings.h"
#include "raylib.h"
#include "utils/json.hpp"
#include "data/GameState.h"


void ApplySettings(SettingsData& settings)
{
    if (settings.availableResolutions.empty()) {
        RefreshResolutions(settings);
    }

    if (settings.availableResolutions.empty()) {
        settings.availableResolutions.push_back({1920, 1080});
    }

    if (settings.selectedResolutionIndex < 0 ||
        settings.selectedResolutionIndex >= static_cast<int>(settings.availableResolutions.size())) {
        settings.selectedResolutionIndex = 0;
    }

    const Resolution res = settings.availableResolutions[settings.selectedResolutionIndex];
    settings.monitor = GetCurrentMonitor();

    switch (settings.displayMode) {
        case DisplayMode::Windowed: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            ClearWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SetWindowSize(res.width, res.height);
            break;
        }

        case DisplayMode::Borderless: {
            ClearWindowState(FLAG_FULLSCREEN_MODE);
            SetWindowState(FLAG_BORDERLESS_WINDOWED_MODE);
            SetWindowSize(GetMonitorWidth(settings.monitor), GetMonitorHeight(settings.monitor));
            break;
        }
    }

    settings.originalResolutionIndex = settings.selectedResolutionIndex;
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
    j["resolutionIndex"] = settings.selectedResolutionIndex;
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

void RefreshResolutions(SettingsData& data)
{
    const int monitorCount = GetMonitorCount();
    const int monitor = (monitorCount > 0) ? 0 : 0;
    data.monitor = monitor;
    data.availableResolutions.clear();

    const int monWidth = GetMonitorWidth(monitor);
    const int monHeight = GetMonitorHeight(monitor);

    if (monWidth >= 1920 && monHeight >= 1080) {
        data.availableResolutions.push_back({1920, 1080});
    }
    if (monWidth >= 2560 && monHeight >= 1440) {
        data.availableResolutions.push_back({2560, 1440});
    }
    if (monWidth >= 3840 && monHeight >= 2160) {
        data.availableResolutions.push_back({3840, 2160});
    }

    if (data.availableResolutions.empty()) {
        data.availableResolutions.push_back({1920, 1080});
    }

    if (data.selectedResolutionIndex < 0 ||
        data.selectedResolutionIndex >= static_cast<int>(data.availableResolutions.size())) {
        data.selectedResolutionIndex = 0;
    }
}

void InitSettings(SettingsData& data, const std::string &filename) {
    data.filename = filename;
    data.monitor = 0;

    std::ifstream file(filename);
    if (file) {
        nlohmann::json j;
        file >> j;

        data.selectedResolutionIndex = j.value("resolutionIndex", 0);

        const int savedDisplayMode = j.value("displayMode", 0);
        if (savedDisplayMode == 1 || savedDisplayMode == 2) {
            data.displayMode = DisplayMode::Borderless;
        } else {
            data.displayMode = DisplayMode::Windowed;
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

    data.originalResolutionIndex = data.selectedResolutionIndex;
    data.originalDisplayMode = data.displayMode;
}

