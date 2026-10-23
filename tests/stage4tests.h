#pragma once
class QQuickWindow;
class DockSettings;
class ApplicationModel;
class ApplicationCatalogue;
class WindowTracker;
class DockPlatform;
int runStage4Tests(QQuickWindow *,DockSettings *,ApplicationModel *,ApplicationCatalogue *,WindowTracker *);
int runControlCenterTests(QQuickWindow *,DockPlatform *);
