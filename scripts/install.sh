#!/usr/bin/env bash
# User-local source installation. System dependencies are installed separately.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
mode=auto
check_only=false
for option in "$@"; do
    case "$option" in
        --basic) mode=basic ;;
        --glass) mode=glass ;;
        --check) check_only=true ;;
        --help|-h)
            printf 'Usage: %s [--basic|--glass] [--check]\n' "$0"
            printf 'Default: build Dock; also build glass when the installed KWin SDK is 6.8.0.\n'
            printf '%s\n' '--basic: skip the KWin module. --glass: require the supported SDK.'
            printf '%s\n' '--check: inspect prerequisites without changing anything.'
            exit 0 ;;
        *) printf 'Unknown option: %s\n' "$option" >&2; exit 2 ;;
    esac
done
if (( EUID == 0 )); then
    printf 'Run as your desktop user, without sudo. See README for dependency installation.\n' >&2
    exit 1
fi
missing=()
for tool in cmake c++ make python3 qdbus6 rg pkg-config; do
    command -v "$tool" >/dev/null || missing+=("$tool")
done
if (( ${#missing[@]} )); then
    printf 'Missing commands: %s\nInstall the Arch packages listed in README, then retry.\n' "${missing[*]}" >&2
    exit 1
fi
abi_header="${MACGLASS_KWIN_ABI_HEADER:-/usr/include/kwin/config-kwin.h}"
compatible=false
if [[ -r "$abi_header" ]] && rg -q '^#define KWIN_PLUGIN_VERSION_STRING "6\.8\.0"$' "$abi_header"; then
    compatible=true
fi
if [[ "$mode" == glass && "$compatible" == false ]]; then
    printf 'Full glass requires the KWin 6.8.0 SDK and Qt >= 6.11.\nUse --basic for native blur, or report your system through GitHub Issues.\n' >&2
    exit 1
fi
if [[ "$mode" == auto && "$compatible" == false ]]; then
    printf 'KWin SDK differs from 6.8.0: installing basic Dock (native blur, no desktop refraction).\n'
fi
if [[ "$check_only" == true ]]; then
    printf 'Required commands: OK. KWin 6.8.0 SDK available: %s.\n' "$compatible"
    printf 'CMake will check Qt/KDE library versions during the build. Nothing changed.\n'
    exit 0
fi
"$project_dir/scripts/build.sh"
glass=false
if [[ "$mode" != basic && "$compatible" == true ]]; then
    "$project_dir/scripts/refraction.sh" build
    "$project_dir/scripts/refraction.sh" install
    glass=true
fi
if [[ "$glass" == true ]]; then
    rm -f -- "$project_dir/build/.macglass-no-refraction"
else
    touch "$project_dir/build/.macglass-no-refraction"
fi
export MACGLASS_INSTALL_PROJECT="$project_dir"
export MACGLASS_INSTALL_GLASS="$glass"
python3 - <<'PY'
import os
from pathlib import Path
import shlex

project = Path(os.environ['MACGLASS_INSTALL_PROJECT'])
bin_dir = Path.home() / '.local/bin'
data_dir = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
bin_dir.mkdir(parents=True, exist_ok=True)
launcher = bin_dir / 'mac-glass-dock'
if launcher.is_symlink() or (launcher.exists() and 'MacGlass-Managed' not in launcher.read_text()):
    raise SystemExit(f'Refusing to overwrite an unrelated launcher: {launcher}')
launcher.write_text('#!/bin/sh\n# MacGlass-Managed\nexec ' + shlex.quote(str(project / 'scripts/session.sh')) + ' "$@"\n')
launcher.chmod(0o755)

def desktop_quote(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%') + '"'

entry = data_dir / 'applications/org.macglass.Dock.desktop'
entry.parent.mkdir(parents=True, exist_ok=True)
if entry.is_symlink() or (entry.exists() and 'X-MacGlass-Managed=true' not in entry.read_text()):
    raise SystemExit(f'Refusing to overwrite an unrelated desktop entry: {entry}')
entry.write_text('[Desktop Entry]\nType=Application\nName=Mac Glass Dock\n'
                 'Comment=A glass dock for KDE Plasma on Wayland\n'
                 'Comment[ru]=Стеклянный Dock для KDE Plasma на Wayland\n'
                 'Exec=' + desktop_quote(launcher) + '\n'
                 'Icon=preferences-desktop-theme\nTerminal=false\nCategories=Utility;\n'
                 'OnlyShowIn=KDE;\nStartupNotify=false\n'
                 'X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1\nX-MacGlass-Managed=true\n')
print(f'Installed launcher: {launcher}')
print(f'Application menu: {entry}')
print(f'Glass module installed: {os.environ["MACGLASS_INSTALL_GLASS"]}')
PY
printf '\nReady. Start from the application menu or run:\n  ~/.local/bin/mac-glass-dock\n'
printf 'Enable autostart in Dock settings if wanted. Keep this project folder in place.\n'
