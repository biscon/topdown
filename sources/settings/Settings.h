//
// Created by Stinus Troels Petersen on 14/06/2025.
//

#ifndef SANDBOX_SETTINGS_H
#define SANDBOX_SETTINGS_H

#include <string>
#include "SettingsData.h"

void ApplySettings(SettingsData& settings);
void SaveSettings(const SettingsData& settings);
void InitSettings(SettingsData& data, const std::string &filename);
void RefreshResolutions(SettingsData& data);

#endif //SANDBOX_SETTINGS_H
