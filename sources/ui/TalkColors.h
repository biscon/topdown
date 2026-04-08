#pragma once

#include <string>
#include "raylib.h"

struct TalkColorEntry {
    const char* name;
    Color color;
};

int GetTalkColorEntryCount();
const TalkColorEntry& GetTalkColorEntry(int index);
bool TryGetTalkColorByName(const std::string& name, Color& outColor);
