#!/usr/bin/env bash
# Read-only, deliberately omits username, hostname, network names and full logs.
set -u
printf '%s\n' 'Mac Glass Dock — compatibility report'
if [[ -r /etc/os-release ]]; then
    (. /etc/os-release; printf 'OS: %s\n' "${PRETTY_NAME:-unknown}")
fi
printf 'Architecture: %s\nSession: %s\nDesktop: %s\n' "$(uname -m)" "${XDG_SESSION_TYPE:-unknown}" "${XDG_CURRENT_DESKTOP:-unknown}"
for tool in kwin_wayland plasmashell; do
    if command -v "$tool" >/dev/null; then "$tool" --version 2>/dev/null; fi
done
if command -v pacman >/dev/null; then
    pacman -Q qt6-base qt6-declarative qt6-shadertools qt6-wayland layer-shell-qt kwindowsystem kwayland kpipewire 2>/dev/null || true
fi
if command -v lspci >/dev/null; then
    printf '\nGPU:\n'
    lspci | rg -i 'VGA compatible|3D controller|Display controller' || true
fi
printf '\nGlass effect status:\n'
if command -v qdbus6 >/dev/null; then
    qdbus6 org.kde.KWin /MacGlassRefraction org.macglass.Refraction.status 2>/dev/null || printf 'Not loaded / not available\n'
else
    printf 'qdbus6 missing\n'
fi
printf '\nReview this output before posting. Add monitor scale and what worked/failed.\n'
