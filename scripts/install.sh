#!/usr/bin/env bash
# Builds and installs Spun Widget for the current user: the program into ~/.local (or $PREFIX)
# and the Noctalia plugin into Noctalia's plugin directory (skip it with --no-plugin).
set -euo pipefail
root="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
prefix="${PREFIX:-$HOME/.local}"
plugins="${XDG_DATA_HOME:-$HOME/.local/share}/noctalia/plugins"

cmake -S "$root" -B "$root/build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix"
cmake --build "$root/build" --parallel
cmake --install "$root/build"

if [[ "${1:-}" != "--no-plugin" ]]; then
    mkdir -p "$plugins"
    rm -rf "$plugins/spun-widget"
    cp -r "$root/spun-widget" "$plugins/spun-widget"
fi

cat <<EOF

Installed: $prefix/bin/spun-widget

Next:
  1. Enable the plugin:   noctalia msg plugins enable agent00k0t/spun-widget
  2. Add the "Spun Widget" desktop widget in Noctalia's widget editor and place it.
  3. Start the player now and with Hyprland, e.g. in hyprland.lua:
       hl.exec_cmd("$prefix/bin/spun-widget")
     or in hyprland.conf:
       exec-once = $prefix/bin/spun-widget
  4. Optional, Shift for the needle: add contrib/hyprland-shift.lua to hyprland.lua.
EOF
