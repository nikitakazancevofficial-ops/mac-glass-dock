#!/usr/bin/env python3
"""Reversible user-local desktop integration. No sudo or Plasma restart."""
import argparse
import fcntl
import json
import os
from pathlib import Path
import shutil
import shlex
import subprocess
import sys
import tempfile
import time

PROJECT = Path(__file__).resolve().parents[1]
CONFIG = Path(os.environ.get('XDG_CONFIG_HOME', Path.home() / '.config'))
DATA = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local/share'))
STATE = Path(os.environ.get('XDG_STATE_HOME', Path.home() / '.local/state')) / 'mac-glass-dock'
RECORD = STATE / 'desktop-integration.json'
THEME = 'org.macglass.decoration'
GROUP = 'org.kde.kdecoration2'
KEYS = ('library', 'theme', 'ButtonsOnLeft', 'ButtonsOnRight', 'BorderSize', 'BorderSizeAuto')

def run(*args):
    result = subprocess.run(args, text=True, capture_output=True, timeout=15)
    if result.returncode:
        raise RuntimeError(result.stderr.strip() or result.stdout.strip() or f'{args[0]} failed')
    return result.stdout.strip()

def plasma(js):
    value = run('qdbus6', 'org.kde.plasmashell', '/PlasmaShell', 'org.kde.PlasmaShell.evaluateScript', js)
    # Plasma script exceptions may be returned as successful DBus replies.
    if value.startswith(('Error:', 'TypeError:', 'ReferenceError:')):
        raise RuntimeError(value)
    return value

def panels():
    return json.loads(plasma('print(JSON.stringify(panels().map(p=>({id:p.id,location:p.location,height:p.height,screen:p.screen,hiding:p.hiding,widgets:p.widgets().map(w=>({id:w.id,type:w.type}))}))))'))

def atomic(path, content):
    path.parent.mkdir(parents=True, exist_ok=True)
    fd, tmp = tempfile.mkstemp(prefix='.macglass-', dir=path.parent)
    try:
        with os.fdopen(fd, 'w') as file:
            file.write(content)
            file.flush()
            os.fsync(file.fileno())
        os.replace(tmp, path)
    finally:
        if os.path.exists(tmp):
            os.unlink(tmp)

def state():
    return json.loads(RECORD.read_text()) if RECORD.exists() else {'active': False, 'decorated': False}

def save(record):
    atomic(RECORD, json.dumps(record, ensure_ascii=False, indent=2) + '\n')

def backup(record):
    if 'backup' in record:
        return
    folder = STATE / 'original'
    folder.mkdir(parents=True, exist_ok=True)
    for name in ('plasma-org.kde.plasma.desktop-appletsrc', 'plasmashellrc', 'kwinrc'):
        source = CONFIG / name
        if source.exists() and not (folder / name).exists():
            shutil.copy2(source, folder / name)
    record['backup'] = str(folder)
    save(record)

def kread(key):
    return run('kreadconfig6', '--file', str(CONFIG / 'kwinrc'), '--group', GROUP, '--key', key, '--default', '__MACGLASS_UNSET__')

def kwrite(key, value):
    args = ['kwriteconfig6', '--file', str(CONFIG / 'kwinrc'), '--group', GROUP, '--key', key]
    run(*(args + (['--delete'] if value == '__MACGLASS_UNSET__' else [value])))

def quote_exec(path):
    return '"' + str(path).replace('\\', '\\\\').replace('"', '\\"').replace('`', '\\`').replace('$', '\\$').replace('%', '%%') + '"'

def systemd_quote(value):
    return '"' + str(value).replace('\\', '\\\\').replace('"', '\\"').replace('%', '%%') + '"'

def systemd_autostart(enabled):
    unit_name = 'mac-glass-dock-login.service'
    unit = CONFIG / 'systemd/user' / unit_name
    app_name = 'mac-glass-dock.service'
    app_unit = CONFIG / 'systemd/user' / app_name
    command = systemd_quote(sys.executable) + ' ' + systemd_quote(STATE / 'recover.py')
    # Keep the running Dock open when autostart is disabled, but remove its
    # restart policy so an explicit quit/desktop restore remains effective.
    atomic(app_unit, '[Unit]\nDescription=Mac Glass Dock\n'
           'After=graphical-session.target\nPartOf=graphical-session.target\nStartLimitIntervalSec=0\n\n'
           '[Service]\nType=simple\nExecStart=' + command + ' run-dock\n'
           'Restart=' + ('always' if enabled else 'no') + '\nRestartSec=3\nTimeoutStopSec=10\n\n'
           '[Install]\nWantedBy=graphical-session.target\n')
    if enabled:
        atomic(unit, '[Unit]\nDescription=Mac Glass Dock startup and resume recovery\n'
               'After=graphical-session.target\nPartOf=graphical-session.target\nStartLimitIntervalSec=0\n\n'
               '[Service]\nType=simple\nExecStart=' + command + ' supervise\n'
               'Restart=always\nRestartSec=3\nTimeoutStopSec=10\n\n'
               '[Install]\nWantedBy=graphical-session.target\n')
        run('systemctl', '--user', 'daemon-reload')
        run('systemctl', '--user', 'enable', app_name)
        run('systemctl', '--user', 'enable', '--now', unit_name)
    else:
        try:
            run('systemctl', '--user', 'disable', unit_name)
        except RuntimeError:
            pass
        run('systemctl', '--user', 'disable', app_name)
        unit.unlink(missing_ok=True)
        run('systemctl', '--user', 'daemon-reload')
        # Update the app's restart policy before stopping the supervisor: the
        # recovery path may be running inside that very service.
        run('systemctl', '--user', 'stop', '--no-block', unit_name)

def recovery_entry(record):
    startup = CONFIG / 'autostart/org.macglass.Recovery.desktop'
    if 'recoveryOriginal' not in record:
        record['recoveryOriginal'] = startup.read_text() if startup.exists() else None
        save(record)
    # Keep the undo program outside the checkout: recovery must still work if
    # the project is moved or its build directory is cleaned before reboot.
    recovery = STATE / 'recover.py'
    atomic(recovery, Path(__file__).read_text())
    if record.get('autostart'):
        atomic(startup, '[Desktop Entry]\nType=Application\nName=Mac Glass recovery\nHidden=true\n')
        return
    atomic(startup, '[Desktop Entry]\nType=Application\nName=Restore original Plasma panel\n'
           'Exec=' + quote_exec(Path(sys.executable)) + ' ' + quote_exec(recovery) + ' restore-login\n'
           'Terminal=false\nOnlyShowIn=KDE;\nX-KDE-autostart-phase=2\n')

def enable_autostart(record):
    if not (PROJECT / 'build/mac-glass-dock').is_file() or not os.access(PROJECT / 'build/mac-glass-dock', os.X_OK):
        raise RuntimeError('Build mac-glass-dock before enabling autostart')
    entry = CONFIG / 'autostart/org.macglass.Dock.desktop'
    if 'autostartOriginal' not in record:
        record['autostartOriginal'] = entry.read_text() if entry.exists() else None
    # Prepare the entry/helper first. If interrupted before the state commit,
    # the old recovery still restores Plasma instead of leaving no Dock.
    atomic(STATE / 'recover.py', Path(__file__).read_text())
    atomic(entry, '[Desktop Entry]\nType=Application\nName=Mac Glass Dock\n'
           'Comment=Start the user Dock and its glass effect after KDE is ready\n'
           'Exec=' + quote_exec(Path(sys.executable)) + ' ' + quote_exec(STATE / 'recover.py') + ' login\n'
           'Terminal=false\nOnlyShowIn=KDE;\nStartupNotify=false\nX-KDE-autostart-enabled=true\nX-KDE-autostart-phase=2\n'
           'X-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1\nX-MacGlass-Managed=true\n')
    entry.chmod(0o644)
    record['project'] = str(PROJECT)
    record['autostart'] = True
    save(record)
    recovery_entry(record)
    # Keep the standard XDG entry and add KDE's dedicated script hook as a
    # fallback. The login helper is idempotent (flock + dock_ready), so both
    # launch paths may safely fire during the same login.
    script = CONFIG / 'autostart-scripts/org.macglass.Dock'
    if 'autostartScriptOriginal' not in record:
        record['autostartScriptOriginal'] = script.read_text() if script.exists() else None
        record['autostartScriptMode'] = script.stat().st_mode & 0o777 if script.exists() else None
        save(record)
    atomic(script, '#!/bin/sh\nexec ' + shlex.quote(sys.executable) + ' '
           + shlex.quote(str(STATE / 'recover.py')) + ' login\n')
    script.chmod(0o700)
    systemd_autostart(True)

def disable_autostart(record, recover=True):
    if recover and (record.get('active') or record.get('decorated')):
        temporary = dict(record, autostart=False)
        recovery_entry(temporary)
    record['autostart'] = False
    save(record)
    atomic(CONFIG / 'autostart/org.macglass.Dock.desktop',
           '[Desktop Entry]\nType=Application\nName=Mac Glass Dock\nHidden=true\nX-MacGlass-Managed=true\n')
    script = CONFIG / 'autostart-scripts/org.macglass.Dock'
    if 'autostartScriptOriginal' in record:
        original = record['autostartScriptOriginal']
        if original is None:
            script.unlink(missing_ok=True)
        else:
            atomic(script, original)
            script.chmod(record.get('autostartScriptMode') or 0o700)
    systemd_autostart(False)

def dock_ready():
    try:
        value = json.loads(run('qdbus6', 'org.macglass.Dock', '/Dock', 'org.macglass.Dock.opticalState'))
        screen = value.get('dock', {}).get('screen', {})
        return (value.get('pid', 0) > 0 and bool(value.get('dock', {}).get('surface'))
                and screen.get('w', 0) >= 800 and screen.get('h', 0) >= 600)
    except (RuntimeError, ValueError, OSError, subprocess.TimeoutExpired):
        return False

def login(record):
    if record.get('autostart'):
        # All desktop entry/script hooks converge on one supervisor, including
        # KDE's migrated old-autostart-scripts hook. Never detach an untracked Dock.
        run('systemctl', '--user', 'start', '--no-block', 'mac-glass-dock-login.service')

def run_dock(record):
    project = Path(record['project'])
    if not (project / 'build/mac-glass-dock').is_file() or not (project / 'scripts/session.sh').is_file():
        raise RuntimeError('Saved Dock project/build is missing')
    deadline = time.monotonic() + 60
    while True:
        try:
            run('qdbus6', 'org.kde.KWin', '/KWin')
            run('qdbus6', 'org.kde.plasmashell', '/PlasmaShell')
            break
        except RuntimeError:
            if time.monotonic() >= deadline:
                raise
            time.sleep(1)
    # Replace this helper so systemd directly tracks the real Dock process.
    launcher = str(project / 'scripts/session.sh')
    os.execv(launcher, [launcher])

def suspend_time():
    # CLOCK_BOOTTIME includes suspend; monotonic does not. Unlike wall time,
    # this detects wakeup without false restarts after an NTP/timezone change.
    return time.clock_gettime(time.CLOCK_BOOTTIME) - time.monotonic()

def supervise(record):
    if not record.get('autostart'):
        return
    STATE.mkdir(parents=True, exist_ok=True)
    with (STATE / 'supervise.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        slept = suspend_time()
        wait_for_dock(record)
        failures = 0
        while state().get('autostart'):
            time.sleep(2)
            now = suspend_time()
            resumed = now - slept > 1
            slept = now
            failures = 0 if dock_ready() else failures + 1
            if resumed or failures >= 3:
                with (STATE / 'login.log').open('a') as log:
                    log.write('Recreating Dock after resume\n' if resumed else 'Recovering unresponsive Dock\n')
                run('systemctl', '--user', 'restart', '--no-block', 'mac-glass-dock.service')
                # Give the previous DBus owner time to exit before checking the
                # replacement. No restart loop while outputs/KDE settle.
                time.sleep(2)
                wait_for_dock(state())
                failures = 0

def wait_for_dock(record):
    global PROJECT
    if not record.get('autostart'):
        return
    STATE.mkdir(parents=True, exist_ok=True)
    with (STATE / 'login.lock').open('a') as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        with (STATE / 'login.log').open('a', buffering=1) as log:
            permanent_failure = False
            try:
                PROJECT = Path(record['project'])
                if not (PROJECT / 'build/mac-glass-dock').is_file() or not (PROJECT / 'scripts/session.sh').is_file():
                    permanent_failure = True
                    raise RuntimeError('Saved Dock project/build is missing')
                deadline = time.monotonic() + 60
                while True:
                    try:
                        run('qdbus6', 'org.kde.KWin', '/KWin')
                        run('qdbus6', 'org.kde.plasmashell', '/PlasmaShell')
                        break
                    except RuntimeError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(1)
                if not dock_ready():
                    run('systemctl', '--user', 'start', '--no-block', 'mac-glass-dock.service')
                for attempt in range(240):
                    if dock_ready():
                        if record.get('active'):
                            activate(record)  # Preserve the previously chosen left-panel layout.
                        log.write('Dock ready; existing preferences and desktop backup preserved\n')
                        return
                    time.sleep(.25)
                raise RuntimeError('Dock did not become ready')
            except Exception as error:
                log.write('Startup failed: ' + str(error) + '\n')
                # Only a missing checkout is permanent. Crashes and temporary
                # output/KDE loss keep automatic recovery enabled.
                if permanent_failure:
                    restore(record)
                elif record.get('autostart'):
                    if record.get('active'):
                        for panel in record.get('panels', []):
                            plasma(f'var p=panelById({int(panel["id"])}); if(!p) throw new Error("Saved panel missing"); p.location={json.dumps(panel["location"])}; p.hiding={json.dumps(panel["hiding"])}; p.height={int(panel["height"])}; print("ok");')
                else:
                    restore(record)
                raise

def restore_at_login(record):
    # A queued/cached old Recovery.desktop must not race the persistent Dock.
    if record.get('autostart'):
        return
    for attempt in range(30):
        try:
            run('qdbus6', 'org.kde.plasmashell', '/PlasmaShell')
            break
        except RuntimeError:
            time.sleep(1)
    restore(record)

def decorate(record):
    backup(record)
    recovery_entry(record)
    if not record.get('decorated'):
        record['decoration'] = {key: kread(key) for key in KEYS}
        # Save before the first mutation for recovery after interruption.
        record['decorated'] = True
        save(record)
    source = PROJECT / 'decoration'
    target = DATA / 'kwin/decorations' / THEME
    shutil.copytree(source, target, dirs_exist_ok=True)
    for key, value in {'library': 'org.kde.kwin.aurorae', 'theme': THEME,
                       'ButtonsOnLeft': 'XIA', 'ButtonsOnRight': '', 'BorderSize': 'None', 'BorderSizeAuto': 'false'}.items():
        kwrite(key, value)
    run('qdbus6', 'org.kde.KWin', '/KWin', 'org.kde.KWin.reconfigure')

def restore_decoration(record):
    if record.get('decorated'):
        for key, value in record['decoration'].items():
            kwrite(key, value)
        run('qdbus6', 'org.kde.KWin', '/KWin', 'org.kde.KWin.reconfigure')
        record['decorated'] = False
        save(record)

def launchers(record):
    # Desktop Exec quoting is not shell quoting. Escape the four reserved chars.
    quoted = quote_exec(PROJECT / 'scripts/session.sh')
    entry = ('[Desktop Entry]\nType=Application\nName=Mac Glass Dock\nComment=Liquid glass Dock for KDE Plasma\n'
             'Icon=preferences-system-windows\nExec=' + quoted + '\nTerminal=false\nCategories=Utility;\nX-KDE-Wayland-Interfaces=zkde_screencast_unstable_v1\n'
             'Actions=Settings;Restore;\n\n[Desktop Action Settings]\nName=Стекло и Dock\nExec=' + quoted + ' --settings\n'
             '\n[Desktop Action Restore]\nName=Вернуть панель Plasma\nExec=' + quoted + ' --restore-plasma\n')
    atomic(DATA / 'applications/org.macglass.Dock.desktop', entry)
    # Recovery is active only for the temporary (non-autostart) mode.
    recovery_entry(record)

def activate(record):
    # Require an actually running Dock before relocating the user's panel.
    run('qdbus6', 'org.macglass.Dock', '/Dock', 'org.macglass.Dock.opticalState')
    backup(record)
    recovery_entry(record)
    if not record.get('active'):
        bottom = [p for p in panels() if p['location'] == 'bottom' and any(w['type'] in ('org.kde.plasma.icontasks', 'org.kde.plasma.taskmanager') for w in p['widgets'])]
        record['panels'] = bottom
        record['active'] = True
        save(record)
    for p in record.get('panels', []):
        location = record.get('dockLocation', 'left')
        if location not in ('left', 'right', 'top', 'bottom'):
            raise RuntimeError('Invalid saved Dock panel location')
        plasma(f'var p=panelById({int(p["id"])}); if(!p) throw new Error("Saved panel missing"); p.location={json.dumps(location)}; p.hiding="autohide"; p.height=30; print("ok");')
    for expected in record.get('panels', []):
        actual = next((p for p in panels() if p['id'] == expected['id']), None)
        if not actual or actual['location'] != record.get('dockLocation', 'left') or actual['hiding'] != 'autohide' or actual['widgets'] != expected['widgets']:
            raise RuntimeError('Panel verification failed; use restore')
    launchers(record)
    save(record)

def restore(record):
    if record.get('active'):
        for p in record.get('panels', []):
            plasma(f'var p=panelById({int(p["id"])}); if(!p) throw new Error("Saved panel missing"); p.location={json.dumps(p["location"])}; p.hiding={json.dumps(p["hiding"])}; p.height={int(p["height"])}; print("ok");')
        for expected in record.get('panels', []):
            actual = next((p for p in panels() if p['id'] == expected['id']), None)
            if not actual or any(actual[k] != expected[k] for k in ('location', 'height', 'hiding', 'widgets')):
                raise RuntimeError('Restored panel did not match its backup')
        record['active'] = False
        save(record)
    restore_decoration(record)
    if record.get('autostart'):
        disable_autostart(record, recover=False)
    startup = CONFIG / 'autostart/org.macglass.Recovery.desktop'
    if 'recoveryOriginal' in record:
        atomic(startup, record['recoveryOriginal'] or '[Desktop Entry]\nType=Application\nName=Mac Glass recovery\nHidden=true\n')

def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('action', choices=['status', 'activate', 'restore', 'restore-login', 'decorate', 'restore-decoration', 'enable-autostart', 'disable-autostart', 'login', 'run-dock', 'supervise'])
    args = parser.parse_args()
    record = state()
    if args.action == 'activate':
        activate(record)
    elif args.action == 'restore':
        restore(record)
    elif args.action == 'restore-login':
        restore_at_login(record)
    elif args.action == 'enable-autostart':
        enable_autostart(record)
    elif args.action == 'disable-autostart':
        disable_autostart(record)
    elif args.action == 'login':
        login(record)
    elif args.action == 'run-dock':
        run_dock(record)
    elif args.action == 'supervise':
        supervise(record)
    elif args.action == 'decorate':
        decorate(record)
    elif args.action == 'restore-decoration':
        restore_decoration(record)
    print(json.dumps({'active': record.get('active', False), 'decorated': record.get('decorated', False), 'autostart': record.get('autostart', False), 'backup': record.get('backup', '')}, ensure_ascii=False))

if __name__ == '__main__':
    try:
        main()
    except Exception as error:
        try:
            record = state()
            print(json.dumps({'active': record.get('active', False), 'decorated': record.get('decorated', False)}))
        except Exception:
            pass
        print(str(error), file=sys.stderr)
        sys.exit(1)
