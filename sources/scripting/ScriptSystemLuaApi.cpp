#include <sstream>
#include "scripting/ScriptSystemInternal.h"
#include "scripting/ScriptSystem.h"
#include "ui/TalkColors.h"
#include "adventure/Inventory.h"
#include "debug/DebugConsole.h"
#include "adventure/Dialogue.h"
#include "raymath.h"
#include "audio/Audio.h"
#include "adventure/AdventureScriptCommands.h"
#include "topdown/LevelLoad.h"
#include "topdown/TopdownScriptCommands.h"

static bool ParseOptionalTalkColorAndDuration(
        lua_State* L,
        int firstOptionalArgIndex,
        Color& outColor,
        bool& outHasColor,
        int& outDurationMs)
{
    outColor = WHITE;
    outHasColor = false;
    outDurationMs = -1;

    const int top = lua_gettop(L);
    if (top < firstOptionalArgIndex) {
        return true;
    }

    const int type = lua_type(L, firstOptionalArgIndex);
    if (type == LUA_TSTRING) {
        const char* colorName = lua_tostring(L, firstOptionalArgIndex);
        if (colorName == nullptr || !TryGetTalkColorByName(std::string(colorName), outColor)) {
            return false;
        }

        outHasColor = true;

        if (top >= firstOptionalArgIndex + 1) {
            outDurationMs = static_cast<int>(luaL_checkinteger(L, firstOptionalArgIndex + 1));
        }

        return true;
    }

    if (type == LUA_TNUMBER) {
        outDurationMs = static_cast<int>(luaL_checkinteger(L, firstOptionalArgIndex));
        return true;
    }

    if (type == LUA_TNIL) {
        return true;
    }

    return false;
}

static bool ParseOptionalStringList(
        lua_State* L,
        int argIndex,
        std::vector<std::string>& outValues)
{
    outValues.clear();

    const int top = lua_gettop(L);
    if (top < argIndex || lua_isnoneornil(L, argIndex)) {
        return true;
    }

    if (!lua_istable(L, argIndex)) {
        return false;
    }

    const lua_Integer len = luaL_len(L, argIndex);
    for (lua_Integer i = 1; i <= len; ++i) {
        lua_geti(L, argIndex, i);

        if (!lua_isstring(L, -1)) {
            lua_pop(L, 1);
            return false;
        }

        const char* value = lua_tostring(L, -1);
        outValues.push_back(value != nullptr ? std::string(value) : std::string());
        lua_pop(L, 1);
    }

    return true;
}


// async helper
static int Lua_WaitContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)status;
    (void)ctx;

    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_DialogueContinuation(lua_State* L, int status, lua_KContext ctx)
{
    (void)status;
    (void)ctx;

    if (lua_gettop(L) >= 1) {
        return 1;
    }

    lua_pushnil(L);
    return 1;
}

int Lua_consolePrint(lua_State* L)
{
    const int argCount = lua_gettop(L);

    std::ostringstream out;
    for (int i = 1; i <= argCount; ++i) {
        if (i > 1) {
            out << "    ";
        }

        const int type = lua_type(L, i);
        switch (type) {
            case LUA_TNIL:
                out << "nil";
                break;

            case LUA_TBOOLEAN:
                out << (lua_toboolean(L, i) ? "true" : "false");
                break;

            case LUA_TNUMBER:
                if (lua_isinteger(L, i)) {
                    out << static_cast<long long>(lua_tointeger(L, i));
                } else {
                    out << lua_tonumber(L, i);
                }
                break;

            case LUA_TSTRING:
            {
                const char* s = lua_tostring(L, i);
                out << (s != nullptr ? s : "");
                break;
            }

            default:
                out << "<" << lua_typename(L, type) << ">";
                break;
        }
    }

    const std::string text = out.str();
    TraceLog(LOG_INFO, "[LUA] %s", text.c_str());
    ScriptSystemConsolePrint(text);
    return 0;
}

static int Lua_setFlag(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const bool value = lua_toboolean(L, 2) != 0;

    gameState->script.flags[std::string(name)] = value;
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_flag(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    bool value = false;
    auto it = gameState->script.flags.find(std::string(name));
    if (it != gameState->script.flags.end()) {
        value = it->second;
    }

    lua_pushboolean(L, value ? 1 : 0);
    return 1;
}

static int Lua_setInt(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const int value = static_cast<int>(luaL_checkinteger(L, 2));

    gameState->script.ints[std::string(name)] = value;
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_getInt(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    int value = -1;
    auto it = gameState->script.ints.find(std::string(name));
    if (it != gameState->script.ints.end()) {
        value = it->second;
    }

    lua_pushinteger(L, value);
    return 1;
}

static int Lua_setString(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const char* value = luaL_checkstring(L, 2);

    gameState->script.strings[std::string(name)] = std::string(value);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_getString(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    std::string value;
    auto it = gameState->script.strings.find(std::string(name));
    if (it != gameState->script.strings.end()) {
        value = it->second;
    }

    lua_pushstring(L, value.c_str());
    return 1;
}

static int Lua_hasItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool hasItem = ControlledActorHasItem(*gameState, std::string(itemId));
    lua_pushboolean(L, hasItem ? 1 : 0);
    return 1;
}

static int Lua_giveItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = GiveItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_removeItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = RemoveItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_actorHasItem(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool hasItem = ActorHasItem(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, hasItem ? 1 : 0);
    return 1;
}

static int Lua_giveItemTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = GiveItemToActor(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_removeItemFrom(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* itemId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = RemoveItemFromActor(*gameState, std::string(actorId), std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_say(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 2, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSay(*gameState, std::string(text), durationMs);
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}


static int Lua_sayProp(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || propId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayProp(
            *gameState,
            std::string(propId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_sayAt(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    const char* text = luaL_checkstring(L, 3);

    Color color = WHITE;
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 4, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayAt(
            *gameState,
            Vector2{x, y},
            std::string(text),
            hasColor ? color : WHITE,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_startSay(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 2, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartSay(*gameState, std::string(text), durationMs);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startSayProp(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || propId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartSayProp(
            *gameState,
            std::string(propId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startSayAt(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    const char* text = luaL_checkstring(L, 3);

    Color color = WHITE;
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 4, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartSayAt(
            *gameState,
            Vector2{x, y},
            std::string(text),
            hasColor ? color : WHITE,
            durationMs);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startSayActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || actorId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartSayActor(
            *gameState,
            std::string(actorId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_walkTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkTo(*gameState, Vector2{x, y});
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_runTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunTo(*gameState, Vector2{x, y});
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkToHotspot(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    if (gameState == nullptr || id == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptWalkToHotspot(*gameState, std::string(id));
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}


static int Lua_delay(lua_State* L)
{
    const float ms = static_cast<float>(luaL_checknumber(L, 1));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::DelayMs, ms)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_changeLevel(lua_State* L)
{
    const char* levelId = luaL_checkstring(L, 1);
    const char* spawnId = luaL_optstring(L, 2, "");

    if (gameState == nullptr || levelId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    gameState->topdown.hasPendingLevelChange = true;
    gameState->topdown.pendingLevelId = levelId;
    gameState->topdown.pendingSpawnId = (spawnId != nullptr) ? spawnId : "";

    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_log(lua_State* L)
{
    const char* text = luaL_checkstring(L, 1);
    const std::string msg = (text != nullptr) ? std::string(text) : std::string();

    TraceLog(LOG_INFO, "[LUA] %s", msg.c_str());
    ScriptSystemConsolePrint(msg);
    return 0;
}

static int Lua_logf(lua_State* L)
{
    const char* fmt = luaL_checkstring(L, 1);
    const int argCount = lua_gettop(L);

    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);

    lua_pushstring(L, fmt);
    for (int i = 2; i <= argCount; ++i) {
        lua_pushvalue(L, i);
    }

    if (lua_pcall(L, argCount, 1, 0) != LUA_OK) {
        const char* err = lua_tostring(L, -1);
        const std::string msg = std::string("[LUA] logf format error: ") +
                                (err != nullptr ? err : "<unknown>");
        TraceLog(LOG_ERROR, "%s", msg.c_str());
        ScriptSystemConsolePrint(msg);
        lua_pop(L, 1);
        return 0;
    }

    const char* text = lua_tostring(L, -1);
    const std::string msg = (text != nullptr) ? std::string(text) : std::string();

    TraceLog(LOG_INFO, "[LUA] %s", msg.c_str());
    ScriptSystemConsolePrint(msg);
    lua_pop(L, 1);
    return 0;
}

static int Lua_playAnimation(lua_State* L)
{
    const char* animationName = luaL_checkstring(L, 1);

    if (gameState == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayAnimation(*gameState, std::string(animationName));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playPropAnimation(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || propId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayPropAnimation(
            *gameState,
            std::string(propId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropAnimation(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || propId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropAnimation(
            *gameState,
            std::string(propId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropPosition(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropPosition(
            *gameState,
            std::string(propId),
            Vector2{x, y});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_movePropTo(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 4));
    const char* interpolation = luaL_checkstring(L, 5);

    if (gameState == nullptr || propId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMovePropTo(
            *gameState,
            std::string(propId),
            Vector2{x, y},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropVisible(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropVisible(
            *gameState,
            std::string(propId),
            visible);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropFlipX(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const bool flipX = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropFlipX(
            *gameState,
            std::string(propId),
            flipX);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setPropPositionRelative(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float dx = static_cast<float>(luaL_checknumber(L, 2));
    const float dy = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || propId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetPropPositionRelative(
            *gameState,
            std::string(propId),
            Vector2{dx, dy});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_movePropBy(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float dx = static_cast<float>(luaL_checknumber(L, 2));
    const float dy = static_cast<float>(luaL_checknumber(L, 3));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 4));
    const char* interpolation = luaL_checkstring(L, 5);

    if (gameState == nullptr || propId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMovePropBy(
            *gameState,
            std::string(propId),
            Vector2{dx, dy},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_sayActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* text = luaL_checkstring(L, 2);

    Color color{};
    bool hasColor = false;
    int durationMs = -1;
    if (!ParseOptionalTalkColorAndDuration(L, 3, color, hasColor, durationMs)) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (gameState == nullptr || actorId == nullptr || text == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSayActor(
            *gameState,
            std::string(actorId),
            std::string(text),
            hasColor ? &color : nullptr,
            durationMs);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::SpeechComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorTo(
            *gameState,
            std::string(actorId),
            Vector2{x, y},
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorToHotspot(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* hotspotId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorToHotspot(
            *gameState,
            std::string(actorId),
            std::string(hotspotId),
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_walkActorToExit(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* exitId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    int actorIndex = -1;
    const bool ok = AdventureScriptWalkActorToExit(
            *gameState,
            std::string(actorId),
            std::string(exitId),
            &actorIndex);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingActorWalkWait(L, actorIndex)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}


static int Lua_startWalkTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkTo(*gameState, Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startRunTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunTo(*gameState, Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkToHotspot(lua_State* L)
{
    const char* hotspotId = luaL_checkstring(L, 1);

    if (gameState == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkToHotspot(*gameState, std::string(hotspotId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkToExit(lua_State* L)
{
    const char* exitId = luaL_checkstring(L, 1);

    if (gameState == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkToExit(*gameState, std::string(exitId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorTo(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorTo(
            *gameState,
            std::string(actorId),
            Vector2{x, y});

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorToHotspot(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* hotspotId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || hotspotId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorToHotspot(
            *gameState,
            std::string(actorId),
            std::string(hotspotId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkActorToExit(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const char* exitId = luaL_checkstring(L, 2);

    if (gameState == nullptr || actorId == nullptr || exitId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStartWalkActorToExit(
            *gameState,
            std::string(actorId),
            std::string(exitId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_disableControls(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetControlsEnabled(*gameState, false);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_enableControls(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetControlsEnabled(*gameState, true);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startScript(lua_State* L)
{
    const char* functionName = luaL_checkstring(L, 1);

    if (gameState == nullptr || functionName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = ScriptSystemStartFunction(gameState->script, std::string(functionName));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopScript(lua_State* L)
{
    const char* functionName = luaL_checkstring(L, 1);

    if (gameState == nullptr || functionName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ScriptSystemStopFunction(gameState->script, std::string(functionName));
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_stopAllScripts(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ScriptSystemStopAll(gameState->script);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_clearHeldItem(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    ClearControlledActorHeldItem(*gameState);
    lua_pushboolean(L, 1);
    return 1;
}

static int Lua_setHeldItem(lua_State* L)
{
    const char* itemId = luaL_checkstring(L, 1);

    if (gameState == nullptr || itemId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = SetControlledActorHeldItem(*gameState, std::string(itemId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_dialogue(lua_State* L)
{
    const char* choiceSetId = luaL_checkstring(L, 1);

    std::vector<std::string> hiddenOptionIds;
    if (!ParseOptionalStringList(L, 2, hiddenOptionIds)) {
        lua_pushnil(L);
        return 1;
    }

    if (gameState == nullptr || choiceSetId == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    const std::vector<std::string>* hiddenPtr =
            hiddenOptionIds.empty() ? nullptr : &hiddenOptionIds;

    const bool ok = StartDialogueChoice(*gameState, std::string(choiceSetId), hiddenPtr);
    if (!ok) {
        lua_pushnil(L);
        return 1;
    }

    if (!StartPendingDialogueWait(L)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_DialogueContinuation);
}

static int Lua_playSound(lua_State* L)
{
    const char* audioId = luaL_checkstring(L, 1);

    if (gameState == nullptr || audioId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlaySound(*gameState, std::string(audioId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopSound(lua_State* L)
{
    const char* audioId = luaL_checkstring(L, 1);

    if (gameState == nullptr || audioId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStopSound(*gameState, std::string(audioId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playMusic(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    float fadeMs = 0.0f;
    if (lua_gettop(L) >= 2 && lua_isnumber(L, 2)) {
        fadeMs = (float)lua_tonumber(L, 2);
    }

    if (gameState == nullptr || id == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayMusic(*gameState, std::string(id), fadeMs);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopMusic(lua_State* L)
{
    float fadeMs = 0.0f;
    if (lua_gettop(L) >= 1 && lua_isnumber(L, 1)) {
        fadeMs = (float)lua_tonumber(L, 1);
    }

    if (gameState == nullptr) {
        return 0;
    }
    AdventureScriptStopMusic(*gameState, fadeMs);
    return 0;
}

static int Lua_setSoundEmitterEnabled(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);
    const bool enabled = lua_toboolean(L, 2) != 0;

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetSoundEmitterEnabled(
            *gameState,
            std::string(emitterId),
            enabled);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_soundEmitterEnabled(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool enabled = false;
    const bool ok = AdventureScriptGetSoundEmitterEnabled(
            *gameState,
            std::string(emitterId),
            enabled);

    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, enabled ? 1 : 0);
    return 1;
}

static int Lua_setSoundEmitterVolume(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);
    const float volume = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetSoundEmitterVolume(
            *gameState,
            std::string(emitterId),
            volume);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playEmitter(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPlayEmitter(*gameState, std::string(emitterId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_stopEmitter(lua_State* L)
{
    const char* emitterId = luaL_checkstring(L, 1);

    if (gameState == nullptr || emitterId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptStopEmitter(*gameState, std::string(emitterId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}


static int Lua_setLayerVisible(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (!gameState || !name) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetImageLayerVisible(*gameState, name, visible);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_layerVisible(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    if (!gameState || !name) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool visible = false;
    const bool ok = TopdownScriptIsImageLayerVisible(*gameState, name, visible);

    lua_pushboolean(L, ok ? (visible ? 1 : 0) : 0);
    return 1;
}

static int Lua_setLayerOpacity(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);
    const float opacity = (float)luaL_checknumber(L, 2);

    if (!gameState || !name) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetImageLayerOpacity(*gameState, name, opacity);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_layerOpacity(lua_State* L)
{
    const char* name = luaL_checkstring(L, 1);

    if (!gameState || !name) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    float opacity = 0.0f;
    const bool ok = TopdownScriptGetImageLayerOpacity(*gameState, name, opacity);

    lua_pushnumber(L, ok ? opacity : 0.0f);
    return 1;
}

static int Lua_cameraFollow(lua_State* L)
{
    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptCameraFollowControlledActor(*gameState);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_cameraFollowActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);

    if (gameState == nullptr || actorId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptCameraFollowActor(
            *gameState,
            std::string(actorId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setCameraPosition(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptSetCameraPosition(*gameState, Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_moveCameraTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 3));
    const char* interpolation = luaL_checkstring(L, 4);

    if (gameState == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMoveCameraTo(
            *gameState,
            Vector2{x, y},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_centerCameraOn(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptCenterCameraOn(*gameState, Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_moveCameraCenterTo(lua_State* L)
{
    const float x = static_cast<float>(luaL_checknumber(L, 1));
    const float y = static_cast<float>(luaL_checknumber(L, 2));
    const float durationMs = static_cast<float>(luaL_checknumber(L, 3));
    const char* interpolation = luaL_optstring(L, 4, "accelerateDecelerate");

    if (gameState == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptMoveCameraCenterTo(
            *gameState,
            Vector2{x, y},
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_panCameraToActor(lua_State* L)
{
    const char* actorId = luaL_checkstring(L, 1);
    const float durationMs = static_cast<float>(luaL_checknumber(L, 2));
    const char* interpolation = luaL_optstring(L, 3, "accelerateDecelerate");

    if (gameState == nullptr || actorId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPanCameraToActor(
            *gameState,
            std::string(actorId),
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_panCameraToProp(lua_State* L)
{
    const char* propId = luaL_checkstring(L, 1);
    const float durationMs = static_cast<float>(luaL_checknumber(L, 2));
    const char* interpolation = luaL_optstring(L, 3, "accelerateDecelerate");

    if (gameState == nullptr || propId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPanCameraToProp(
            *gameState,
            std::string(propId),
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_panCameraToHotspot(lua_State* L)
{
    const char* hotspotId = luaL_checkstring(L, 1);
    const float durationMs = static_cast<float>(luaL_checknumber(L, 2));
    const char* interpolation = luaL_optstring(L, 3, "accelerateDecelerate");

    if (gameState == nullptr || hotspotId == nullptr || interpolation == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptPanCameraToHotspot(
            *gameState,
            std::string(hotspotId),
            durationMs,
            std::string(interpolation));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_shakeScreen(lua_State* L)
{
    const float durationMs = static_cast<float>(luaL_checknumber(L, 1));
    const float strengthPx = static_cast<float>(luaL_checknumber(L, 2));
    const float frequencyHz = static_cast<float>(luaL_optnumber(L, 3, 30.0));
    const bool smooth = lua_toboolean(L, 4) != 0;

    if (gameState == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = AdventureScriptShakeScreen(
            *gameState,
            durationMs,
            strengthPx,
            frequencyHz,
            smooth);

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setEffectRegionVisible(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    const bool visible = lua_toboolean(L, 2) != 0;

    if (!gameState || !id) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetEffectRegionVisible(*gameState, id, visible);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_effectRegionVisible(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    if (!gameState || !id) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bool visible = false;
    const bool ok = TopdownScriptIsEffectRegionVisible(*gameState, id, visible);

    lua_pushboolean(L, ok ? (visible ? 1 : 0) : 0);
    return 1;
}

static int Lua_setEffectRegionOpacity(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);
    const float opacity = (float)luaL_checknumber(L, 2);

    if (!gameState || !id) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetEffectRegionOpacity(*gameState, id, opacity);
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_effectRegionOpacity(lua_State* L)
{
    const char* id = luaL_checkstring(L, 1);

    if (!gameState || !id) {
        lua_pushnumber(L, 0.0);
        return 1;
    }

    float opacity = 0.0f;
    const bool ok = TopdownScriptGetEffectRegionOpacity(*gameState, id, opacity);

    lua_pushnumber(L, ok ? opacity : 0.0f);
    return 1;
}

static int Lua_walkToSpawn(lua_State* L)
{
    const char* spawnId = luaL_checkstring(L, 1);

    if (gameState == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkToSpawn(*gameState, std::string(spawnId));
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_runToSpawn(lua_State* L)
{
    const char* spawnId = luaL_checkstring(L, 1);

    if (gameState == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunToSpawn(*gameState, std::string(spawnId));
    if (!ok) {
        lua_pushboolean(L, 0);
        return 1;
    }

    if (!StartPendingWait(L, ScriptWaitType::WalkComplete, 0.0f)) {
        return 1;
    }

    return lua_yieldk(L, 0, 0, Lua_WaitContinuation);
}

static int Lua_startWalkToSpawn(lua_State* L)
{
    const char* spawnId = luaL_checkstring(L, 1);

    if (gameState == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkToSpawn(*gameState, std::string(spawnId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startRunToSpawn(lua_State* L)
{
    const char* spawnId = luaL_checkstring(L, 1);

    if (gameState == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunToSpawn(*gameState, std::string(spawnId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_spawnNpc(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const char* assetId = luaL_checkstring(L, 2);
    const char* spawnId = luaL_checkstring(L, 3);

    if (gameState == nullptr || npcId == nullptr || assetId == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSpawnNpc(
            *gameState,
            std::string(npcId),
            std::string(assetId),
            std::string(spawnId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_removeNpc(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);

    if (gameState == nullptr || npcId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptRemoveNpc(*gameState, std::string(npcId));
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkNpcTo(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || npcId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkNpcTo(*gameState, std::string(npcId), Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startRunNpcTo(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const float x = static_cast<float>(luaL_checknumber(L, 2));
    const float y = static_cast<float>(luaL_checknumber(L, 3));

    if (gameState == nullptr || npcId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunNpcTo(*gameState, std::string(npcId), Vector2{x, y});
    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startWalkNpcToSpawn(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const char* spawnId = luaL_checkstring(L, 2);

    if (gameState == nullptr || npcId == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartWalkNpcToSpawn(
            *gameState,
            std::string(npcId),
            std::string(spawnId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_startRunNpcToSpawn(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const char* spawnId = luaL_checkstring(L, 2);

    if (gameState == nullptr || npcId == nullptr || spawnId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptStartRunNpcToSpawn(
            *gameState,
            std::string(npcId),
            std::string(spawnId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_playNpcAnimation(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || npcId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptPlayNpcAnimation(
            *gameState,
            std::string(npcId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_setNpcAnimation(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);
    const char* animationName = luaL_checkstring(L, 2);

    if (gameState == nullptr || npcId == nullptr || animationName == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptSetNpcAnimation(
            *gameState,
            std::string(npcId),
            std::string(animationName));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

static int Lua_clearNpcAnimation(lua_State* L)
{
    const char* npcId = luaL_checkstring(L, 1);

    if (gameState == nullptr || npcId == nullptr) {
        lua_pushboolean(L, 0);
        return 1;
    }

    const bool ok = TopdownScriptClearNpcAnimation(
            *gameState,
            std::string(npcId));

    lua_pushboolean(L, ok ? 1 : 0);
    return 1;
}

void RegisterLuaAPI(lua_State* L)
{
    lua_register(L, "setFlag", Lua_setFlag);
    lua_register(L, "flag", Lua_flag);
    lua_register(L, "setInt", Lua_setInt);
    lua_register(L, "getInt", Lua_getInt);
    lua_register(L, "setString", Lua_setString);
    lua_register(L, "getString", Lua_getString);

    lua_register(L, "hasItem", Lua_hasItem);
    lua_register(L, "giveItem", Lua_giveItem);
    lua_register(L, "removeItem", Lua_removeItem);

    lua_register(L, "actorHasItem", Lua_actorHasItem);
    lua_register(L, "giveItemTo", Lua_giveItemTo);
    lua_register(L, "removeItemFrom", Lua_removeItemFrom);
    lua_register(L, "clearHeldItem", Lua_clearHeldItem);
    lua_register(L, "setHeldItem", Lua_setHeldItem);

    lua_register(L, "say", Lua_say);
    lua_register(L, "sayProp", Lua_sayProp);
    lua_register(L, "sayAt", Lua_sayAt);
    lua_register(L, "sayActor", Lua_sayActor);
    lua_register(L, "startSay", Lua_startSay);
    lua_register(L, "startSayProp", Lua_startSayProp);
    lua_register(L, "startSayAt", Lua_startSayAt);
    lua_register(L, "startSayActor", Lua_startSayActor);

    lua_register(L, "dialogue", Lua_dialogue);

    lua_register(L, "walkTo", Lua_walkTo);
    lua_register(L, "runTo", Lua_runTo);

    lua_register(L, "walkToHotspot", Lua_walkToHotspot);
    lua_register(L, "delay", Lua_delay);
    lua_register(L, "changeLevel", Lua_changeLevel);
    lua_register(L, "playAnimation", Lua_playAnimation);
    lua_register(L, "playPropAnimation", Lua_playPropAnimation);
    lua_register(L, "setPropAnimation", Lua_setPropAnimation);
    lua_register(L, "setPropPosition", Lua_setPropPosition);
    lua_register(L, "movePropTo", Lua_movePropTo);

    lua_register(L, "setPropVisible", Lua_setPropVisible);
    lua_register(L, "setPropFlipX", Lua_setPropFlipX);
    lua_register(L, "setPropPositionRelative", Lua_setPropPositionRelative);
    lua_register(L, "movePropBy", Lua_movePropBy);

    lua_register(L, "walkActorTo", Lua_walkActorTo);
    lua_register(L, "walkActorToHotspot", Lua_walkActorToHotspot);
    lua_register(L, "walkActorToExit", Lua_walkActorToExit);


    lua_register(L, "startWalkTo", Lua_startWalkTo);
    lua_register(L, "startRunTo", Lua_startRunTo);

    lua_register(L, "walkToSpawn", Lua_walkToSpawn);
    lua_register(L, "runToSpawn", Lua_runToSpawn);

    lua_register(L, "startWalkToSpawn", Lua_startWalkToSpawn);
    lua_register(L, "startRunToSpawn", Lua_startRunToSpawn);

    lua_register(L, "spawnNpc", Lua_spawnNpc);
    lua_register(L, "removeNpc", Lua_removeNpc);

    lua_register(L, "startWalkNpcTo", Lua_startWalkNpcTo);
    lua_register(L, "startRunNpcTo", Lua_startRunNpcTo);
    lua_register(L, "startWalkNpcToSpawn", Lua_startWalkNpcToSpawn);
    lua_register(L, "startRunNpcToSpawn", Lua_startRunNpcToSpawn);

    lua_register(L, "setNpcAnimation", Lua_setNpcAnimation);
    lua_register(L, "clearNpcAnimation", Lua_clearNpcAnimation);
    lua_register(L, "playNpcAnimation", Lua_playNpcAnimation);
    lua_register(L, "playNpcAnimation", Lua_playNpcAnimation);

    lua_register(L, "startWalkToHotspot", Lua_startWalkToHotspot);
    lua_register(L, "startWalkToExit", Lua_startWalkToExit);

    lua_register(L, "startWalkActorTo", Lua_startWalkActorTo);
    lua_register(L, "startWalkActorToHotspot", Lua_startWalkActorToHotspot);
    lua_register(L, "startWalkActorToExit", Lua_startWalkActorToExit);

    lua_register(L, "disableControls", Lua_disableControls);
    lua_register(L, "enableControls", Lua_enableControls);

    lua_register(L, "startScript", Lua_startScript);
    lua_register(L, "stopScript", Lua_stopScript);
    lua_register(L, "stopAllScripts", Lua_stopAllScripts);


    lua_register(L, "setEffectRegionVisible", Lua_setEffectRegionVisible);
    lua_register(L, "effectRegionVisible", Lua_effectRegionVisible);
    lua_register(L, "setEffectRegionOpacity", Lua_setEffectRegionOpacity);
    lua_register(L, "effectRegionOpacity", Lua_effectRegionOpacity);

    lua_register(L, "playSound", Lua_playSound);
    lua_register(L, "stopSound", Lua_stopSound);
    lua_register(L, "playMusic", Lua_playMusic);
    lua_register(L, "stopMusic", Lua_stopMusic);

    lua_register(L, "setSoundEmitterEnabled", Lua_setSoundEmitterEnabled);
    lua_register(L, "soundEmitterEnabled", Lua_soundEmitterEnabled);
    lua_register(L, "setSoundEmitterVolume", Lua_setSoundEmitterVolume);
    lua_register(L, "playEmitter", Lua_playEmitter);
    lua_register(L, "stopEmitter", Lua_stopEmitter);

    lua_register(L, "setLayerVisible", Lua_setLayerVisible);
    lua_register(L, "layerVisible", Lua_layerVisible);
    lua_register(L, "setLayerOpacity", Lua_setLayerOpacity);
    lua_register(L, "layerOpacity", Lua_layerOpacity);

    lua_register(L, "cameraFollow", Lua_cameraFollow);
    lua_register(L, "cameraFollowActor", Lua_cameraFollowActor);
    lua_register(L, "setCameraPosition", Lua_setCameraPosition);
    lua_register(L, "moveCameraTo", Lua_moveCameraTo);

    lua_register(L, "centerCameraOn", Lua_centerCameraOn);
    lua_register(L, "moveCameraCenterTo", Lua_moveCameraCenterTo);
    lua_register(L, "panCameraToActor", Lua_panCameraToActor);
    lua_register(L, "panCameraToProp", Lua_panCameraToProp);
    lua_register(L, "panCameraToHotspot", Lua_panCameraToHotspot);
    lua_register(L, "shakeScreen", Lua_shakeScreen);

    lua_register(L, "print", Lua_consolePrint);
    lua_register(L, "log", Lua_log);
    lua_register(L, "logf", Lua_logf);


}

void RegisterLuaTalkColorGlobals(lua_State* L)
{
    for (int i = 0; i < GetTalkColorEntryCount(); ++i) {
        const TalkColorEntry& entry = GetTalkColorEntry(i);
        lua_pushstring(L, entry.name);
        lua_setglobal(L, entry.name);
    }
}
