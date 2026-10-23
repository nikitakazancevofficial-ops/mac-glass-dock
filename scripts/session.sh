#!/usr/bin/env bash
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
if [[ "${1:-}" == --restore-plasma ]]; then
    # Login services can become ready after XDG autostart phase 2.
    for attempt in {1..30}; do
        if qdbus6 org.kde.plasmashell /PlasmaShell >/dev/null 2>&1; then break; fi
        sleep 1
    done
    python3 "$project_dir/scripts/desktop-session.py" restore
    qdbus6 org.macglass.Dock /Dock org.macglass.Dock.quitDock || true
    exit 0
fi
if [[ ! -e "$project_dir/build/.macglass-no-refraction" ]] && ! qdbus6 org.macglass.Dock /Dock org.macglass.Dock.opticalState >/dev/null 2>&1; then
    "$project_dir/scripts/refraction.sh" enable || printf 'Glass effect unavailable; using native blur.\n' >&2
fi
exec "$project_dir/build/mac-glass-dock" "$@"
