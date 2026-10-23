--!nonstrict
-- An empty transparent frame. The turntable itself is drawn by the spun-widget program, which
-- reads this widget's position and size from Noctalia's settings and lies on top of it.
-- Noctalia saves widget settings to its file only when the settings menu closes, but it rebuilds
-- this script on every setting change, so the top level runs again with the new values: it hands
-- them to the program at once through <pluginDataDir>/settings.json.
local keys = { "player_color", "record_color", "chapter_color", "progress_color", "chapters", "player", "swap_animation" }

local function encode(value)
  if type(value) == "boolean" then return value and "true" or "false" end
  if type(value) == "number" then return tostring(value) end
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

function update()
  desktopWidget.render(ui.column({ flexGrow = 1 }, {}))
end
