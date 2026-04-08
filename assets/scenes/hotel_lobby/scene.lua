local bell = require("audio.desk_bell")
local Glow = require("effects.glow")

local ClerkWaypoints = {
    { x = 3*157, y = 3*251, face = "left"  },
    { x = 3*202, y = 3*249, face = "front" },
    { x = 3*256, y = 3*250, face = "right" }
}

function Scene_onEnter()
    if not flag("hotel_lobby_init") then
        setFlag("hotel_lobby_init", true)
        -- first time only stuff
        -- e.g. intro dialogue, item placement, whatever
    end
    -- start running scene scripts (animation, interactivity etc)
    startScript("WallLampGlowLoop")
    startScript("CeilingLampGlowLoop")
    startScript("HotelAmbienceLoop")
    startScript("HotelClerkPacingLoop")
end

function Scene_onExit()
    stopScript("HotelAmbienceLoop")
    --setSoundEmitterEnabled("hotel_room_tone", false)
end

function Scene_look_to_town_square()
    say("Back out to the square.")
    say("Fresh air… if you can call it that.")
    return true
end

function Scene_look_desk()
    say("A well-worn reception desk.")
    say("Someone keeps it tidy, at least on the surface.")
    return true
end

function Scene_use_desk()
    return Scene_look_desk()
end

function Scene_look_key_rack()
    say("Room keys, neatly arranged.")
    say("More than I expected for a place like this.")
    return true
end

function Scene_use_key_rack()
    return Scene_look_key_rack()
end

function Scene_look_chair()
    say("An old chair with a deep seat.")
    say("Looks like it has seen more waiting than resting.")
    return true
end

function Scene_use_chair()
    say("I'd rather not sit just yet.")
    return true
end

function Scene_look_stairs()
    say("The stairs lead up to the rooms.")
    say("The wood creaks even when I'm not on it.")
    return true
end

function Scene_use_stairs()
    if not flag("hotel_room_unlocked") then


        disableControls()
        stopScript("HotelClerkPacingLoop")
        walkActorTo("hotel_clerk", ClerkWaypoints[3].x, ClerkWaypoints[3].y)
        faceActor("hotel_clerk", "right")
        if flag("hotel_room_denied") then
            sayActor("hotel_clerk", "I told you, sir. We are closed for business.")
            walkTo(3*461, 3*247)
            face("front")
            say("Strange... a good deal of noise for a place that is supposed to be empty.")
        else
            sayActor("hotel_clerk", "I would not go up there, sir.")
            walkTo(3*408, 3*260)
            face("left")
            say("I was only looking.")
            sayActor("hotel_clerk", "Then you have seen enough.")
        end
        startScript("HotelClerkPacingLoop")
        enableControls()
        return true
    end

    say("That must be my room upstairs.")
    -- later: changeScene("hotel_upstairs", "from_lobby")
    return true
end

function Scene_look_coat_rack()
    say("A few coats hang here.")
    say("None of them look recently worn.")
    return true
end

function Scene_use_coat_rack()
    return Scene_look_coat_rack()
end

function Scene_look_display_case()
    say("A model ship in a glass display case.")
    say("Someone has taken better care of it than anything else in the lobby.")
    return true
end

function Scene_use_display_case()
    return Scene_look_display_case()
end

function Scene_look_bell()
    say("A small brass desk bell.")
    say("Polished more often than the rest of the lobby.")
    return true
end

function Scene_use_bell()
    local times = getInt("hotel_bell_rang_count")
    if times < 0 then
        times = 0
    end

    if times == 0 then
        bell.RingDeskBell(1)
        setInt("hotel_bell_rang_count", 1)
        stopScript("HotelClerkPacingLoop")
        walkActorTo("hotel_clerk", ClerkWaypoints[3].x, ClerkWaypoints[3].y)
        faceActor("hotel_clerk", "front")
        sayActor("hotel_clerk", "Do not do that, sir. I am standing right here.")
        startScript("HotelClerkPacingLoop")
    elseif times == 1 then
        bell.RingDeskBell(2)
        setInt("hotel_bell_rang_count", 2)
        stopScript("HotelClerkPacingLoop")
        walkActorTo("hotel_clerk", ClerkWaypoints[3].x, ClerkWaypoints[3].y)
        faceActor("hotel_clerk", "front")
        sayActor("hotel_clerk", "I should prefer not to be summoned like a servant.")
        startScript("HotelClerkPacingLoop")
    else
        say("I had better not be overly rude.")
    end
    return true
end

function Scene_look_actor_hotel_clerk()
    walkToHotspot("desk")
    face("left")
    say("The clerk watches me with the kind of patience that usually runs out fast.")
    return true
end

function Scene_use_actor_hotel_clerk()
    disableControls()

    if flag("hotel_room_unlocked") then
        walkToHotspot("desk")
        face("left")
        say("I'd rather not speak to that unpleasant gentleman again.")
        say("I should go upstairs and see to my room.")
        enableControls()
        return true
    end

    walkToHotspot("desk")
    face("back")

    stopScript("HotelClerkPacingLoop")
    walkActorTo("hotel_clerk", ClerkWaypoints[3].x, ClerkWaypoints[3].y)
    faceActor("hotel_clerk", "front")
    sayActor("hotel_clerk", "Yes?")

    enableControls()

    Adv.runConversationDynamic("hotel_clerk_intro", {
        need_room = function()
            if not flag("hotel_room_denied") then
                setFlag("asked_clerk_room", true)
                setFlag("hotel_room_denied", true)

                say("I need a room for the night.")
                sayActor("hotel_clerk", "No rooms. We are closed for business, sir.")

                if not flag("saw_store_ledger") then
                    say("Your key rack says otherwise.")
                    sayActor("hotel_clerk", "Then it ought to mind its own business.")
                    return
                end
            else
                say("I need a room for the night.")
                sayActor("hotel_clerk", "I told you, sir. We are closed.")
                sayActor("hotel_clerk", "There are no rooms.")

                if not flag("saw_store_ledger") then
                    return
                end
            end

            -- Only reaches here if player HAS seen the ledger
            local followup = dialogue("hotel_clerk_room_followup")

            if followup == "confront_ledger" then
                setFlag("confronted_clerk_with_ledger", true)
                setFlag("hotel_room_unlocked", true)

                say("The store ledger tells a different story.")
                sayActor("hotel_clerk", "Does it.")
                say("Food, lamp oil, soap.")
                say("Enough supplies for a good many guests.")
                sayActor("hotel_clerk", "The storekeeper keeps his accounts. That is his affair.")
                say("And the inn is your affair.")
                say("You told me you had no rooms.")
                sayActor("hotel_clerk", "I told you what seemed advisable.")
                say("Advisable for whom?")
                sayActor("hotel_clerk", "...")
                sayActor("hotel_clerk", "There may be one room available.")
                sayActor("hotel_clerk", "Upstairs. End of the hall.")
                sayActor("hotel_clerk", "You will keep to it, and trouble no one.")
                say("...")
                return "exit"

            elseif followup == "leave_it" or followup == nil then
                say("Very well. Good day to you.")
                return "exit"
            end
        end,

        who_are_you = function()
            setFlag("asked_clerk_name", true)
            say("Do you run this place?")
            sayActor("hotel_clerk", "I mind the desk.")
            say("That wasn't quite what I asked.")
            sayActor("hotel_clerk", "It was close enough.")
        end,

        about_town = function()
            setFlag("asked_clerk_town", true)
            say("Quiet town.")
            sayActor("hotel_clerk", "We prefer it that way.")
        end,

        friend = function()
            setFlag("asked_clerk_friend", true)
            say("I'm looking for a friend.")
            sayActor("hotel_clerk", "Then I hope your friend had sense enough to keep moving.")
        end,

        goodbye = function()
            say("Never mind.")
            return "exit"
        end
    }, function()
        return Adv.hiddenOptions({
            who_are_you = flag("asked_clerk_name"),
            about_town = flag("asked_clerk_town"),
            friend = flag("asked_clerk_friend")
        })
    end)

    startScript("HotelClerkPacingLoop")
    return true
end

-- Ambience scripts ----------------------------
function HotelClerkPacingLoop()
    local lastIndex = -1
    while true do
        -- pick a new point (not same as last)
        local index = math.random(1, #ClerkWaypoints)
        if index == lastIndex then
            index = (index % #ClerkWaypoints) + 1
        end
        lastIndex = index

        local p = ClerkWaypoints[index]

        walkActorTo("hotel_clerk", p.x, p.y)
        faceActor("hotel_clerk", p.face)
        if index == 3 then
            if math.random(0,100) > 50 then
                playActorAnimation("hotel_clerk", "pickup_right")
            end
        end

        -- linger time (feels natural / slightly uneasy)
        delay(math.random(4000, 9000))
    end
end

-- Effect scripts -------------------------
function WallLampGlowLoop()
    Glow.runFire(
        { "wall_lamp_glow1", "wall_lamp_glow2" },
        0.50,
        0.35
    )
end

function CeilingLampGlowLoop()
    Glow.runElectric(
        { "ceiling_lamp_glow1", "ceiling_lamp_glow2" },
        0.65,
        0.45
    )
end

-- Audio ----------------------------------------------

function HotelAmbienceLoop()
    -- base bed
    --setSoundEmitterEnabled("hotel_room_tone", true)

    while true do
        delay(math.random(8000, 20000)) -- 8–20 sec gaps

        local roll = math.random(1, 100)

        if roll <= 40 then
            -- structure creaks
            local creaks = {
                "floor_creak_left",
                "floor_creak_upstairs",
                "wall_creak_right"
            }
            playEmitter(creaks[math.random(#creaks)])

        elseif roll <= 70 then
            -- subtle object sounds
            local objects = {
                "three_knocks",
                "metal_pipe"
            }
            playEmitter(objects[math.random(#objects)])

        else
            -- the important one: upstairs presence
            shakeScreen(2500, 4, 25, true)
            playEmitter("upstairs_movement")

            -- give it space to breathe
            delay(math.random(5000, 10000))
        end
    end
end
