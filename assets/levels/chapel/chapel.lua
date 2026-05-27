local Glow = require("effects.glow")

function Level_onEnter()
    log("on enter")
    startScript("StreetLampGlowLoop")
    startScript("ShedLampGlowLoop")
    startScript("ChapelLamp1GlowLoop")
    startScript("ChapelLamp2GlowLoop")
    startScript("ChapelLamp3GlowLoop")
    startScript("ChapelLamp4GlowLoop")

    addEquipmentSet("handgun")
    equipEquipmentSet("handgun")
    addAmmo("pistol", 24)
    setLoadedAmmo("handgun", 12)

    addEquipmentSet("shotgun")
    addAmmo("shotgun", 12)

    --setDoorLocked("left_cemetary_door", true)
end

function StreetLampGlowLoop()
    Glow.runElectric(
        { "unused", "street_lamp_glow" },
        0.00,
        0.45
    )
end

function ShedLampGlowLoop()
    Glow.runElectric(
        { "unused", "shed_lamp_glow" },
        0.00,
        0.25
    )
end

function ChapelLamp1GlowLoop()
    Glow.runFire(
        { "unused", "chapel_lamp1_glow" },
        0.00,
        0.25
    )
end

function ChapelLamp2GlowLoop()
    Glow.runFire(
        { "unused", "chapel_lamp2_glow" },
        0.00,
        0.25
    )
end

function ChapelLamp3GlowLoop()
    Glow.runFire(
        { "unused", "chapel_lamp3_glow" },
        0.00,
        0.25
    )
end

function ChapelLamp4GlowLoop()
    Glow.runFire(
        { "unused", "chapel_lamp4_glow" },
        0.00,
        0.25
    )
end