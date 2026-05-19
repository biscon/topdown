local Glow = require("effects.glow")

function Level_onEnter()
    log("on enter")
    startScript("StreetLampGlowLoop")
end

function StreetLampGlowLoop()
    Glow.runElectric(
        { "unused", "street_lamp_glow" },
        0.00,
        0.45
    )
end