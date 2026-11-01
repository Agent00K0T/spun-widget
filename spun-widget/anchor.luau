--!nonstrict
-- An empty transparent frame. The turntable itself is drawn by the spun-widget program, which
-- reads this widget's position and size from Noctalia's settings and lies on top of it.
-- Noctalia saves widget settings to its file only when the settings menu closes, but it rebuilds
-- this script on every setting change, so the top level runs again with the new values: it hands
-- them to the program at once through <pluginDataDir>/settings.json.
local keys = { "player_color", "record_color", "chapter_color", "progress_color", "chapters", "players", "ignored_players", "swap_animation" }

local function encode(value)
  if type(value) == "boolean" then return value and "true" or "false" end
  if type(value) == "number" then return tostring(value) end
  if type(value) == "table" then
    local items = {}
    for _, item in ipairs(value) do table.insert(items, encode(item)) end
    return "[" .. table.concat(items, ",") .. "]"
  end
  return '"' .. tostring(value):gsub('\\', '\\\\'):gsub('"', '\\"') .. '"'
end

local function publish()
  local parts = {}
  for _, key in ipairs(keys) do
    local value = noctalia.getConfig(key)
    if value ~= nil then table.insert(parts, '"' .. key .. '":' .. encode(value)) end
  end
  local dir = noctalia.pluginDataDir()
  noctalia.mkdirAll(dir)
  noctalia.writeFile(dir .. "/settings.json", "{" .. table.concat(parts, ",") .. "}")
end

publish()

-- While Noctalia's widget editor is open this frame shows a snapshot of the player (which stays
-- under the editor, or hides once the frame is moved away), so it moves and resizes with the frame like the built-in widgets. The program names the
-- current snapshot in <pluginDataDir>/preview.txt (read here when the script is rebuilt, e.g. after
-- a setting change) and sends a new one with the "preview" IPC event; an empty path = no snapshot.
local preview = (noctalia.readFile(noctalia.pluginDataDir() .. "/preview.txt") or ""):match("^%s*(.-)%s*$")

function update()
  if preview ~= "" then
    -- 440 = the housing in the player's own units; Noctalia fits it into the frame
    desktopWidget.render(ui.image({ path = preview, width = 440, height = 440, fit = "contain" }))
  else
    desktopWidget.render(ui.column({ flexGrow = 1 }, {}))
  end
end

function onIpc(event, payload)
  if event == "preview" then
    preview = payload or ""
    update()
  end
end
