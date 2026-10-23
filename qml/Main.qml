pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import MacGlassDock

Window {
    id: window
    objectName: "dockWindow"
    required property DockPlatform platform
    required property DockSettings settings
    required property ApplicationModel applications
    required property ApplicationCatalogue catalogue
    required property WindowTracker windows
    property var controlCenterState
    property bool previewMode: false
    property real hideProgress: 0
    property string hoveredApplication: ""
    readonly property bool compact: hideProgress > .999
    readonly property bool shouldAutoHide: settings.autoHide && (!settings.smartAutoHide || platform.obstructed || (windows.activeWindowExpanded && windows.expandedOn(Qt.rect(screen.virtualX,screen.virtualY,screen.width,screen.height))))
    onShouldAutoHideChanged: if(!shouldAutoHide) hideProgress=0
    function revealDock() { hideProgress=0 }
    function openControlCenter() { controlCenter.expanded = true }
    width: dock.width + 48
    height: compact ? 2 : dock.height + 48
    visible: false
    color: "transparent"
    flags: previewMode ? Qt.Window : Qt.Window | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    title: "Mac Glass Dock"

    Dock {
        id: dock
        settings: window.settings
        applications: window.applications
        platform: window.platform
        tooltipsSuppressed: windowSwitcher.visible || launcher.visible || settingsPanel.visible || appMenu.visible
        baseSize: Math.min(window.settings.iconSize, Math.max(12, (window.screen.width * 0.90 - 96 - (window.applications.count + 1) * window.settings.spacing) / Math.max(1, window.applications.count + 2) / window.settings.maxScale))
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: -window.hideProgress * (dock.height + 48)
        opacity: 1 - window.hideProgress
        onGeometryUpdated: Qt.callLater(window.updateRegion)
        onLauncherRequested: window.openLauncher()
        onSettingsRequested: window.openSettings()
        onAppHoverChanged: (id,inside)=> {if(inside)window.hoveredApplication=id;else if(window.hoveredApplication===id)window.hoveredApplication=""}
        onWindowsRequested: (id,name,icon)=> {
            if(!launcher.visible && !settingsPanel.visible && !appMenu.visible) windowSwitcher.present(id,name,icon)
        }
        onAppContextRequested: (id, name, pinned) => {
            windowSwitcher.dismiss();
            launcher.dismiss();
            settingsPanel.dismiss();
            appMenu.desktopId = id;
            appMenu.appName = name;
            appMenu.pinned = pinned;
            appMenu.open();
        }
    }

    function openLauncher() {
        if (launcher.visible) { launcher.dismiss(); return }
        windowSwitcher.dismiss();
        hideProgress = 0;
        settingsPanel.dismiss();
        appMenu.dismiss();
        launcher.open();
    }
    function openSettings() {
        windowSwitcher.dismiss();
        hideProgress = 0;
        launcher.dismiss();
        appMenu.dismiss();
        settingsPanel.open();
    }
    LauncherWindow {
        id: launcher
        settings: window.settings
        platform: window.platform
        catalogue: window.catalogue
        sourceDockPoint: dock.mapToItem(window.contentItem,dock.launcherAnchor.x,dock.launcherAnchor.y)
        onSettingsRequested: window.openSettings()
    }
    SettingsWindow {
        id: settingsPanel
        settings: window.settings
        platform: window.platform
        windows: window.windows
    }
    AppMenuWindow {
        id: appMenu
        settings: window.settings
        platform: window.platform
        windows: window.windows
        applications: window.applications
        onSettingsRequested: window.openSettings()
    }
    ControlCenterWindow {
        id: controlCenter
        platform: window.platform
        settings: window.settings
        windows: window.windows
        telemetry: window.controlCenterState
    }
    WindowSwitcher {
        id: windowSwitcher
        settings: window.settings
        platform: window.platform
        windows: window.windows
        anchorHovered: window.hoveredApplication===desktopId
    }
    GlassPanelWindow {
        id: errorPanel
        objectName: "errorPanel"
        settings: window.settings
        platform: window.platform
        kind: "menu"
        width: 450
        height: 270
        closeOnDeactivate: false
        Column {
            anchors.fill: parent
            spacing: 16
            Text {
                style: Text.Outline
                styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                renderType: Text.QtRendering
                text: "Не удалось запустить"
                color: window.settings.systemDark ? "#edf5ff" : "#172b42"
                font.pixelSize: 20
            }
            Text {
                style: Text.Outline
                styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                renderType: Text.QtRendering
                width: parent.width
                text: window.applications.lastError
                color: window.settings.systemDark ? "#edf5ff" : "#172b42"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            GlassButton {
                settings: window.settings
                text: "Закрыть"
                onClicked: errorPanel.dismiss()
            }
        }
    }
    Connections {
        target: window.applications
        function onRuntimeError() {
            launcher.dismiss();
            errorPanel.open();
        }
    }
    Behavior on hideProgress {
        NumberAnimation {
            duration: window.settings.reducedMotion ? 0 : 230 / window.settings.animationSpeed
            easing.type: Easing.InOutCubic
        }
    }
    Timer {
        interval: 900
        running: window.shouldAutoHide && !window.previewMode && !dock.pointerInside && !window.platform.edgeHovered && !launcher.visible && !settingsPanel.visible && !appMenu.visible && !errorPanel.visible && !windowSwitcher.visible
        onTriggered: window.hideProgress = 1
    }
    HoverHandler {
        onHoveredChanged: {
            if(hovered && window.compact) window.revealDock()
            if(!hovered) dock.cancelHover()
        }
    }
    Connections {
        target: window.platform
        function onRevealRequested() {window.revealDock()}
        function onPointerOutsideDock() {dock.cancelHover()}
    }
    Connections {
        target: window.settings
        function onChanged() {
            if (!window.settings.autoHide)
                window.hideProgress = 0;
        }
    }
    onHideProgressChanged: Qt.callLater(updateRegion)

    function updateRegion() {
        platform.setHidden(compact);
        if (hideProgress > .98) {
            platform.updateRegion(Qt.rect(0, height - 2, width, 2), 0, []);
            return;
        }
        let rect = dock.surfaceRect;
        let icons = dock.iconRects.map(r => Qt.rect(r.x + dock.x, r.y + dock.y, r.width, r.height));
        platform.updateRegion(Qt.rect(rect.x + dock.x, rect.y + dock.y, rect.width, rect.height), 30, icons);
        if(hideProgress<.001) platform.updateTargets(dock.applicationAnchors.map(t=>({id:t.id, rect:Qt.rect(t.rect.x+dock.x-width/2,t.rect.y+dock.y-height,t.rect.width,t.rect.height)})),Qt.rect(rect.x+dock.x-width/2,rect.y+dock.y-height,rect.width,rect.height));
    }
    onWidthChanged: Qt.callLater(updateRegion)
    onHeightChanged: Qt.callLater(updateRegion)
    Component.onCompleted: Qt.callLater(updateRegion)
}
