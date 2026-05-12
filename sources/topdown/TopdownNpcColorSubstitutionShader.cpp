#include "topdown/TopdownNpcColorSubstitutionShader.h"

#include "raylib.h"

#include <cstdio>

namespace
{
    struct TopdownNpcColorSubstitutionShaderState {
        bool loaded = false;
        Shader shader{};

        int skinSrcLoc = -1;
        int hairSrcLoc = -1;
        int chestSrcLoc = -1;
        int legsSrcLoc = -1;

        int skinDstLoc = -1;
        int hairDstLoc = -1;
        int chestDstLoc = -1;
        int legsDstLoc = -1;

        int toleranceLoc = -1;
    };

    static TopdownNpcColorSubstitutionShaderState gNpcColorSubstitutionShader;

    static Vector3 ColorToVector3(Color color)
    {
        return Vector3{
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f
        };
    }

    static void FillColorVectors(const TopdownNpcColorSet3& colors, Vector3* outColors)
    {
        for (int i = 0; i < 3; ++i) {
            outColors[i] = ColorToVector3(colors.colors[i]);
        }
    }

    static void FillColorVectors(const TopdownNpcColorSet4& colors, Vector3* outColors)
    {
        for (int i = 0; i < 4; ++i) {
            outColors[i] = ColorToVector3(colors.colors[i]);
        }
    }

    static void FillColorVectors(const TopdownNpcColorSet5& colors, Vector3* outColors)
    {
        for (int i = 0; i < 5; ++i) {
            outColors[i] = ColorToVector3(colors.colors[i]);
        }
    }

    static int GetArrayLocation(const Shader& shader, const char* name)
    {
        int loc = GetShaderLocation(shader, name);
        if (loc >= 0) {
            return loc;
        }

        char firstElementName[64]{};
        snprintf(firstElementName, sizeof(firstElementName), "%s[0]", name);
        return GetShaderLocation(shader, firstElementName);
    }

    static void SetShaderVector3Array(Shader& shader, int loc, const Vector3* values, int count)
    {
        if (loc >= 0) {
            SetShaderValueV(shader, loc, values, SHADER_UNIFORM_VEC3, count);
        }
    }
}

bool InitTopdownNpcColorSubstitutionShader()
{
    if (gNpcColorSubstitutionShader.loaded) {
        return true;
    }

    gNpcColorSubstitutionShader.shader = LoadShader(
            nullptr,
            ASSETS_PATH "shaders/npc_color_substitution.fs");

    if (gNpcColorSubstitutionShader.shader.id == 0) {
        TraceLog(LOG_ERROR, "Failed to load NPC color substitution shader");
        return false;
    }

    Shader& shader = gNpcColorSubstitutionShader.shader;
    gNpcColorSubstitutionShader.skinSrcLoc = GetArrayLocation(shader, "uSkinSrc");
    gNpcColorSubstitutionShader.hairSrcLoc = GetArrayLocation(shader, "uHairSrc");
    gNpcColorSubstitutionShader.chestSrcLoc = GetArrayLocation(shader, "uChestSrc");
    gNpcColorSubstitutionShader.legsSrcLoc = GetArrayLocation(shader, "uLegsSrc");
    gNpcColorSubstitutionShader.skinDstLoc = GetArrayLocation(shader, "uSkinDst");
    gNpcColorSubstitutionShader.hairDstLoc = GetArrayLocation(shader, "uHairDst");
    gNpcColorSubstitutionShader.chestDstLoc = GetArrayLocation(shader, "uChestDst");
    gNpcColorSubstitutionShader.legsDstLoc = GetArrayLocation(shader, "uLegsDst");
    gNpcColorSubstitutionShader.toleranceLoc = GetShaderLocation(shader, "uTolerance");

    if (gNpcColorSubstitutionShader.skinSrcLoc < 0 ||
        gNpcColorSubstitutionShader.hairSrcLoc < 0 ||
        gNpcColorSubstitutionShader.chestSrcLoc < 0 ||
        gNpcColorSubstitutionShader.legsSrcLoc < 0 ||
        gNpcColorSubstitutionShader.skinDstLoc < 0 ||
        gNpcColorSubstitutionShader.hairDstLoc < 0 ||
        gNpcColorSubstitutionShader.chestDstLoc < 0 ||
        gNpcColorSubstitutionShader.legsDstLoc < 0 ||
        gNpcColorSubstitutionShader.toleranceLoc < 0) {
        TraceLog(LOG_ERROR, "NPC color substitution shader is missing required uniforms");
        UnloadShader(gNpcColorSubstitutionShader.shader);
        gNpcColorSubstitutionShader = {};
        return false;
    }

    gNpcColorSubstitutionShader.loaded = true;
    TraceLog(LOG_INFO, "Loaded NPC color substitution shader");
    return true;
}

void ShutdownTopdownNpcColorSubstitutionShader()
{
    if (gNpcColorSubstitutionShader.loaded) {
        UnloadShader(gNpcColorSubstitutionShader.shader);
    }

    gNpcColorSubstitutionShader = {};
}

bool BeginTopdownNpcColorSubstitutionShader(
        const TopdownNpcColorSubstitutionDefinition& source,
        const TopdownNpcResolvedColorSubstitution& resolved)
{
    if (!gNpcColorSubstitutionShader.loaded || !source.active || !resolved.active) {
        return false;
    }

    Vector3 skinSrc[3]{};
    Vector3 hairSrc[3]{};
    Vector3 chestSrc[5]{};
    Vector3 legsSrc[4]{};
    Vector3 skinDst[3]{};
    Vector3 hairDst[3]{};
    Vector3 chestDst[5]{};
    Vector3 legsDst[4]{};

    FillColorVectors(source.sourceSkin, skinSrc);
    FillColorVectors(source.sourceHair, hairSrc);
    FillColorVectors(source.sourceChest, chestSrc);
    FillColorVectors(source.sourceLegs, legsSrc);
    FillColorVectors(resolved.dstSkin, skinDst);
    FillColorVectors(resolved.dstHair, hairDst);
    FillColorVectors(resolved.dstChest, chestDst);
    FillColorVectors(resolved.dstLegs, legsDst);

    Shader& shader = gNpcColorSubstitutionShader.shader;
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.skinSrcLoc, skinSrc, 3);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.hairSrcLoc, hairSrc, 3);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.chestSrcLoc, chestSrc, 5);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.legsSrcLoc, legsSrc, 4);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.skinDstLoc, skinDst, 3);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.hairDstLoc, hairDst, 3);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.chestDstLoc, chestDst, 5);
    SetShaderVector3Array(shader, gNpcColorSubstitutionShader.legsDstLoc, legsDst, 4);

    const float tolerance = 0.01f;
    if (gNpcColorSubstitutionShader.toleranceLoc >= 0) {
        SetShaderValue(shader, gNpcColorSubstitutionShader.toleranceLoc, &tolerance, SHADER_UNIFORM_FLOAT);
    }

    BeginShaderMode(shader);
    return true;
}

void EndTopdownNpcColorSubstitutionShader()
{
    EndShaderMode();
}
