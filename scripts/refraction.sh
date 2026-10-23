#!/usr/bin/env bash
# Session-only management of our own KWin effect; never writes kwinrc/autostart.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
effect_build="$project_dir/build-kwin"
effect_home="${XDG_DATA_HOME:-$HOME/.local/share}/mac-glass-dock/refraction"
action="${1:-status}"
effect_status() { qdbus6 org.kde.KWin /MacGlassRefraction org.macglass.Refraction.status; }
validate_module() {
    [[ -s "$1" ]]
    local dependencies
    dependencies="$(ldd -r -- "$1" 2>&1)"
    if rg -q 'undefined symbol:|not found' <<<"$dependencies"; then
        printf '%s\n' "$dependencies" >&2
        return 1
    fi
}
case "$action" in
    build)
        cmake -S "$project_dir/kwin" -B "$effect_build" -DCMAKE_BUILD_TYPE=Release -DMACGLASS_NESTED_TEST_CAPTURE=OFF
        cmake --build "$effect_build" --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-1}"
        ;;
    install)
        # QA builds contain test readbacks and must never enter the live session.
        rg -q '^MACGLASS_NESTED_TEST_CAPTURE:BOOL=OFF$' "$effect_build/CMakeCache.txt"
        effect_file="$effect_build/plugins/kwin/effects/plugins/macglassrefraction.so"
        validate_module "$effect_file"
        read -r effect_digest _ < <(sha256sum "$effect_file" "$effect_build/native/libmacglassbootstrap.so" "$effect_build/native/qmldir" "$effect_build/bootstrap.qml" "$effect_build/reload.qml" "$effect_build/register.qml" | sha256sum)
        release_dir="$effect_home/releases/${effect_digest:0:20}"
        if [[ ! -e "$release_dir" ]]; then
            mkdir -p "$release_dir/plugins/kwin/effects/plugins" "$release_dir/native"
            cp -- "$effect_file" "$release_dir/plugins/kwin/effects/plugins/macglassrefraction.so"
            cp -- "$effect_build/native/libmacglassbootstrap.so" "$effect_build/native/qmldir" "$release_dir/native/"
            cp -- "$effect_build/bootstrap.qml" "$release_dir/bootstrap.qml"
            cp -- "$effect_build/reload.qml" "$release_dir/reload.qml"
            cp -- "$effect_build/register.qml" "$release_dir/register.qml"
        fi
        mkdir -p "$effect_home/native"
        if [[ ! -f "$effect_home/native/libmacglassbootstrap.so" ]]; then
            cp -- "$effect_build/native/libmacglassbootstrap.so" "$effect_build/native/qmldir" "$effect_home/native/"
        fi
        cmp -- "$effect_file" "$release_dir/plugins/kwin/effects/plugins/macglassrefraction.so"
        [[ ! -e "$effect_home/current" || -L "$effect_home/current" ]]
        ln -sfn -- "$release_dir" "$effect_home/current"
        printf 'Installed: %s\n' "$release_dir"
        ;;
    enable)
        release_dir="$(readlink -f -- "$effect_home/current")"
        [[ -f "$release_dir/bootstrap.qml" ]]
        # Validate before unloading the currently working compositor module.
        validate_module "$release_dir/plugins/kwin/effects/plugins/macglassrefraction.so"
        # Versioned absolute paths avoid Qt's retained plugin factory cache.
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect macglassrefraction
        qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript macglass-optics-user-loader >/dev/null
        qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript macglass-optics-type-loader >/dev/null
        primer_id="$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadDeclarativeScript "$release_dir/register.qml" macglass-optics-type-loader)"
        [[ "$primer_id" =~ ^[0-9]+$ ]]
        qdbus6 org.kde.KWin "/Scripting/Script$primer_id" org.kde.kwin.Script.run
        script_id="$(qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.loadDeclarativeScript "$release_dir/reload.qml" macglass-optics-user-loader)"
        [[ "$script_id" =~ ^[0-9]+$ ]]
        qdbus6 org.kde.KWin "/Scripting/Script$script_id" org.kde.kwin.Script.run
        for attempt in {1..30}; do
            if [[ "$(effect_status 2>/dev/null)" == "ready:v1" ]]; then effect_status;exit 0; fi
            sleep .1
        done
        exit 1
        ;;
    disable)
        qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect macglassrefraction
        qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript macglass-optics-user-loader
        qdbus6 org.kde.KWin /Scripting org.kde.kwin.Scripting.unloadScript macglass-optics-type-loader
        ;;
    status)
        effect_status
        qdbus6 org.kde.KWin /MacGlassRefraction org.macglass.Refraction.diagnostics
        ;;
    *) printf 'Usage: %s build|install|enable|disable|status\n' "$0" >&2; exit 2 ;;
esac
