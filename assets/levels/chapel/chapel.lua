local Glow = require("effects.glow")

function Level_onEnter()
    log("on enter")
    startScript("StreetLampGlowLoop")

    addEquipmentSet("handgun")
    equipEquipmentSet("handgun")
    addAmmo("pistol", 24)
    setLoadedAmmo("handgun", 12)

    addEquipmentSet("shotgun")
    addAmmo("shotgun", 12)
end

function StreetLampGlowLoop()
    Glow.runElectric(
        { "unused", "street_lamp_glow" },
        0.00,
        0.45
    )
end