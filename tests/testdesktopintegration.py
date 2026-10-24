"""Exercise rollback and next-login recovery with a private fake Plasma bus."""
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('desktop', Path(__file__).resolve().parents[1] / 'scripts/desktop-session.py')
desktop = importlib.util.module_from_spec(spec)
spec.loader.exec_module(desktop)

class DesktopRecoveryTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = Path(self.temp.name)
        for name in ('CONFIG', 'DATA', 'STATE'):
            replacement = root / name.lower()
            patcher = patch.object(desktop, name, replacement)
            patcher.start(); self.addCleanup(patcher.stop)
        patcher = patch.object(desktop, 'RECORD', desktop.STATE / 'desktop-integration.json')
        patcher.start(); self.addCleanup(patcher.stop)
        self.panel = {'id': 23, 'location': 'bottom', 'height': 30, 'screen': 0,
                      'hiding': 'none', 'widgets': [{'id': 26, 'type': 'org.kde.plasma.icontasks'}, {'id': 28, 'type': 'org.kde.plasma.systemtray'}]}
        self.original = json.loads(json.dumps(self.panel))
        self.keys = {'library': '__MACGLASS_UNSET__', 'theme': '__MACGLASS_UNSET__', 'unrelated': 'keep'}
        def plasma(script):
            if 'p.location="left"' in script:
                self.panel.update(location='left', hiding='autohide')
            elif 'p.location="bottom"' in script:
                self.panel.update(self.original)
            return 'ok'
        for name, replacement in [('run', lambda *args: '{}'), ('panels', lambda: [self.panel.copy()]),
                                   ('plasma', plasma), ('kread', lambda k: self.keys.get(k, '__MACGLASS_UNSET__')),
                                   ('kwrite', lambda k, v: self.keys.__setitem__(k, v))]:
            patcher = patch.object(desktop, name, replacement)
            patcher.start(); self.addCleanup(patcher.stop)

    def test_panel_and_decoration_restore_after_simulated_restart(self):
        record = desktop.state()
        desktop.activate(record)
        desktop.decorate(record)
        self.assertEqual(self.panel['location'], 'left')
        self.assertEqual(self.panel['widgets'], self.original['widgets'])
        # A fresh process reads the durable undo record after a crash/reboot.
        desktop.restore(desktop.state())
        self.assertEqual(self.panel, self.original)
        self.assertEqual(self.keys['library'], '__MACGLASS_UNSET__')
        self.assertEqual(self.keys['unrelated'], 'keep')
        self.assertFalse(desktop.state()['active'])
        self.assertFalse(desktop.state()['decorated'])

    def test_next_login_restores_instead_of_launching_dock(self):
        record = desktop.state()
        desktop.activate(record)
        recovery = desktop.CONFIG / 'autostart/org.macglass.Recovery.desktop'
        self.assertIn('restore-login', recovery.read_text())
        self.assertIn(str(desktop.STATE / 'recover.py'), recovery.read_text())
        self.assertTrue((desktop.STATE / 'recover.py').is_file())
        self.assertFalse((desktop.CONFIG / 'autostart/org.macglass.Dock.desktop').exists())
        desktop.restore(desktop.state())
        self.assertIn('Hidden=true', recovery.read_text())

    def test_repeated_activation_does_not_lose_original_panel(self):
        record = desktop.state()
        desktop.activate(record)
        self.panel['location'] = 'top'  # Migrate a previous active replacement.
        desktop.activate(desktop.state())
        self.assertEqual(self.panel['location'], 'left')
        self.assertEqual(desktop.state()['panels'][0]['location'], 'bottom')
        desktop.restore(desktop.state())
        self.assertEqual(self.panel, self.original)

    def prepare_autostart(self):
        project = desktop.STATE / 'project'
        (project / 'build').mkdir(parents=True)
        (project / 'scripts').mkdir()
        binary = project / 'build/mac-glass-dock'
        binary.write_text('test fixture only\n'); binary.chmod(0o755)
        (project / 'scripts/session.sh').write_text('test fixture only\n')
        patcher = patch.object(desktop, 'PROJECT', project)
        patcher.start(); self.addCleanup(patcher.stop)
        record = desktop.state();desktop.activate(record);desktop.enable_autostart(record)
        return record

    def test_autostart_preserves_layout_and_suppresses_old_recovery(self):
        record = self.prepare_autostart()
        entry = desktop.CONFIG / 'autostart/org.macglass.Dock.desktop'
        self.assertIn(' login\n', entry.read_text())
        self.assertNotIn('Hidden=true', entry.read_text())
        self.assertIn('Hidden=true', (desktop.CONFIG / 'autostart/org.macglass.Recovery.desktop').read_text())
        desktop.restore_at_login(desktop.state())  # queued old recovery is harmless
        self.assertEqual(self.panel['location'], 'left')
        self.assertEqual(record['panels'][0], self.original)
        self.assertTrue(desktop.state()['autostart'])

    def test_disabling_autostart_rearms_temporary_recovery(self):
        self.prepare_autostart()
        desktop.disable_autostart(desktop.state())
        self.assertFalse(desktop.state()['autostart'])
        self.assertIn('Hidden=true', (desktop.CONFIG / 'autostart/org.macglass.Dock.desktop').read_text())
        self.assertIn('restore-login', (desktop.CONFIG / 'autostart/org.macglass.Recovery.desktop').read_text())
        self.assertEqual(self.panel['location'], 'left')
        desktop.restore_at_login(desktop.state())
        self.assertEqual(self.panel, self.original)

    def test_full_restore_also_disables_persistent_dock(self):
        self.prepare_autostart()
        desktop.restore(desktop.state())
        self.assertEqual(self.panel, self.original)
        self.assertFalse(desktop.state()['autostart'])
        self.assertIn('Hidden=true', (desktop.CONFIG / 'autostart/org.macglass.Dock.desktop').read_text())

    def test_login_does_not_duplicate_an_existing_dock(self):
        self.prepare_autostart()
        with patch.object(desktop, 'dock_ready', return_value=True), patch.object(desktop.subprocess, 'Popen') as start:
            desktop.wait_for_dock(desktop.state())
            start.assert_not_called()
        self.assertEqual(self.panel['location'], 'left')
        self.assertTrue(desktop.state()['autostart'])

    def test_missing_build_at_login_restores_plasma(self):
        self.prepare_autostart()
        (desktop.PROJECT / 'build/mac-glass-dock').unlink()  # private temporary fixture
        with self.assertRaisesRegex(RuntimeError, 'missing'):
            desktop.wait_for_dock(desktop.state())
        self.assertEqual(self.panel, self.original)
        self.assertFalse(desktop.state()['autostart'])

    def test_cold_login_starts_saved_project_and_keeps_preferences(self):
        record = self.prepare_autostart()
        preferences = desktop.CONFIG / 'macglass.ini'
        preferences.write_text('user settings stay here\n')
        with patch.object(desktop, 'dock_ready', side_effect=[False, True]), \
             patch.object(desktop, 'run', wraps=desktop.run) as start:
            desktop.wait_for_dock(desktop.state())
            start.assert_any_call('systemctl', '--user', 'start', '--no-block', 'mac-glass-dock.service')
        self.assertEqual(preferences.read_text(), 'user settings stay here\n')
        self.assertTrue(desktop.state()['autostart'])
        self.assertEqual(self.panel['location'], 'left')

    def test_startup_timeout_restores_panel_and_keeps_recovery_enabled(self):
        self.prepare_autostart()
        with patch.object(desktop, 'dock_ready', return_value=False), \
             patch.object(desktop.time, 'sleep'):
            with self.assertRaisesRegex(RuntimeError, 'ready'):
                desktop.wait_for_dock(desktop.state())
        self.assertEqual(self.panel, self.original)
        self.assertTrue(desktop.state()['autostart'])

    def test_transient_kde_readiness_failure_keeps_future_autostart(self):
        self.prepare_autostart()
        with patch.object(desktop, 'run', side_effect=RuntimeError('KDE not ready')), \
             patch.object(desktop.time, 'monotonic', side_effect=[0, 61]), \
             patch.object(desktop.subprocess, 'Popen') as start:
            with self.assertRaisesRegex(RuntimeError, 'KDE not ready'):
                desktop.wait_for_dock(desktop.state())
            start.assert_not_called()
        self.assertEqual(self.panel, self.original)
        self.assertTrue(desktop.state()['autostart'])

    def test_units_track_dock_and_supervisor_and_restart_after_exit(self):
        self.prepare_autostart()
        units = desktop.CONFIG / 'systemd/user'
        app = (units / 'mac-glass-dock.service').read_text()
        supervisor = (units / 'mac-glass-dock-login.service').read_text()
        self.assertIn(' run-dock\n', app)
        self.assertIn(' supervise\n', supervisor)
        for unit in (app, supervisor):
            self.assertIn('Type=simple\n', unit)
            self.assertIn('Restart=always\n', unit)
            self.assertIn('PartOf=graphical-session.target\n', unit)
            self.assertIn('StartLimitIntervalSec=0\n', unit)
        desktop.disable_autostart(desktop.state())
        self.assertIn('Restart=no\n', (units / 'mac-glass-dock.service').read_text())

    def test_desktop_hooks_only_start_one_systemd_supervisor(self):
        record = self.prepare_autostart()
        with patch.object(desktop, 'run') as run:
            desktop.login(record)
            run.assert_called_once_with('systemctl', '--user', 'start', '--no-block', 'mac-glass-dock-login.service')
            run.reset_mock()
            desktop.login(dict(record, autostart=False))
            run.assert_not_called()

    def test_run_dock_replaces_helper_with_saved_launcher(self):
        record = self.prepare_autostart()
        launcher = str(Path(record['project']) / 'scripts/session.sh')
        with patch.object(desktop.os, 'execv') as execute:
            desktop.run_dock(record)
            execute.assert_called_once_with(launcher, [launcher])

    def test_supervisor_recreates_dock_after_suspend(self):
        record = self.prepare_autostart()
        with patch.object(desktop, 'state', side_effect=[record, record, dict(record, autostart=False)]), \
             patch.object(desktop, 'suspend_time', side_effect=[100, 160]), \
             patch.object(desktop, 'dock_ready', return_value=True), \
             patch.object(desktop, 'wait_for_dock') as ready, \
             patch.object(desktop.time, 'sleep'), patch.object(desktop, 'run') as run:
            desktop.supervise(record)
            run.assert_called_once_with('systemctl', '--user', 'restart', '--no-block', 'mac-glass-dock.service')
            self.assertEqual(ready.call_count, 2)

    def test_supervisor_recovers_unresponsive_dock_without_restart_storm(self):
        record = self.prepare_autostart()
        with patch.object(desktop, 'state', side_effect=[record, record, record, record, dict(record, autostart=False)]), \
             patch.object(desktop, 'suspend_time', return_value=100), \
             patch.object(desktop, 'dock_ready', return_value=False), \
             patch.object(desktop, 'wait_for_dock'), \
             patch.object(desktop.time, 'sleep'), patch.object(desktop, 'run') as run:
            desktop.supervise(record)
            run.assert_called_once_with('systemctl', '--user', 'restart', '--no-block', 'mac-glass-dock.service')

    def test_suspend_clock_ignores_elapsed_awake_time(self):
        with patch.object(desktop.time, 'clock_gettime', side_effect=[100, 105, 170]), \
             patch.object(desktop.time, 'monotonic', side_effect=[80, 85, 90]):
            self.assertEqual(desktop.suspend_time(), 20)
            self.assertEqual(desktop.suspend_time(), 20)
            self.assertEqual(desktop.suspend_time(), 80)

    def test_systemd_paths_escape_percent_specifiers(self):
        self.assertEqual(desktop.systemd_quote('/home/test/100% Dock'), '"/home/test/100%% Dock"')

    def test_unresponsive_dbus_counts_as_not_ready(self):
        with patch.object(desktop, 'run', side_effect=desktop.subprocess.TimeoutExpired('qdbus6', 15)):
            self.assertFalse(desktop.dock_ready())

if __name__ == '__main__':
    unittest.main()
