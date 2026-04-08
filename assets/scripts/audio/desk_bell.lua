-- assets/scripts/audio/desk_bell.lua

local M = {}

function M.RingDeskBell(attempt)
    local dings = {
        "desk_bell1",
        "desk_bell2"
    }
    delay(250)
    playSound(dings[attempt])
    delay(1500)
end

return M
