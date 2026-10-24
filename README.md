<div align="center">

<img src="docs/media/banner.svg" alt="Mac Glass Dock — glass that follows your desktop" width="100%" />

**A floating glass dock for KDE Plasma on Wayland.**<br>
Live desktop refraction · spring magnification · Genie animations · a glass app launcher

[English](README.md) · [Русский](README.ru.md) · [Install](#install-on-arch-linux) · [Screenshots](#a-closer-look) · [Feedback](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new/choose)

![Release](https://img.shields.io/badge/first_public_preview-0.4.0-80d8c2?style=flat-square)
![Platform](https://img.shields.io/badge/KDE_Plasma_6-Wayland-82b5f5?style=flat-square)
![License](https://img.shields.io/badge/free_for_personal_use-noncommercial-d7c5a5?style=flat-square)
[![GitHub stars](https://img.shields.io/github/stars/nikitakazancevofficial-ops/mac-glass-dock?style=flat-square)](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/stargazers)

<img src="docs/media/tahoe-launcher.webp" alt="Real desktop screenshot: the glass application launcher and floating dock on KDE Plasma" width="100%" />

*Actual application capture in an isolated KDE Wayland session. Tahoe wallpaper © Apple; sample app catalogue. No generated UI mockup.*

**If you like the project, a ⭐ helps other Linux users discover it. Thank you!**

</div>

**Update — 24 October 2026:** Fixed automatic startup after login and reboot, added automatic restart after crashes and recovery after sleep. Reboot startup has been verified on the author's laptop. Existing users should refresh their autostart configuration: [update instructions](docs/RELEASE_NOTES.md).

**Update — 23 October 2026:** Fixed volume control in Control Center. The slider now correctly changes the system output volume. Pull the latest code, rebuild and restart Dock to get the fix. [Details](docs/RELEASE_NOTES.md).

## Why this dock?

I wanted a Linux dock that feels like a piece of glass: a rounded, illuminated edge, individual icon lenses, and a desktop that bends through the material as it moves. Mac Glass Dock is my take on that idea, built with C++, Qt Quick and a custom KWin effect.

This is the **first public preview** (development version **0.4.0**). It is free for personal, noncommercial use. There is more to polish, and your reports will help decide what comes next.

## What you get

| | |
| :-- | :-- |
| **Live glass** | The KWin module samples the composited background behind Dock surfaces and refracts it locally, with soft blur, edge highlights and separate icon lenses. |
| **Motion with character** | Smooth spring magnification, a launch bounce, and Genie transitions between supported windows and their dock icons. The launcher opens from its grid button. |
| **Your apps, one place** | Search installed XDG applications, browse categories, and pin favourites. Icons come from your installed system theme. |
| **A glass Control Center** | Wi-Fi, Bluetooth, volume, display brightness and keyboard backlight controls, plus battery and clock. Availability depends on your hardware and system services. |
| **Make it yours** | Live material settings, three icon styles, presets, autohide, screen selection and Reduced Motion. |
| **Plasma integration** | Optional running-window indicators, window actions, live previews, autostart and a reversible panel replacement. |

The author's laptop has working **screen brightness and keyboard backlight** controls. Please report how they behave on yours. Focus, media and screen-mirroring tiles are still early UI; do not assume every visible tile has a complete backend.

## Install on Arch Linux

**Start here:** log into **KDE Plasma 6 → Wayland**. The main Dock needs **Qt 6.8+**; the full refraction module currently requires **KWin exactly 6.8.0**, **Qt 6.11+** and OpenGL. The C++ compiler must support C++17 for Dock and C++23 for the module.

> **Check the version before expecting full glass.** Arch is a rolling distribution; newer KWin releases may already be installed. The installer checks the SDK and uses basic native blur on unsupported versions. It does not downgrade your desktop. Support for more versions is planned.

### 1. Install dependencies

On an existing Arch / CachyOS KDE desktop:

```bash
sudo pacman -Syu --needed base-devel git cmake ninja python ripgrep pkgconf \
  qt6-base qt6-declarative qt6-shadertools qt6-svg qt6-wayland qt6-tools \
  layer-shell-qt kwindowsystem kservice kio kwayland breeze-icons \
  kconfig kcoreaddons libepoxy libdrm wayland vulkan-headers kpipewire
```

If the upgrade includes Qt, Plasma or KWin, **log out and back in** before starting the effect, so the running compositor matches the installed libraries.

### 2. Download and install

Choose a folder you can keep; the launcher and autostart use this checkout.

```bash
git clone https://github.com/nikitakazancevofficial-ops/mac-glass-dock.git
cd mac-glass-dock
./scripts/install.sh
```

### 3. Start

Open **Mac Glass Dock** in your application menu, or run:

```bash
~/.local/bin/mac-glass-dock
```

That's the entire setup. The commands take about a minute to follow; downloads and compilation can take longer depending on your computer. The default build uses one job to limit memory use. With enough RAM, use `CMAKE_BUILD_PARALLEL_LEVEL=2 ./scripts/install.sh`.

<details>
<summary><strong>Installation options and Control Center dependencies</strong></summary>

```bash
./scripts/install.sh --check   # Read-only prerequisite check
./scripts/install.sh --basic   # Dock with native blur; skip the KWin module
./scripts/install.sh --glass   # Require the exact supported KWin SDK
```

Installation creates `~/.local/bin/mac-glass-dock`, an application-menu entry and, on the supported SDK, user-local effect files. It does not start Dock, enable autostart or move your Plasma panel. No sudo is needed after installing system packages.

Control Center uses the services you already run: `nmcli` (NetworkManager), `bluetoothctl` (bluez-utils), `wpctl` (WirePlumber), `upower`, `kscreen-doctor` (libkscreen) and KDE PowerDevil. Install missing optional tools only for the features you need; installing a package does not configure its service. Brightness/backlight controls require compatible hardware. Live window previews also use Plasma's task-manager QML module and KPipeWire; if unavailable, titles and icons remain available.

</details>

## First minute with Dock

- **Move to the bottom edge** to reveal an automatically hidden Dock.
- **Click the grid button** to open apps. Type to search; right-click an app for available actions and pinning.
- **Click the sliders button** at the right for settings. Try the **«Со скринов»** preset as a starting point, then adjust the material to taste. The author’s original appearance values are in [config/screenshots.ini](config/screenshots.ini).
- Enable **running applications / window integration** in settings for window actions. Enable **window animations** for Genie effects when the KWin module is available.
- **Hover over an app for about two seconds** for its window picker. Live previews depend on Plasma/PipeWire support.
- Click the **top-right status area** for Control Center. **Escape** closes popups.

The current application interface is primarily Russian. **Both guides and issue forms support English and Russian**; complete English UI localization is on the roadmap.

## What “real glass” means

The full effect operates **inside the compositor**: it reads pixels already being composed behind our surfaces, then displaces and softly blurs those pixels through rounded lenses. Desktop changes remain visible through the glass, including during the launcher's compositor-driven Genie transition.

That is real-time **background refraction**, rather than a wallpaper image painted into a translucent rectangle. The rounded rim, Fresnel-style highlights and chromatic edges give the material depth. It is an artistic real-time glass effect, **not a physically complete optical simulation** or Apple's implementation.

Without the KWin module, Dock still runs with native KWin blur and a Qt material fallback. **The fallback does not provide the same desktop refraction.** Software rendering also reduces the visual effects. [Technical details →](docs/REFRACTION.md)

## Animations

| Interaction | Behaviour |
| :-- | :-- |
| Hover | Neighbouring icons magnify with Gaussian falloff and spring smoothing. |
| Launch | A bounded icon bounce indicates a launch request. |
| Open / close / minimize / restore | With window integration and the KWin effect, matched windows bend toward/from their icon with a two-phase Genie deformation. |
| Application launcher | A Genie transition grows from the grid button; the compositor path keeps the refracted background live while moving. |
| Reduced Motion | Disables large transitions and reduces movement. |

Window-to-icon matching, third-party KWin effects and unusual window types can affect animations. Unmatched windows use a simpler fallback. Screenshots below show the actual material; they do not demonstrate timing or frame rate.

## See it move

![Recorded Genie launcher animation on a Tahoe background](docs/media/genie-demo.gif)

*Real compositor capture in an isolated demo session. Animation speed is deliberately set to 0.45 for visibility; this compressed GIF is not a performance benchmark. The background is a static Tahoe image, not a live-wallpaper feature of Dock.*

## A closer look

### Desktop

[![Floating glass dock on the desktop](docs/media/tahoe-desktop.webp)](docs/media/tahoe-desktop.webp)

### Control Center

[![Control Center with glass tiles and hardware controls](docs/media/tahoe-control-center.jpg)](docs/media/tahoe-control-center.jpg)

### Application launcher

[![Application launcher with search and categories](docs/media/tahoe-launcher.webp)](docs/media/tahoe-launcher.webp)

<details>
<summary>More screenshots from the author's everyday desktop</summary>

![Author's desktop](docs/media/desktop.webp)
![Author's Control Center](docs/media/control-center.webp)
![Author's launcher](docs/media/launcher.webp)

</details>

The Tahoe background is Apple artwork, shown for demonstration; [wallpaper source](https://512pixels.net/projects/default-mac-wallpapers-in-5k/). It is not installed by Dock or distributed as a standalone wallpaper.

Click a screenshot to view the full-size image. Application logos and wallpaper visible in screenshots belong to their respective owners; they are not a bundled icon theme or wallpaper pack.

## Autostart and your Plasma panel

**Autostart is optional.** Turn it on in Dock settings, or run from the project folder:

```bash
python3 scripts/desktop-session.py enable-autostart
python3 scripts/desktop-session.py status
```

The helper waits for KDE and starts the installed glass module before Dock when full glass was installed. The XDG/KDE autostart hooks start one user systemd supervisor. A separate service tracks Dock and automatically restarts it after an exit or crash; the supervisor recreates it after waking from sleep so its Wayland surfaces reconnect to the restored displays. Both services stop with the graphical session. You do not need to configure these manually.

**Keep the project folder and `build/mac-glass-dock` in place.** After moving it, rerun installation and enable autostart from the new location. If the saved build is missing, the helper attempts to restore the original Plasma panel and disables its startup. Temporary KDE/display failures and crashes keep automatic recovery enabled. Disabling autostart removes the restart policy and future startup while leaving the current Dock open.

The **«Сделать основным Dock»** setting moves an existing bottom task panel to the left, preserving its widgets. **«Вернуть панель Plasma»** restores it. Panel replacement is separate from installation. Without persistent autostart, temporary panel replacement is restored at the next login.

```bash
# Disable future autostart, keeping the current Dock open:
python3 scripts/desktop-session.py disable-autostart

# Restore the original panel/decoration, disable autostart and quit Dock:
./scripts/session.sh --restore-plasma
```

Recovery files and backups live in `${XDG_STATE_HOME:-~/.local/state}/mac-glass-dock/`. The optional Soft Corners window decoration affects server-side titlebars; apps with their own decorations keep their own controls.

## Compatibility and current limits

| Environment | Status |
| :-- | :-- |
| Author's Arch-based / CachyOS KDE Wayland laptop, KWin 6.8.0, Qt 6.12 | Development/test environment; full glass path available. |
| Arch Linux + KDE Plasma 6 + Wayland | Initial target. Exact library/compositor versions still matter. |
| Other KWin versions | Basic Dock can be tried; the refraction module deliberately rejects a different ABI. |
| Other distributions with KDE Wayland | Not validated yet. Please submit a system report. |
| GNOME, Hyprland, Sway, X11, Windows, macOS | Unsupported in this preview. |

Mixed-DPI monitors, hotplug, HDR, GPU/driver differences and long-session performance need more testing. There is no universal FPS or battery-life claim. This is an early preview; keep your regular panel available until you are happy with your setup.

## Tell me how it works on your system

**English or Russian is welcome.** [Send feedback](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new/choose), [report a bug](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new?template=bug_report.yml), or [request support for your system](https://github.com/nikitakazancevofficial-ops/mac-glass-dock/issues/new?template=compatibility.yml).

Please include your distro, Plasma/KWin/Qt versions, GPU/driver, Wayland session and monitor scale. This read-only command helps gather the basics:

```bash
./scripts/system-info.sh
```

It does not upload anything. Review its output before posting, and remove private details from screenshots or logs. Tell me what works as well as what fails — **successful reports help build a useful compatibility list**.

If this is the kind of Linux desktop you want to see more of, **star the repository and share a link**. Stars are optional; using the project is free for personal purposes.

## What's next

- Broader KWin/version compatibility and simpler packaging.
- More testing on different GPUs, monitors and distributions.
- Better app/window matching, keyboard navigation and performance.
- Complete English interface localization and accessibility improvements.
- Further glass and animation polish.
- **File-manager work is planned too.** It is future work, not a file manager included in this release; the folder icon currently opens your installed file manager.

I will keep improving the project. Suggestions and clear bug reports are very welcome. [Roadmap →](docs/ROADMAP.md) · [Contributing →](CONTRIBUTING.md)

## Build, update and remove

```bash
# Update an existing checkout:
git pull --ff-only
./scripts/install.sh
```

Quit Dock before updating, then start it again. After a KWin upgrade, log back in and check compatibility before reusing the effect.

For development/testing, use `./scripts/build.sh --tests` and `ctest --test-dir build --output-on-failure`. Tests use private XDG paths; offscreen tests do not verify real compositor output. [Development notes →](docs/DEVELOPMENT.ru.md)

To remove it, first run `./scripts/session.sh --restore-plasma`, then `./scripts/refraction.sh disable` if the effect is loaded. Remove the managed launcher `~/.local/bin/mac-glass-dock`, `${XDG_DATA_HOME:-~/.local/share}/applications/org.macglass.Dock.desktop`, and the project folder when no longer needed. The module is under `${XDG_DATA_HOME:-~/.local/share}/mac-glass-dock/refraction/`. Keep recovery backups until your original desktop is restored. System dependency packages are shared with KDE; do not remove them blindly.

## License

**Free for personal, noncommercial use. Commercial use is not permitted.** You may inspect, modify for personal use, and share noncommercial copies under the same conditions, keeping attribution and the license. See the [full license](LICENSE) and [dependency notices](THIRD_PARTY_NOTICES.md).

This is **source-available software with a personal-use restriction**, not an OSI open-source license. Independently installed Qt/KDE and other dependencies retain their own licenses. Not affiliated with Apple or KDE.
