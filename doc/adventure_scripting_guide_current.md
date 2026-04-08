# Adventure Engine Lua Scripting Guide

A practical reference for the current Lua API exposed by the engine.

This version is based on the current `ScriptSystemLuaApi.cpp` bindings and is meant to be easy to scan while writing scene scripts.

---

## Table of Contents

- [Cheat Sheet](#cheat-sheet)
- [General Notes](#general-notes)
- [Scene Hook Naming Conventions](#scene-hook-naming-conventions)
- [Talk Color Globals](#talk-color-globals)
- [Persistent Script State](#persistent-script-state)
- [Inventory Functions](#inventory-functions)
- [Held Item Functions](#held-item-functions)
- [Speech Functions](#speech-functions)
- [Dialogue](#dialogue)
- [Blocking Movement and Timing](#blocking-movement-and-timing)
- [Non-Blocking Movement](#non-blocking-movement)
- [Facing and Scene Changes](#facing-and-scene-changes)
- [Animation Functions](#animation-functions)
- [Prop Functions](#prop-functions)
- [Actor Functions](#actor-functions)
- [Control Locking](#control-locking)
- [Background Scripts](#background-scripts)
- [Effects / Lighting Functions](#effects--lighting-functions)
- [Audio Functions](#audio-functions)
- [Layer Functions](#layer-functions)
- [Camera Functions](#camera-functions)
- [Logging Helpers](#logging-helpers)
- [Return Semantics and Fallback Behavior](#return-semantics-and-fallback-behavior)
- [Choicesets and conversation](#choice-sets)
- [Examples](#examples)
- [Complete API List](#complete-api-list)

---

## Cheat Sheet

### State
- `setFlag(name, value)`
- `flag(name)`
- `setInt(name, value)`
- `getInt(name)`
- `setString(name, value)`
- `getString(name)`

### Inventory
- `hasItem(itemId)`
- `giveItem(itemId)`
- `removeItem(itemId)`
- `actorHasItem(actorId, itemId)`
- `giveItemTo(actorId, itemId)`
- `removeItemFrom(actorId, itemId)`
- `clearHeldItem()`
- `setHeldItem(itemId)`

### Speech
- `say(text [, color] [, durationMs])`
- `sayActor(actorId, text [, color] [, durationMs])`
- `sayProp(propId, text [, color] [, durationMs])`
- `sayAt(x, y, text [, color] [, durationMs])`

### Dialogue
- `dialogue(choiceSetId [, hiddenOptionIds])`

### Blocking movement / waits
- `walkTo(x, y)`
- `walkToHotspot(hotspotId)`
- `walkToExit(exitId)`
- `walkActorTo(actorId, x, y)`
- `walkActorToHotspot(actorId, hotspotId)`
- `walkActorToExit(actorId, exitId)`
- `delay(ms)`

### Non-blocking movement
- `startWalkTo(x, y)`
- `startWalkToHotspot(hotspotId)`
- `startWalkToExit(exitId)`
- `startWalkActorTo(actorId, x, y)`
- `startWalkActorToHotspot(actorId, hotspotId)`
- `startWalkActorToExit(actorId, exitId)`

### Facing / scene
- `face(facingName)`
- `faceActor(actorId, facingName)`
- `changeScene(sceneId [, spawnId])`

### Animation
- `playAnimation(animationName)`
- `playActorAnimation(actorId, animationName)`
- `playPropAnimation(propId, animationName)`
- `setPropAnimation(propId, animationName)`

### Props
- `setPropPosition(propId, x, y)`
- `setPropPositionRelative(propId, dx, dy)`
- `movePropTo(propId, x, y, durationMs, interpolation)`
- `movePropBy(propId, dx, dy, durationMs, interpolation)`
- `setPropVisible(propId, visible)`
- `setPropFlipX(propId, flipX)`

### Actors
- `controlActor(actorId)`
- `setActorVisible(actorId, visible)`

### Controls
- `disableControls()`
- `enableControls()`

### Background scripts
- `startScript(functionName)`
- `stopScript(functionName)`
- `stopAllScripts()`

### Effects
- `setEffectVisible(effectId, visible)`
- `effectVisible(effectId)`
- `setEffectOpacity(effectId, opacity)`
- `setEffectTint(effectId, r, g, b [, a])`

### Audio
- `playSound(audioId)`
- `playMusic(id [, fadeMs])`
- `stopMusic([fadeMs])`
- `setSoundEmitterEnabled(emitterId, enabled)`
- `soundEmitterEnabled(emitterId)`
- `setSoundEmitterVolume(emitterId, volume)`
- `playEmitter(emitterId)`
- `stopEmitter(emitterId)`

### Layers
- `setLayerVisible(layerName, visible)`
- `layerVisible(layerName)`
- `toggleLayer(layerName)`
- `setLayerOpacity(layerName, opacity)`
- `layerOpacity(layerName)`

### Camera
- `cameraFollow()`
- `cameraFollowActor(actorId)`
- `setCameraPosition(x, y)`
- `moveCameraTo(x, y, durationMs, interpolation)`
- `centerCameraOn(x, y)`
- `moveCameraCenterTo(x, y, durationMs [, interpolation])`
- `panCameraToActor(actorId, durationMs [, interpolation])`
- `panCameraToProp(propId, durationMs [, interpolation])`
- `panCameraToHotspot(hotspotId, durationMs [, interpolation])`

### Logging
- `print(...)`
- `log(text)`
- `logf(format, ...)`

---

## General Notes

Scene scripts are plain Lua files loaded per scene.

Typical scene-level hooks:

```lua
function Scene_onEnter()
    log("Entered scene")
end

function Scene_onExit()
    log("Leaving scene")
end
```

A lot of engine commands are **blocking from the script's point of view**. These yield the current Lua coroutine and resume automatically when the wait condition finishes.

Common yielding commands:

- `say(...)`
- `sayActor(...)`
- `sayProp(...)`
- `sayAt(...)`
- `dialogue(...)`
- `walkTo(...)`
- `walkToHotspot(...)`
- `walkToExit(...)`
- `walkActorTo(...)`
- `walkActorToHotspot(...)`
- `walkActorToExit(...)`
- `delay(...)`

Non-blocking movement variants are prefixed with `start...`.

---

## Scene Hook Naming Conventions

These are naming patterns the engine looks for when interactions happen.

### Scene lifecycle

```lua
Scene_onEnter()
Scene_onExit()
```

### Hotspots

```lua
Scene_use_<hotspotId>()
Scene_look_<hotspotId>()
```

Example:

```lua
function Scene_use_furnace()
    say("It's hot.")
    return true
end
```

### Actors

```lua
Scene_use_actor_<actorId>()
Scene_look_actor_<actorId>()
```

### Item examine

```lua
Scene_look_item_<itemId>()
```

### Item on item

```lua
Scene_use_item_<heldItemId>_on_item_<targetItemId>()
```

### Item on hotspot

```lua
Scene_use_item_<heldItemId>_on_hotspot_<hotspotId>()
```

### Item on actor

```lua
Scene_use_item_<heldItemId>_on_actor_<actorId>()
```

### Item on exit

```lua
Scene_use_item_<heldItemId>_on_exit_<exitId>()
```

### Exits

There are two separate possibilities:

1. Script hook names matching the generic object-id pattern used by the action system:

```lua
Scene_use_<exitId>()
Scene_look_<exitId>()
```

2. Item-on-exit hooks:

```lua
Scene_use_item_<heldItemId>_on_exit_<exitId>()
```

### Notes

- IDs should match the engine-authored IDs from Tiled / JSON.
- If a function is missing, engine fallback behavior may run.
- For item-use handlers, returning `false` keeps the held item selected.

---

## Talk Color Globals

Named talk colors are exposed as Lua globals.

All available globals:

```
WHITE
CREAM
YELLOW
ORANGE
RED
PINK
MAGENTA
PURPLE
BLUE
CYAN
GREEN
LIME
```

These are passed as strings under the hood, so use them directly:

```lua
say("That looks bad.", RED)
sayActor("npc_actor", "No kidding.", CYAN)
```

---

## Persistent Script State

These values live in the global script state and are saved / loaded with the game.

### `setFlag(name, value)`

Sets a boolean flag.

```lua
setFlag("windowUnlocked", true)
```

### `flag(name)`

Reads a boolean flag.

Missing flags default to `false`.

```lua
if flag("windowUnlocked") then
    say("It's already unlocked.")
end
```

### `setInt(name, value)`

Sets an integer value.

```lua
setInt("coinCount", 3)
```

### `getInt(name)`

Reads an integer value.

Missing ints default to `-1`.

```lua
local coins = getInt("coinCount")
```

### `setString(name, value)`

Sets a string value.

```lua
setString("lastRoom", "basement")
```

### `getString(name)`

Reads a string value.

Missing strings default to `""`.

```lua
local room = getString("lastRoom")
```

---

## Inventory Functions

These work on the currently controlled actor unless otherwise noted.

### `hasItem(itemId)`

Returns whether the controlled actor has the item.

```lua
if hasItem("rusty_key") then
    say("Still got it.")
end
```

### `giveItem(itemId)`

Gives an item to the controlled actor.

```lua
giveItem("rusty_key")
```

### `removeItem(itemId)`

Removes an item from the controlled actor.

```lua
removeItem("rusty_key")
```

### `actorHasItem(actorId, itemId)`

Checks whether a specific actor has an item.

```lua
if actorHasItem("npc_actor", "badge") then
    say("He has the badge.")
end
```

### `giveItemTo(actorId, itemId)`

Gives an item to a specific actor.

```lua
giveItemTo("npc_actor", "badge")
```

### `removeItemFrom(actorId, itemId)`

Removes an item from a specific actor.

```lua
removeItemFrom("npc_actor", "badge")
```

---

## Held Item Functions

### `clearHeldItem()`

Clears the currently selected / held inventory item for the controlled actor.

```lua
clearHeldItem()
```

### `setHeldItem(itemId)`

Sets the currently held inventory item for the controlled actor.

This only succeeds if the actor already has that item.

```lua
setHeldItem("rusty_key")
```

---

## Speech Functions

Speech commands yield until the speech finishes.

### `say(text [, color] [, durationMs])`
### `say(text [, durationMs])`

Speaks as the currently controlled actor.

```lua
say("This place reeks.")
say("This place reeks.", 2500)
say("This place reeks.", YELLOW)
say("This place reeks.", YELLOW, 2500)
```

### `sayActor(actorId, text [, color] [, durationMs])`
### `sayActor(actorId, text [, durationMs])`

Speaks anchored to an actor.

```lua
sayActor("janitor", "Keep out of there.")
sayActor("janitor", "Keep out of there.", CYAN)
```

### `sayProp(propId, text [, color] [, durationMs])`
### `sayProp(propId, text [, durationMs])`

Speaks anchored to a prop.

```lua
sayProp("radio", "Bzzzzt...")
sayProp("cat", "Meooow!", PINK)
```

### `sayAt(x, y, text [, color] [, durationMs])`
### `sayAt(x, y, text [, durationMs])`

Speaks at a world position.

```lua
sayAt(850, 420, "A cold draft blows in here.")
```

### Optional arguments rules

The optional argument parser works like this:

- if the first optional argument is a talk color global, it is treated as the color
- if the first optional argument is a number, it is treated as duration
- if both are present, color comes before duration

So these are valid:

```lua
say("Hello")
say("Hello", 1500)
say("Hello", RED)
say("Hello", RED, 1500)
```

---

## Dialogue

### `dialogue(choiceSetId [, hiddenOptionIds])`

Starts a dialogue choice set and yields until the player picks an option or the dialogue closes.

Returns:
- selected option id as a string
- `nil` if no result was produced

```lua
local choice = dialogue("npc_actor_intro")

if choice == "who_are_you" then
    say("Who are you?")
elseif choice == "goodbye" then
    say("Never mind.")
end
```

Hide specific options by passing a Lua array of option IDs:

```lua
local choice = dialogue("npc_actor_intro", {
    "insult",
    "give_sausage"
})
```

---

## Blocking Movement and Timing

These functions yield until complete.

### `walkTo(x, y)`

Walk the controlled actor to a world position.

```lua
walkTo(1100, 640)
```

### `walkToHotspot(hotspotId)`

Walk the controlled actor to a hotspot walk target.

```lua
walkToHotspot("furnace")
```

### `walkToExit(exitId)`

Walk the controlled actor to an exit walk target.

```lua
walkToExit("stairs_up")
```

### `walkActorTo(actorId, x, y)`

Walk a specific actor to a world position.

```lua
walkActorTo("janitor", 900, 610)
```

### `walkActorToHotspot(actorId, hotspotId)`

```lua
walkActorToHotspot("janitor", "door")
```

### `walkActorToExit(actorId, exitId)`

```lua
walkActorToExit("janitor", "hallway")
```

### `delay(ms)`

Yield for a fixed time in milliseconds.

```lua
delay(1000)
say("One second later...")
```

---

## Non-Blocking Movement

These start movement and return immediately.

### `startWalkTo(x, y)`

```lua
startWalkTo(1100, 640)
```

### `startWalkToHotspot(hotspotId)`

```lua
startWalkToHotspot("furnace")
```

### `startWalkToExit(exitId)`

```lua
startWalkToExit("stairs_up")
```

### `startWalkActorTo(actorId, x, y)`

```lua
startWalkActorTo("janitor", 900, 610)
```

### `startWalkActorToHotspot(actorId, hotspotId)`

```lua
startWalkActorToHotspot("janitor", "door")
```

### `startWalkActorToExit(actorId, exitId)`

```lua
startWalkActorToExit("janitor", "hallway")
```

---

## Facing and Scene Changes

### `face(facingName)`

Faces the controlled actor immediately.

Valid values:

- `"left"`
- `"right"`
- `"front"`
- `"back"`

```lua
face("left")
```

### `faceActor(actorId, facingName)`

Faces a specific actor immediately.

```lua
faceActor("janitor", "right")
```

### `changeScene(sceneId [, spawnId])`

Queues a scene change.

```lua
changeScene("street")
changeScene("street", "alley_entry")
```

---

## Animation Functions

### `playAnimation(animationName)`

Play a one-shot animation on the controlled actor.

```lua
playAnimation("reach_right")
```

### `playActorAnimation(actorId, animationName)`

Play a one-shot animation on a specific actor.

```lua
playActorAnimation("janitor", "wave")
```

### `playPropAnimation(propId, animationName)`

Play a one-shot animation on a prop.

```lua
playPropAnimation("furnace_fire", "ignite")
```

### `setPropAnimation(propId, animationName)`

Set a persistent / looping prop animation.

```lua
setPropAnimation("furnace_fire", "idle")
```

---

## Prop Functions

### `setPropPosition(propId, x, y)`

Set a prop to an absolute world position.

```lua
setPropPosition("crate", 840, 620)
```

### `setPropPositionRelative(propId, dx, dy)`

Move a prop relative to its current position.

```lua
setPropPositionRelative("crate", 20, 0)
```

### `movePropTo(propId, x, y, durationMs, interpolation)`

Tween a prop to an absolute position.

Supported interpolation names:

- `"linear"`
- `"accelerate"`
- `"decelerate"`
- `"accelerateDecelerate"`
- `"overshoot"`

```lua
movePropTo("crate", 900, 620, 500, "overshoot")
```

### `movePropBy(propId, dx, dy, durationMs, interpolation)`

Tween a prop relative to its current position.

```lua
movePropBy("crate", 60, 0, 400, "accelerateDecelerate")
```

### `setPropVisible(propId, visible)`

Show or hide a prop.

```lua
setPropVisible("rusty_key_prop", false)
```

### `setPropFlipX(propId, flipX)`

Flip a prop horizontally.

```lua
setPropFlipX("crate", true)
```

---

## Actor Functions

### `controlActor(actorId)`

Switches the controlled actor.

The target actor must exist in the scene, be visible, and be marked controllable.

```lua
controlActor("sam")
```

### `setActorVisible(actorId, visible)`

Show or hide an actor.

```lua
setActorVisible("janitor", false)
```

---

## Control Locking

### `disableControls()`

Disables player controls.

This also prevents the inventory from opening.

```lua
disableControls()
```

### `enableControls()`

Re-enables player controls.

```lua
enableControls()
```

---

## Background Scripts

The engine supports multiple Lua coroutines. Scene hooks usually run as foreground scripts, but you can also start background loops manually.

### `startScript(functionName)`

Starts a function by name.

```lua
startScript("StreetTraffic")
startScript("FlickerLightBulbLoop")
```

### `stopScript(functionName)`

Stops a running function by name.

```lua
stopScript("StreetTraffic")
```

### `stopAllScripts()`

Stops all running scripts.

```lua
stopAllScripts()
```

---

## Effects / Lighting Functions

These operate on scene effect IDs from authored effect layers.

### `setEffectVisible(effectId, visible)`

```lua
setEffectVisible("light_bulb", true)
```

### `effectVisible(effectId)`

Returns a boolean.

```lua
if effectVisible("light_bulb") then
    say("The bulb is on.")
end
```

### `setEffectOpacity(effectId, opacity)`

Opacity is clamped to the range `0.0 .. 1.0`.

```lua
setEffectOpacity("furnace_glow_core", 0.75)
```

### `setEffectTint(effectId, r, g, b [, a])`

RGBA integer channels in the range `0 .. 255`.

```lua
setEffectTint("furnace_glow_core", 255, 220, 180)
setEffectTint("furnace_glow_core", 255, 220, 180, 200)
```

---

## Audio Functions

### `playSound(audioId)`

Plays a sound by audio definition ID.

Typical use: UI clicks, one-shot SFX, scripted one-shot sounds.

```lua
playSound("ui_click")
playSound("gunshot")
```

### `playMusic(id [, fadeMs])`

Starts music by audio definition ID.

If another music track is already playing, the audio system handles the transition.

```lua
playMusic("basement_theme")
playMusic("street_theme", 1000)
```

### `stopMusic([fadeMs])`

Stops the currently playing music.

Fade duration is optional.

```lua
stopMusic()
stopMusic(1200)
```

### `setSoundEmitterEnabled(emitterId, enabled)`

Enable or disable a scene sound emitter.

Useful for looping spatial emitters like furnaces, machines, waterfalls.

```lua
setSoundEmitterEnabled("furnace_hum", true)
setSoundEmitterEnabled("furnace_hum", false)
```

### `soundEmitterEnabled(emitterId)`

Returns whether a scene emitter is enabled.

```lua
if soundEmitterEnabled("furnace_hum") then
    log("Emitter is on")
end
```

### `setSoundEmitterVolume(emitterId, volume)`

Sets the runtime volume multiplier for a scene sound emitter.

This is separate from the authored base volume.

```lua
setSoundEmitterVolume("furnace_hum", 0.5)
```

### `playEmitter(emitterId)`

Triggers a scene sound emitter manually.

This is mainly useful for **non-looping emitters** that you want to play from script at an authored world position.

```lua
playEmitter("steam_burst")
```

### `stopEmitter(emitterId)`

Stops a scene sound emitter.

Mainly useful for looping emitters.

```lua
stopEmitter("furnace_hum")
```

---

---

## Logging Helpers

### `print(...)`

Console-style print helper.

Prints values separated by tabs/spaces to the debug console / script output.

```lua
print("hello", 42, true)
```

### `log(text)`

Logs a plain message.

```lua
log("Entered puzzle branch")
```

### `logf(format, ...)`

Formatted logging using Lua's `string.format`.

```lua
logf("Player has %d coins", getInt("coinCount"))
```

---

## Return Semantics and Fallback Behavior

### General interaction handlers

For handlers like:

- `Scene_use_<id>()`
- `Scene_look_<id>()`
- `Scene_use_actor_<actorId>()`
- `Scene_look_actor_<actorId>()`

recommended semantics are:

- `return true` → handled success
- `return false` → explicitly handled failure

### Missing script hooks

If a hook is missing, the engine may fall back to default behavior.

Examples:
- missing `Scene_look_item_<itemId>()` falls back to the item's `lookText`
- missing item-use hook falls back to `"That won't work."`

### Item use handlers

For item-use hooks:

```lua
Scene_use_item_<heldItemId>_on_item_<targetItemId>()
Scene_use_item_<heldItemId>_on_hotspot_<hotspotId>()
Scene_use_item_<heldItemId>_on_actor_<actorId>()
Scene_use_item_<heldItemId>_on_exit_<exitId>()
```

Returning `false` keeps the held item selected.

For successful interactions that consume the item, explicitly remove it in script:

```lua
removeItem("rusty_key")
return true
```

### Yielding handlers

Yielding handlers are fine. Just remember that if success should consume an item, the safest pattern is still to explicitly remove it in script.

---
## Choice Sets

Dialogue options are authored as JSON files in:

```text
assets/dialogue/
```

Each file can contain **one or more choice sets**.

Example:

```json
{
    "choiceSets": [
        {
            "id": "npc_actor_intro",
            "options": [
                { "id": "who_are_you", "text": "Who are you?" },
                { "id": "what_happened", "text": "What happened here?" },
                { "id": "rusty_key", "text": "About that rusty key..." },
                { "id": "sausage", "text": "About this sausage..." },
                { "id": "insult", "text": "You seem like an asshole." },
                { "id": "apologize", "text": "Sorry about that." },
                { "id": "give_sausage", "text": "You can have the sausage." },
                { "id": "goodbye", "text": "Never mind." }
            ]
        }
    ]
}
```

### Important properties

#### `choiceSets`

Top-level array of choice set definitions.

A single file may define multiple sets.

#### `id`

Unique ID of the choice set.

This is what Lua passes to `dialogue()`:

```lua
local choice = dialogue("npc_actor_intro")
```

#### `options`

Array of options shown to the player.

Each option has:

- `id` → internal identifier returned to Lua
- `text` → text shown in the dialogue UI

Example:

```json
{ "id": "who_are_you", "text": "Who are you?" }
```

If the player selects that line, Lua receives:

```lua
choice == "who_are_you"
```

---

### Loading Model

Choice sets are **global data**.

That means:

- all dialogue JSON files in `assets/dialogue/` are loaded at engine startup
- scene scripts do **not** reference dialogue files directly
- Lua only needs the choice set ID

So this:

```lua
dialogue("npc_actor_intro")
```

does **not** load a file on demand — it looks up an already-loaded global choice set.

---

### Relationship Between Authored Data and Lua

The mapping is:

- JSON `choiceSets[].id` → `dialogue(choiceSetId)`
- JSON `options[].id` → value returned from `dialogue()`
- JSON `options[].text` → what the player sees on screen

#### Example

Authored data:

```json
{
    "id": "npc_actor_intro",
    "options": [
        { "id": "who_are_you", "text": "Who are you?" },
        { "id": "goodbye", "text": "Never mind." }
    ]
}
```

Lua:

```lua
local choice = dialogue("npc_actor_intro")

if choice == "who_are_you" then
    say("Who are you?")
elseif choice == "goodbye" then
    say("Never mind.")
end
```

---

### Hidden Options and Choice Sets

`dialogue(choiceSetId, hiddenOptions)` does **not** modify the underlying choice set.

It only hides selected option IDs **for that call**.

Example:

```lua
local hidden = {
    "who_are_you",
    "insult"
}

local choice = dialogue("npc_actor_intro", hidden)
```

This means:

- the choice set still contains those options
- they are just not shown this time
- the next call may show them again if they are not hidden

This is what makes the system reusable.

You author a **full superset** of possible replies once, then script logic decides which ones are currently available.

---

### Why This Design Is Useful

This split is powerful:

#### Authored data handles:
- visible player-facing reply text
- stable option IDs
- reusable conversation structure

#### Lua handles:
- progression
- branching
- state
- filtering
- side effects

So instead of authoring multiple near-duplicate dialogue files, you usually author **one full choice set** and then prune it dynamically.

---

### Typical Pattern

#### 1. Author a full choice set

```json
{
    "id": "npc_actor_intro",
    "options": [
        { "id": "who_are_you", "text": "Who are you?" },
        { "id": "what_happened", "text": "What happened here?" },
        { "id": "insult", "text": "You seem like an asshole." },
        { "id": "apologize", "text": "Sorry about that." },
        { "id": "goodbye", "text": "Never mind." }
    ]
}
```

#### 2. Build hidden options from game state

```lua
function BuildNpcActorIntroHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end
```

#### 3. Pass them into `dialogue()` or `Adv.runConversationDynamic()`

```lua
local hidden = BuildNpcActorIntroHiddenOptions()
local choice = dialogue("npc_actor_intro", hidden)
```

or:

```lua
Adv.runConversationDynamic("npc_actor_intro", handlers, BuildNpcActorIntroHiddenOptions)
```

---

### Mental Model

Think of a choice set as:

- a **database of possible replies**
- not a fixed one-time menu

The script decides which subset is currently active.

That is why `Adv.hiddenOptions()` and `Adv.runConversationDynamic()` are so useful:
they let one authored choice set behave like a progressing conversation without duplicating data.

---

### Practical Naming Advice

Because option IDs are what Lua switches on, they should be:

- stable
- short
- descriptive
- logic-oriented, not prose-oriented

Good:

```json
{ "id": "who_are_you", "text": "Who are you?" }
{ "id": "give_sausage", "text": "You can have the sausage." }
```

Bad:

```json
{ "id": "option1", "text": "Who are you?" }
{ "id": "that_funny_line", "text": "You can have the sausage." }
```

The `text` is for the player.  
The `id` is for your script logic.

---

### Summary

- choice sets live in JSON under `assets/dialogue/`
- all choice sets are loaded globally at engine startup
- Lua references them only by choice set ID
- option `id` is returned to Lua
- option `text` is shown to the player
- hidden options only affect the current dialogue call
- the intended workflow is:
    - author one full choice set
    - use Lua state to hide / reveal options dynamically

## Adv.runConversation() vs Adv.runConversationDynamic()

Both helpers are wrappers around:

```lua
dialogue(choiceSetId [, hiddenOptions])
```

They exist to reduce repetitive `while true` / `if choice == ...` boilerplate.

---

### Adv.runConversation(choiceSetId, handlers [, hiddenOptions])

Use this when your hidden options are:

- fixed for the whole conversation, or
- not needed at all

Example:

```lua
Adv.runConversation("npc_actor_intro", {
    who_are_you = function()
        say("Who are you?")
        sayActor("npc_actor", "None of your business.")
    end,

    what_happened = function()
        say("What happened here?")
        sayActor("npc_actor", "Long day. Bad furnace. Worse company.")
    end,

    goodbye = function()
        say("Never mind.")
        return "exit"
    end
})
```

#### Behavior

- calls `dialogue()`
- looks up `handlers[choice]`
- runs that handler
- loops again
- stops if handler returns `"exit"` or `"break"`

#### Good fit

Use `runConversation()` when:
- the same options should remain available each loop
- you do not need to recalculate hidden options after each reply
- the conversation is simple and mostly static

---

### Adv.runConversationDynamic(choiceSetId, handlers, hiddenOptionsFn)

Use this when available replies should change as the conversation progresses.

Example:

```lua
Adv.runConversationDynamic("npc_actor_intro", {
    who_are_you = function()
        setFlag("asked_npc_name", true)
        say("Who are you?")
        sayActor("npc_actor", "None of your business.")
    end,

    what_happened = function()
        setFlag("asked_npc_what_happened", true)
        say("What happened here?")
        sayActor("npc_actor", "Long day. Bad furnace. Worse company.")
    end,

    insult = function()
        setFlag("insulted_npc_actor", true)
        say("You seem like a real joy to be around.")
        sayActor("npc_actor", "And you seem like a corpse that forgot to lie down.")
    end,

    apologize = function()
        say("Alright. Sorry.")
        sayActor("npc_actor", "Hmph. Better.")
    end,

    goodbye = function()
        say("Never mind.")
        return "exit"
    end
}, function()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end)
```

#### Behavior

Same as `runConversation()`, except:

- before **each** loop iteration
- it calls `hiddenOptionsFn()`
- then passes the returned hidden options into `dialogue()`

So option visibility can change live as flags/items/state change.

#### Good fit

Use `runConversationDynamic()` when:
- options should disappear after being asked once
- options should appear only after certain flags are set
- inventory or state should affect available replies
- the conversation evolves over time

This is usually the better choice for real game conversations.

---

### Inline hidden options vs helper function

You can write the hidden option logic inline:

```lua
Adv.runConversationDynamic("npc_actor_intro", handlers, function()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end)
```

Or extract it:

```lua
function BuildNpcActorIntroHiddenOptions()
    return Adv.hiddenOptions({
        who_are_you = flag("asked_npc_name"),
        what_happened = flag("asked_npc_what_happened"),
        apologize = not flag("insulted_npc_actor"),
        insult = flag("insulted_npc_actor")
    })
end

Adv.runConversationDynamic("npc_actor_intro", handlers, BuildNpcActorIntroHiddenOptions)
```

Both are valid.

#### Inline version
Best when:
- the condition list is short
- the logic is only used once
- you want everything visible in one place

#### Separate function
Best when:
- the hidden-option logic is large
- you want to reuse it
- you want cleaner conversation code

---

### Rule of Thumb

- use `runConversation()` for static conversations
- use `runConversationDynamic()` for stateful conversations
- inline hidden options for small cases
- extract a helper function when the logic gets noisy

---

## Examples

### Examine item

```lua
function Scene_look_item_rusty_key()
    say("An old iron key covered in rust.")
end
```

### Item on hotspot

```lua
function Scene_use_item_rusty_key_on_hotspot_window()
    say("The key turns in the lock.")
    setFlag("windowUnlocked", true)
    removeItem("rusty_key")
    return true
end
```

### Item on hotspot, handled failure

```lua
function Scene_use_item_rusty_key_on_hotspot_furnace()
    say("I am not gonna burn it!!")
    return false
end
```

### Use actor

```lua
function Scene_use_actor_npc_actor()
    sayActor("npc_actor", "What do you want?")
    return true
end
```

### Dialogue with hidden options

```lua
function BuildHidden()
    return {
        "insult",
        "give_sausage"
    }
end

function Scene_use_actor_npc_actor()
    local choice = dialogue("npc_actor_intro", BuildHidden())

    if choice == "who_are_you" then
        say("Who are you?")
    elseif choice == "goodbye" then
        say("Never mind.")
    end

    return true
end
```

### Looping background light flicker

```lua
function FlickerLoop()
    while true do
        setEffectOpacity("light_bulb_halo", math.random(50, 100) / 100)
        delay(math.random(40, 120))
    end
end

function Scene_onEnter()
    stopScript("FlickerLoop")
    startScript("FlickerLoop")
end
```

### Scripted music change

```lua
function Scene_onEnter()
    playMusic("basement_theme", 1000)
end

function Scene_onExit()
    stopMusic(500)
end
```

### Triggered emitter

```lua
function Scene_use_pipe()
    playEmitter("steam_burst")
    say("Careful.")
    return true
end
```

---

## Complete API List

### State
- `setFlag(name, value)`
- `flag(name)`
- `setInt(name, value)`
- `getInt(name)`
- `setString(name, value)`
- `getString(name)`

### Inventory / held item
- `hasItem(itemId)`
- `giveItem(itemId)`
- `removeItem(itemId)`
- `actorHasItem(actorId, itemId)`
- `giveItemTo(actorId, itemId)`
- `removeItemFrom(actorId, itemId)`
- `clearHeldItem()`
- `setHeldItem(itemId)`

### Speech / dialogue
- `say(text [, color] [, durationMs])`
- `say(text [, durationMs])`
- `sayActor(actorId, text [, color] [, durationMs])`
- `sayActor(actorId, text [, durationMs])`
- `sayProp(propId, text [, color] [, durationMs])`
- `sayProp(propId, text [, durationMs])`
- `sayAt(x, y, text [, color] [, durationMs])`
- `sayAt(x, y, text [, durationMs])`
- `dialogue(choiceSetId [, hiddenOptionIds])`

### Blocking movement / waits
- `walkTo(x, y)`
- `walkToHotspot(hotspotId)`
- `walkToExit(exitId)`
- `walkActorTo(actorId, x, y)`
- `walkActorToHotspot(actorId, hotspotId)`
- `walkActorToExit(actorId, exitId)`
- `delay(ms)`

### Non-blocking movement
- `startWalkTo(x, y)`
- `startWalkToHotspot(hotspotId)`
- `startWalkToExit(exitId)`
- `startWalkActorTo(actorId, x, y)`
- `startWalkActorToHotspot(actorId, hotspotId)`
- `startWalkActorToExit(actorId, exitId)`

### Facing / scene
- `face(facingName)`
- `faceActor(actorId, facingName)`
- `changeScene(sceneId [, spawnId])`

### Animation
- `playAnimation(animationName)`
- `playActorAnimation(actorId, animationName)`
- `playPropAnimation(propId, animationName)`
- `setPropAnimation(propId, animationName)`

### Props
- `setPropPosition(propId, x, y)`
- `setPropPositionRelative(propId, dx, dy)`
- `movePropTo(propId, x, y, durationMs, interpolation)`
- `movePropBy(propId, dx, dy, durationMs, interpolation)`
- `setPropVisible(propId, visible)`
- `setPropFlipX(propId, flipX)`

### Actors
- `controlActor(actorId)`
- `setActorVisible(actorId, visible)`

### Controls
- `disableControls()`
- `enableControls()`

### Background scripts
- `startScript(functionName)`
- `stopScript(functionName)`
- `stopAllScripts()`

### Effects
- `setEffectVisible(effectId, visible)`
- `effectVisible(effectId)`
- `setEffectOpacity(effectId, opacity)`
- `setEffectTint(effectId, r, g, b [, a])`

### Audio
- `playSound(audioId)`
- `playMusic(id [, fadeMs])`
- `stopMusic([fadeMs])`
- `setSoundEmitterEnabled(emitterId, enabled)`
- `soundEmitterEnabled(emitterId)`
- `setSoundEmitterVolume(emitterId, volume)`
- `playEmitter(emitterId)`
- `stopEmitter(emitterId)`

### Logging
- `print(...)`
- `log(text)`
- `logf(format, ...)`
