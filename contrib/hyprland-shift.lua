-- Optional: Shift for the needle (10x finer seeking, snapping to chapter starts).
-- The widget never takes the keyboard, so Hyprland reports Shift through a small file in
-- $XDG_RUNTIME_DIR; the key still reaches your programs as usual. Add to ~/.config/hypr/hyprland.lua.
local spunShift = (os.getenv("XDG_RUNTIME_DIR") or "/tmp") .. "/spun-shift"
local function setSpunShift(value)
    local f = io.open(spunShift, "w")
    if f then f:write(value); f:close() end
end
-- Raw key events (keycode, time, state): unlike release binds they also report a Shift
-- released after Shift+letter or Alt+Shift. 50 = left Shift, 62 = right Shift.
local shiftDown = {}
local shiftWritten = nil
hl.on("input.keyboard.key", function(keycode, _, state)
    if keycode ~= 50 and keycode ~= 62 then return end
    shiftDown[keycode] = state == 1 or nil
    local value = next(shiftDown) and "1" or "0"
    if value ~= shiftWritten then setSpunShift(value); shiftWritten = value end
end)
setSpunShift("0")
