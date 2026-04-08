#include "TalkColors.h"

#include <cctype>

static const TalkColorEntry gTalkColors[] = {
        { "WHITE",   Color{255, 255, 255, 255} },
        { "CREAM",   Color{255, 244, 214, 255} },
        { "YELLOW",  Color{255, 230, 120, 255} },
        { "ORANGE",  Color{255, 170,  90, 255} },
        { "RED",     Color{255, 110, 110, 255} },
        { "PINK",    Color{255, 170, 210, 255} },
        { "MAGENTA", Color{230, 120, 255, 255} },
        { "PURPLE",  Color{180, 140, 255, 255} },
        { "BLUE",    Color{120, 180, 255, 255} },
        { "CYAN",    Color{120, 240, 255, 255} },
        { "GREEN",   Color{140, 255, 160, 255} },
        { "LIME",    Color{210, 255, 120, 255} }
};

static std::string ToUpperAscii(const std::string& s)
{
    std::string out = s;
    for (char& c : out) {
        c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    }
    return out;
}

int GetTalkColorEntryCount()
{
    return static_cast<int>(sizeof(gTalkColors) / sizeof(gTalkColors[0]));
}

const TalkColorEntry& GetTalkColorEntry(int index)
{
    return gTalkColors[index];
}

bool TryGetTalkColorByName(const std::string& name, Color& outColor)
{
    const std::string upper = ToUpperAscii(name);

    for (int i = 0; i < GetTalkColorEntryCount(); ++i) {
        if (upper == gTalkColors[i].name) {
            outColor = gTalkColors[i].color;
            return true;
        }
    }

    return false;
}
