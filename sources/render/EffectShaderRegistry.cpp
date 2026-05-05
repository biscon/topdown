#include "render/EffectShaderRegistry.h"

#include <string>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <filesystem>
#include <cstring>

namespace fs = std::filesystem;

namespace
{
    static EffectShaderEntry gEffectShaders[] = {
            { EffectShaderType::UvScroll,    EffectShaderCategory::SelfTexture },
            { EffectShaderType::HeatShimmer, EffectShaderCategory::SceneSample },
            { EffectShaderType::RegionGrade, EffectShaderCategory::SceneSample },
            { EffectShaderType::WaterRipple, EffectShaderCategory::SceneSample },
            { EffectShaderType::WindSway,    EffectShaderCategory::SelfTexture },
            { EffectShaderType::PolyClip,    EffectShaderCategory::SelfTexture },
    };

    static constexpr int gEffectShaderCount =
            static_cast<int>(sizeof(gEffectShaders) / sizeof(gEffectShaders[0]));

    static const char* GetShaderFragmentPath(EffectShaderType type)
    {
        switch (type) {
            case EffectShaderType::UvScroll:
                return ASSETS_PATH "shaders/selftexture/uv_scroll.fs";

            case EffectShaderType::HeatShimmer:
                return ASSETS_PATH "shaders/scenesample/heat_shimmer.fs";

            case EffectShaderType::RegionGrade:
                return ASSETS_PATH "shaders/scenesample/region_grade.fs";

            case EffectShaderType::WaterRipple:
                return ASSETS_PATH "shaders/scenesample/water_ripple.fs";

            case EffectShaderType::WindSway:
                return ASSETS_PATH "shaders/selftexture/wind_sway.fs";

            case EffectShaderType::PolyClip:
                return ASSETS_PATH "shaders/selftexture/poly_clip.fs";

            case EffectShaderType::None:
            default:
                return nullptr;
        }
    }

    static bool ReadTextFile(const fs::path& path, std::string& outText)
    {
        outText.clear();

        std::ifstream in(path);
        if (!in.is_open()) {
            return false;
        }

        std::ostringstream ss;
        ss << in.rdbuf();
        outText = ss.str();
        return true;
    }

    static std::string TrimWhitespace(const std::string& text)
    {
        const size_t first = text.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) {
            return "";
        }

        const size_t last = text.find_last_not_of(" \t\r\n");
        return text.substr(first, last - first + 1);
    }

    static bool TryParseIncludeLine(const std::string& line, std::string& outIncludePath)
    {
        outIncludePath.clear();

        const std::string trimmed = TrimWhitespace(line);
        static constexpr const char* prefix = "#include \"";

        if (trimmed.rfind(prefix, 0) != 0) {
            return false;
        }

        const size_t start = std::strlen(prefix);
        const size_t end = trimmed.find('"', start);
        if (end == std::string::npos) {
            return false;
        }

        if (end + 1 != trimmed.size()) {
            return false;
        }

        outIncludePath = trimmed.substr(start, end - start);
        return !outIncludePath.empty();
    }

    static bool ExpandShaderIncludesRecursive(
            const fs::path& shaderPath,
            std::unordered_set<std::string>& includeStack,
            std::string& outExpandedSource)
    {
        outExpandedSource.clear();

        const fs::path normalizedPath = shaderPath.lexically_normal();
        const std::string normalizedKey = normalizedPath.string();

        if (includeStack.find(normalizedKey) != includeStack.end()) {
            TraceLog(LOG_ERROR, "Circular shader include detected: %s", normalizedKey.c_str());
            return false;
        }

        std::string source;
        if (!ReadTextFile(normalizedPath, source)) {
            TraceLog(LOG_ERROR, "Failed to read shader file: %s", normalizedKey.c_str());
            return false;
        }

        includeStack.insert(normalizedKey);

        std::istringstream in(source);
        std::ostringstream out;
        std::string line;
        int lineNumber = 0;

        while (std::getline(in, line)) {
            ++lineNumber;

            std::string includeRelPath;
            if (!TryParseIncludeLine(line, includeRelPath)) {
                out << line << '\n';
                continue;
            }

            const fs::path includePath =
                    (normalizedPath.parent_path() / includeRelPath).lexically_normal();

            std::string includedSource;
            if (!ExpandShaderIncludesRecursive(includePath, includeStack, includedSource)) {
                TraceLog(LOG_ERROR,
                         "Failed expanding include '%s' referenced from '%s' line %d",
                         includeRelPath.c_str(),
                         normalizedKey.c_str(),
                         lineNumber);
                includeStack.erase(normalizedKey);
                return false;
            }

            out << "// begin include: " << includePath.string() << '\n';
            out << includedSource;
            out << "// end include: " << includePath.string() << '\n';
        }

        includeStack.erase(normalizedKey);
        outExpandedSource = out.str();
        return true;
    }

    static bool LoadFragmentShaderWithIncludes(const char* fragmentPath, Shader& outShader)
    {
        outShader = {};

        if (fragmentPath == nullptr || fragmentPath[0] == '\0') {
            return false;
        }

        std::unordered_set<std::string> includeStack;
        std::string expandedSource;
        if (!ExpandShaderIncludesRecursive(fs::path(fragmentPath), includeStack, expandedSource)) {
            return false;
        }

        outShader = LoadShaderFromMemory(nullptr, expandedSource.c_str());
        if (outShader.id == 0) {
            TraceLog(LOG_ERROR, "Failed to compile expanded shader: %s", fragmentPath);
            return false;
        }

        return true;
    }

    static void CacheUniformLocations(EffectShaderEntry& entry)
    {
        entry.timeLoc = GetShaderLocation(entry.shader, "uTime");
        entry.scrollSpeedLoc = GetShaderLocation(entry.shader, "uScrollSpeed");
        entry.uvScaleLoc = GetShaderLocation(entry.shader, "uUvScale");
        entry.distortionAmountLoc = GetShaderLocation(entry.shader, "uDistortionAmount");
        entry.noiseScrollSpeedLoc = GetShaderLocation(entry.shader, "uNoiseScrollSpeed");
        entry.intensityLoc = GetShaderLocation(entry.shader, "uIntensity");
        entry.phaseOffsetLoc = GetShaderLocation(entry.shader, "uPhaseOffset");
        entry.sceneSizeLoc = GetShaderLocation(entry.shader, "uSceneSize");
        entry.regionPosLoc = GetShaderLocation(entry.shader, "uRegionPos");
        entry.regionSizeLoc = GetShaderLocation(entry.shader, "uRegionSize");
        entry.brightnessLoc = GetShaderLocation(entry.shader, "uBrightness");
        entry.contrastLoc = GetShaderLocation(entry.shader, "uContrast");
        entry.saturationLoc = GetShaderLocation(entry.shader, "uSaturation");
        entry.tintLoc = GetShaderLocation(entry.shader, "uTint");
        entry.softnessLoc = GetShaderLocation(entry.shader, "uSoftness");
        entry.usePolygonLoc = GetShaderLocation(entry.shader, "uUsePolygon");
        entry.polygonVertexCountLoc = GetShaderLocation(entry.shader, "uPolygonVertexCount");
        entry.polygonPointsLoc = GetShaderLocation(entry.shader, "uPolygonPoints");
    }
}

EffectShaderCategory GetEffectShaderCategory(EffectShaderType type)
{
    switch (type) {
        case EffectShaderType::UvScroll:
        case EffectShaderType::WindSway:
        case EffectShaderType::PolyClip:
            return EffectShaderCategory::SelfTexture;

        case EffectShaderType::HeatShimmer:
        case EffectShaderType::RegionGrade:
        case EffectShaderType::WaterRipple:
            return EffectShaderCategory::SceneSample;

        case EffectShaderType::None:
        default:
            return EffectShaderCategory::None;
    }
}

const char* EffectShaderTypeToString(EffectShaderType type)
{
    switch (type) {
        case EffectShaderType::UvScroll:
            return "uv_scroll";

        case EffectShaderType::HeatShimmer:
            return "heat_shimmer";

        case EffectShaderType::RegionGrade:
            return "region_grade";

        case EffectShaderType::WaterRipple:
            return "water_ripple";

        case EffectShaderType::WindSway:
            return "wind_sway";

        case EffectShaderType::PolyClip:
            return "poly_clip";

        case EffectShaderType::None:
        default:
            return "none";
    }
}

bool InitEffectShaderRegistry()
{
    bool allOk = true;

    for (int i = 0; i < gEffectShaderCount; ++i) {
        EffectShaderEntry& entry = gEffectShaders[i];

        const char* fragmentPath = GetShaderFragmentPath(entry.type);
        if (fragmentPath == nullptr) {
            continue;
        }

        if (!LoadFragmentShaderWithIncludes(fragmentPath, entry.shader)) {
            TraceLog(LOG_ERROR,
                     "Failed to load effect shader '%s' from '%s'",
                     EffectShaderTypeToString(entry.type),
                     fragmentPath);
            allOk = false;
            continue;
        }

        entry.loaded = true;
        CacheUniformLocations(entry);

        TraceLog(LOG_INFO,
                 "Loaded effect shader '%s' (%s)",
                 EffectShaderTypeToString(entry.type),
                 fragmentPath);
    }

    return allOk;
}

void ShutdownEffectShaderRegistry()
{
    for (int i = 0; i < gEffectShaderCount; ++i) {
        EffectShaderEntry& entry = gEffectShaders[i];
        if (!entry.loaded) {
            continue;
        }

        UnloadShader(entry.shader);
        entry.shader = {};
        entry.loaded = false;

        entry.timeLoc = -1;
        entry.scrollSpeedLoc = -1;
        entry.uvScaleLoc = -1;
        entry.distortionAmountLoc = -1;
        entry.noiseScrollSpeedLoc = -1;
        entry.intensityLoc = -1;
        entry.phaseOffsetLoc = -1;
        entry.sceneSizeLoc = -1;
        entry.regionPosLoc = -1;
        entry.regionSizeLoc = -1;
        entry.brightnessLoc = -1;
        entry.contrastLoc = -1;
        entry.saturationLoc = -1;
        entry.tintLoc = -1;
        entry.softnessLoc = -1;
        entry.usePolygonLoc = -1;
        entry.polygonVertexCountLoc = -1;
        entry.polygonPointsLoc = -1;
    }
}

const EffectShaderEntry* FindEffectShaderEntry(EffectShaderType type)
{
    if (type == EffectShaderType::None) {
        return nullptr;
    }

    for (int i = 0; i < gEffectShaderCount; ++i) {
        if (gEffectShaders[i].type == type) {
            return gEffectShaders[i].loaded ? &gEffectShaders[i] : nullptr;
        }
    }

    return nullptr;
}
