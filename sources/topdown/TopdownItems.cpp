#include "topdown/TopdownItems.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

#include "raylib.h"
#include "audio/Audio.h"
#include "resources/TextureAsset.h"
#include "topdown/PlayerRegistry.h"
#include "topdown/TopdownHelpers.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

static std::string NormalizeItemPath(const fs::path& path)
{
    return path.lexically_normal().string();
}

static bool StripAssetsPathPrefix(const fs::path& path, fs::path& outRelativeToAssets)
{
    auto it = path.begin();
    if (it == path.end() || it->string() != "assets") {
        return false;
    }

    ++it;
    outRelativeToAssets.clear();
    for (; it != path.end(); ++it) {
        outRelativeToAssets /= *it;
    }

    return true;
}

static fs::path ResolveItemTexturePath(const fs::path& jsonDir, const std::string& texturePath)
{
    fs::path path(texturePath);
    if (path.is_absolute()) {
        return path.lexically_normal();
    }

    fs::path relativeToAssets;
    if (StripAssetsPathPrefix(path, relativeToAssets)) {
        return (fs::path(ASSETS_PATH) / relativeToAssets).lexically_normal();
    }

    return (jsonDir / path).lexically_normal();
}

static bool ParseTopdownItemKind(const std::string& text, TopdownItemKind& outKind)
{
    if (text == "ammo") {
        outKind = TopdownItemKind::Ammo;
        return true;
    }

    if (text == "health") {
        outKind = TopdownItemKind::Health;
        return true;
    }

    outKind = TopdownItemKind::Unknown;
    return false;
}

static const json* FindItemObjectProperty(const json& objectJson, const char* name)
{
    auto it = objectJson.find("properties");
    if (it == objectJson.end() || !it->is_array()) {
        return nullptr;
    }

    for (const auto& prop : *it) {
        if (prop.is_object() && prop.value("name", std::string()) == name) {
            return &prop;
        }
    }

    return nullptr;
}

static std::string GetItemObjectPropertyString(
        const json& objectJson,
        const char* name,
        const std::string& defaultValue = std::string())
{
    const json* prop = FindItemObjectProperty(objectJson, name);
    if (prop == nullptr) {
        return defaultValue;
    }

    return prop->value("value", defaultValue);
}

bool LoadTopdownItemDefinitions(GameState& state)
{
    TopdownItemRegistry& registry = state.topdown.itemRegistry;
    if (registry.loaded) {
        return !registry.definitions.empty();
    }

    registry.definitions.clear();

    const fs::path jsonPath = fs::path(ASSETS_PATH "items/items.json").lexically_normal();
    json root;
    {
        std::ifstream in(jsonPath);
        if (!in.is_open()) {
            TraceLog(LOG_ERROR,
                     "Failed opening topdown item definition registry: %s",
                     jsonPath.string().c_str());
            return false;
        }

        try {
            in >> root;
        } catch (const std::exception& e) {
            TraceLog(LOG_ERROR,
                     "Failed parsing topdown item definition registry '%s': %s",
                     jsonPath.string().c_str(),
                     e.what());
            return false;
        }
    }

    if (!root.is_object() || !root.contains("items") || !root["items"].is_array()) {
        TraceLog(LOG_ERROR,
                 "Topdown item definition registry missing 'items' array: %s",
                 jsonPath.string().c_str());
        return false;
    }

    std::unordered_set<std::string> seenIds;
    const fs::path jsonDir = jsonPath.parent_path();

    for (const auto& entry : root["items"]) {
        if (!entry.is_object()) {
            continue;
        }

        TopdownItemDefinition def;
        def.id = entry.value("id", std::string());
        if (def.id.empty()) {
            TraceLog(LOG_WARNING, "Skipping topdown item definition with missing id");
            continue;
        }

        if (seenIds.find(def.id) != seenIds.end()) {
            TraceLog(LOG_WARNING,
                     "Skipping duplicate topdown item definition id '%s'",
                     def.id.c_str());
            continue;
        }
        seenIds.insert(def.id);

        const std::string kindText = entry.value("kind", std::string());
        if (!ParseTopdownItemKind(kindText, def.kind)) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown item definition '%s' with unknown kind '%s'",
                     def.id.c_str(),
                     kindText.c_str());
            continue;
        }

        const std::string texturePath = entry.value("texture", std::string());
        if (texturePath.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown item definition '%s' with missing texture",
                     def.id.c_str());
            continue;
        }

        def.displayName = entry.value("displayName", def.id);
        def.texturePath = NormalizeItemPath(ResolveItemTexturePath(jsonDir, texturePath));

        if (def.kind == TopdownItemKind::Ammo) {
            def.ammoType = entry.value("ammoType", std::string());
            if (def.ammoType.empty()) {
                TraceLog(LOG_WARNING,
                         "Skipping ammo item definition '%s' with missing ammoType",
                         def.id.c_str());
                continue;
            }
            def.amount = std::max(0, entry.value("amount", 0));
        } else if (def.kind == TopdownItemKind::Health) {
            def.healAmount = std::max(0.0f, entry.value("healAmount", 0.0f));
        }

        TextureLoadSettings settings{};
        settings.premultiplyAlpha = true;
        settings.filter = TextureFilterMode::Point;
        settings.wrap = TextureWrapMode::Clamp;

        def.textureHandle = LoadTextureAsset(
                state.resources,
                def.texturePath.c_str(),
                settings,
                ResourceScope::Global);

        if (def.textureHandle < 0) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown item definition '%s'; failed loading texture '%s'",
                     def.id.c_str(),
                     def.texturePath.c_str());
            continue;
        }

        registry.definitions.push_back(def);
    }

    registry.loaded = !registry.definitions.empty();
    if (!registry.loaded) {
        TraceLog(LOG_ERROR,
                 "Topdown item definition registry has no usable definitions: %s",
                 jsonPath.string().c_str());
        return false;
    }

    TraceLog(LOG_INFO,
             "Loaded topdown item definitions: %d",
             static_cast<int>(registry.definitions.size()));
    return true;
}

const TopdownItemDefinition* FindTopdownItemDefinition(
        const GameState& state,
        const std::string& id)
{
    for (const TopdownItemDefinition& def : state.topdown.itemRegistry.definitions) {
        if (def.id == id) {
            return &def;
        }
    }

    return nullptr;
}

void ImportTopdownItemLayer(
        GameState& state,
        const json& layer,
        int baseAssetScale)
{
    if (!layer.contains("objects") || !layer["objects"].is_array()) {
        return;
    }

    const float scale = static_cast<float>(baseAssetScale);
    const float layerOffX = layer.value("offsetx", 0.0f);
    const float layerOffY = layer.value("offsety", 0.0f);

    int objectIndex = 0;
    for (const auto& obj : layer["objects"]) {
        if (!obj.is_object()) {
            ++objectIndex;
            continue;
        }

        TopdownAuthoredItem item;
        item.tiledObjectId = obj.value("id", -1);
        item.id = obj.value("name", std::string());
        if (item.id.empty()) {
            item.id = GetItemObjectPropertyString(obj, "id", "");
        }
        if (item.id.empty()) {
            item.id = "item_" + std::to_string(objectIndex);
            TraceLog(LOG_WARNING,
                     "Topdown item object %d missing name/id; using fallback id '%s'",
                     item.tiledObjectId,
                     item.id.c_str());
        }

        item.itemId = GetItemObjectPropertyString(obj, "itemId", "");
        if (item.itemId.empty()) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown item instance '%s' with missing itemId",
                     item.id.c_str());
            ++objectIndex;
            continue;
        }

        if (FindTopdownItemDefinition(state, item.itemId) == nullptr) {
            TraceLog(LOG_WARNING,
                     "Skipping topdown item instance '%s' with unknown itemId '%s'",
                     item.id.c_str(),
                     item.itemId.c_str());
            ++objectIndex;
            continue;
        }

        item.position.x = (obj.value("x", 0.0f) + layerOffX) * scale;
        item.position.y = (obj.value("y", 0.0f) + layerOffY) * scale;
        item.visible = obj.value("visible", true);

        state.topdown.authored.items.push_back(item);
        ++objectIndex;
    }
}

void BuildTopdownRuntimeItemsFromAuthored(TopdownData& topdown)
{
    topdown.runtime.items.clear();
    topdown.runtime.items.reserve(topdown.authored.items.size());

    for (int i = 0; i < static_cast<int>(topdown.authored.items.size()); ++i) {
        const TopdownAuthoredItem& authored = topdown.authored.items[i];

        TopdownRuntimeItem runtime;
        runtime.authoredIndex = i;
        runtime.tiledObjectId = authored.tiledObjectId;
        runtime.id = authored.id;
        runtime.itemId = authored.itemId;
        runtime.position = authored.position;
        runtime.active = true;
        runtime.visible = authored.visible;

        topdown.runtime.items.push_back(runtime);
    }
}


static bool TryPickupAmmoItem(
        GameState& state,
        TopdownRuntimeItem& item,
        const TopdownItemDefinition& def)
{
    if (def.ammoType.empty() || def.amount <= 0) {
        return false;
    }

    if (!TopdownPlayerAddAmmo(state, def.ammoType, def.amount)) {
        return false;
    }

    item.active = false;
    PlaySoundById(state, "item_added");
    TraceLog(LOG_INFO,
             "Picked up ammo item '%s': +%d %s reserve ammo",
             item.id.c_str(),
             def.amount,
             def.ammoType.c_str());
    return true;
}

static bool TryPickupHealthItem(
        GameState& state,
        TopdownRuntimeItem& item,
        const TopdownItemDefinition& def)
{
    TopdownPlayerInventoryRuntime& inventory = state.topdown.runtime.playerInventory;
    const int maxCarried = std::max(0, inventory.maxCarriedHealthItems);
    if (inventory.carriedHealthItems >= maxCarried) {
        return false;
    }

    inventory.carriedHealthItems += 1;
    inventory.carriedHealthHealAmount = std::max(
            inventory.carriedHealthHealAmount,
            def.healAmount);

    item.active = false;
    PlaySoundById(state, "item_added");
    TraceLog(LOG_INFO,
             "Picked up health item '%s': carried %d/%d (heal %.1f)",
             item.id.c_str(),
             inventory.carriedHealthItems,
             maxCarried,
             def.healAmount);
    return true;
}

void TopdownUpdateItems(GameState& state, float dt)
{
    (void)dt;

    if (state.topdown.runtime.player.lifeState != TopdownPlayerLifeState::Alive) {
        return;
    }

    static constexpr float kPickupRadius = 42.0f;
    static constexpr float kPickupRadiusSqr = kPickupRadius * kPickupRadius;
    const Vector2 playerPosition = state.topdown.runtime.player.position;

    for (TopdownRuntimeItem& item : state.topdown.runtime.items) {
        if (!item.active || !item.visible) {
            continue;
        }

        const Vector2 toItem = TopdownSub(item.position, playerPosition);
        if (TopdownLengthSqr(toItem) > kPickupRadiusSqr) {
            continue;
        }

        const TopdownItemDefinition* def = FindTopdownItemDefinition(state, item.itemId);
        if (def == nullptr) {
            continue;
        }

        switch (def->kind) {
            case TopdownItemKind::Ammo:
                TryPickupAmmoItem(state, item, *def);
                break;
            case TopdownItemKind::Health:
                TryPickupHealthItem(state, item, *def);
                break;
            case TopdownItemKind::Unknown:
                break;
        }
    }
}

void TopdownRenderItems(GameState& state)
{
    const float scale = static_cast<float>(state.topdown.currentLevelBaseAssetScale);
    const float timeMs = state.topdown.runtime.timeMs;

    for (int i = 0; i < static_cast<int>(state.topdown.runtime.items.size()); ++i) {
        const TopdownRuntimeItem& item = state.topdown.runtime.items[i];
        if (!item.active || !item.visible) {
            continue;
        }

        const TopdownItemDefinition* def = FindTopdownItemDefinition(state, item.itemId);
        if (def == nullptr) {
            continue;
        }

        const TextureResource* tex = FindTextureResource(state.resources, def->textureHandle);
        if (tex == nullptr || !tex->loaded || tex->texture.id == 0) {
            continue;
        }

        const float dstW = static_cast<float>(tex->texture.width) * scale;
        const float dstH = static_cast<float>(tex->texture.height) * scale;
        const Rectangle worldRect{
                item.position.x - dstW * 0.5f,
                item.position.y - dstH * 0.5f - 8.0f,
                dstW,
                dstH + 16.0f
        };
        if (!TopdownWorldRectOverlapsCameraView(state, worldRect, 192.0f)) {
            continue;
        }

        const Vector2 screen = TopdownWorldToScreen(state, item.position);

        const float phase = timeMs * 0.004f + static_cast<float>(i) * 0.7f;
        const float bobSin = std::sin(phase);
        const float bob = bobSin * 4.0f;

        // bobSin is -1 at the bottom of the bob and +1 at the top.
        // Convert that into 0..1 height, then shrink/dim the shadow as the item rises.
        const float bobHeight01 = (-bobSin + 1.0f) * 0.5f;
        const float shadowScale = 1.0f - bobHeight01 * 0.28f;
        const unsigned char shadowAlpha =
                static_cast<unsigned char>(std::round(76.0f - bobHeight01 * 22.0f));

        const float shadowRadiusX = std::max(7.0f, dstW * 0.52f) * shadowScale;
        const float shadowRadiusY = std::max(2.0f, dstH * 0.18f) * shadowScale;

        DrawEllipse(
                static_cast<int>(std::round(screen.x)),
                static_cast<int>(std::round(screen.y + dstH * 0.55f)),
                shadowRadiusX,
                shadowRadiusY,
                Color{0, 0, 0, shadowAlpha});

        Rectangle src{
                0.0f,
                0.0f,
                static_cast<float>(tex->texture.width),
                static_cast<float>(tex->texture.height)
        };

        const Vector2 origin{ dstW * 0.5f, dstH * 0.5f };
        const float left = std::round(screen.x - origin.x);
        const float top = std::round(screen.y + bob - origin.y);
        const Rectangle dst{
                left + origin.x,
                top + origin.y,
                dstW,
                dstH
        };

        DrawTexturePro(tex->texture, src, dst, origin, 0.0f, WHITE);
    }
}

