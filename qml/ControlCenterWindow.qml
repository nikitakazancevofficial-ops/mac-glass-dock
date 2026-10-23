pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

Window {
    id: cc
    objectName: "controlCenterWindow"
    property string kind: "controlcenter"
    required property DockPlatform platform
    required property DockSettings settings
    required property WindowTracker windows
    property var telemetry
    property bool expanded: false
    property bool edgePeek: false
    property real panelReveal: 0
    // KWin's live Genie path reads the same progress property as GlassPanelWindow.
    readonly property real revealProgress: panelReveal
    property bool focusOn: false
    property real displayLevel: .72
    property real soundLevel: .38
    property real keyboardLevel: .22
    readonly property color ink: (settings.theme === "light" || (settings.theme === "auto" && !settings.systemDark)) ? "#172b42" : "#f0f7ff"
    readonly property color secondaryInk: settings.systemDark ? "#c7d5e6" : "#526a82"
    readonly property bool statusDarkAsset: settings.theme === "light" || (settings.theme === "auto" && !settings.systemDark)
    readonly property color statusInk: statusDarkAsset ? "#172b42" : "#ffffff"
    readonly property color statusPill: statusDarkAsset ? "#b8edf5fc" : "#a91a2a3d"
    readonly property color statusPillEdge: statusDarkAsset ? "#aaffffff" : "#6bffffff"
    readonly property real statusWidth: Math.min(680, width)
    readonly property bool statusShown: !platform.statusObstructed || expanded || edgePeek
    readonly property bool statusInteractive: statusShown
    readonly property rect statusInputRect: Qt.rect(width - statusContent.width - 20, statusBar.y + 5, statusContent.width + 20, 28)
    readonly property bool transitionActive: panelOpenAnimation.running || panelCloseAnimation.running
    readonly property bool genieActive: transitionActive && panel.visible
        && settings.gpuEffects && !settings.reducedMotion && GraphicsInfo.api !== GraphicsInfo.Software
    readonly property bool nativeMotionActive: genieActive && platform.nativePanelGenie
    readonly property bool stablePanelInput: true
    readonly property bool genieClipFrame: true
    readonly property bool presented: expanded
    readonly property bool terminalHidden: false
    readonly property bool waitingForNative: false
    readonly property real panelRadius: 24
    readonly property rect restingFrame: Qt.rect(panel.x, panel.y, panel.width, panel.height)
    readonly property rect destinationIcon: Qt.rect(statusContent.x + statusContent.width / 2 - 24,
                                                    statusBar.y + statusBar.height / 2 - 14, 48, 28)
    width: Math.min(680, Math.max(360, screen.width - 28))
    height: Math.min(790, Math.max(620, screen.height - 22))
    visible: false
    color: "transparent"
    // The layer-shell surface uses OnDemand keyboard interaction. Keep the
    // window focusable so controls work when the panel is opened over a client.
    flags: Qt.Window | Qt.FramelessWindowHint

    function updateShape() {
        // Layer-shell windows do not expose a reliable global origin through
        // mapToGlobal on Wayland. Compute the pill from its right/top anchors.
        const pillWidth = statusContent.width + 16
        // The control-center layer reaches the physical right edge. Keep the
        // published visual pill bounds exact; its input target is extended to
        // the edge below so a click at the reveal hotspot can open it.
        platform.updateStatusRegion(Qt.rect(screen.virtualX + screen.width - 4 - pillWidth,
                                            screen.virtualY + 9, pillWidth, 28))
        if (!statusShown) {
            platform.updatePanelRegion(cc, Qt.rect(0, 0, 0, 0), 0)
            return
        }
        // The collapsed status pill is the right-edge hover preview and click
        // target. Expanding it turns the same surface into the tools panel.
        if (expanded || transitionActive || panelReveal > .001) {
            // Keep the controls' whole input surface throughout the visual
            // animation. Warped glass must never expose the client underneath.
            platform.updatePanelRegion(cc, restingFrame, panelRadius)
        } else {
            platform.updatePanelRegion(cc, Qt.rect(width - statusContent.width - 20, statusBar.y + 5, statusContent.width + 20, 28), 14)
        }
    }
    Component.onCompleted: {
        platform.preparePanel(cc, "controlcenter")
        updateShape()
        visible = true
    }
    onStatusShownChanged: {
        updateShape()
    }
    onExpandedChanged: {
        updateShape()
        if (expanded) {
            panelCloseAnimation.stop()
            platform.setPanelKeyboardFocus(cc, true)
            panelReveal = 0
            panelOpenAnimation.restart()
        } else {
            platform.setPanelKeyboardFocus(cc, false)
            panelOpenAnimation.stop()
            panelCloseAnimation.restart()
        }
    }
    onPanelRevealChanged: updateShape()
    onTransitionActiveChanged: updateShape()
    onGenieActiveChanged: updateShape()
    onEdgePeekChanged: updateShape()
    onStatusInputRectChanged: updateShape()
    onRestingFrameChanged: updateShape()
    Connections {
        target: cc.platform
        function onStatusObstructionChanged() { cc.updateShape() }
        function onControlCenterStatusPressed() {
            // This signal is reserved for the transparent right-edge extension.
            edgeRevealTimer.stop()
            cc.edgePeek = true
            cc.expanded = !cc.expanded
            if (cc.expanded) hideTimer.stop()
            else hideTimer.restart()
        }
    }
    onWidthChanged: updateShape()
    onHeightChanged: updateShape()
    onVisibleChanged: if (visible) Qt.callLater(updateShape)
    Connections {
        target: cc.platform
        function onPanelOutsidePressed(name) {
            if (name !== cc.objectName) return
            // A click on the application underneath the hidden status pill
            // must cancel a pending edge-hover reveal. Otherwise the click
            // reaches the app, but the stale timer opens the pill right after.
            edgeRevealTimer.stop()
            if (!cc.expanded) {
                cc.edgePeek = false
                hideTimer.stop()
                return
            }
            cc.expanded = false
            cc.edgePeek = false
            hideTimer.stop()
        }
        function onControlCenterEdgeHoverChanged(hovered) {
            if (hovered) {
                edgeRevealTimer.restart()
                hideTimer.stop()
            } else {
                edgeRevealTimer.stop()
                if (!cc.expanded && !statusHover.hovered) hideTimer.restart()
            }
        }
    }
    Timer {
        id: edgeRevealTimer
        interval: 240
        onTriggered: {
            if (!cc.expanded && cc.platform.controlCenterEdgeHovered)
                cc.edgePeek = true
        }
    }
    Timer {
        id: hideTimer
        interval: 1100
        onTriggered: {
            if (!cc.expanded && !cc.platform.controlCenterEdgeHovered && !statusHover.hovered)
                cc.edgePeek = false
        }
    }

    Item {
        id: statusBar
        objectName: "controlCenterStatusBar"
        x: 0
        // Keep the preview close to the top of the active edge area.
        y: 0
        width: cc.statusWidth
        height: 38
        opacity: cc.statusShown ? 1 : 0
        Behavior on y { NumberAnimation { duration: 170; easing.type: Easing.OutCubic } }
        Behavior on opacity { NumberAnimation { duration: 120 } }
        onYChanged: cc.updateShape()
        Rectangle {
            x: statusContent.x - 8
            y: 5
            width: statusContent.width + 16
            height: 28
            radius: 14
            color: cc.statusPill
            border.color: cc.statusPillEdge
            border.width: 1
        }
        Row {
            id: statusContent
            anchors.right: parent.right
            anchors.rightMargin: 12
            width: childrenRect.width
            height: parent.height
            spacing: 7
            StatusIcon { width: 20; height: 20; kind: "wifi"; color: cc.statusInk; anchors.verticalCenter: parent.verticalCenter; stroke: 3; useC: true; darkAsset: cc.statusDarkAsset; opacity: cc.telemetry.wifiEnabled ? 1 : .45 }
            StatusIcon { width: 28; height: 20; kind: "battery"; color: cc.statusInk; anchors.verticalCenter: parent.verticalCenter; stroke: 1.9; useC: true; darkAsset: cc.statusDarkAsset; batteryLevel: cc.telemetry.battery / 100; charging: cc.telemetry.charging }
            Text { text: cc.telemetry.battery > 0 ? cc.telemetry.battery + "%" : "—"; color: cc.statusInk; font.pixelSize: 16; font.weight: Font.DemiBold; anchors.verticalCenter: parent.verticalCenter }
            StatusIcon { width: 20; height: 20; kind: "controls"; color: cc.statusInk; anchors.verticalCenter: parent.verticalCenter; stroke: 2.1 }
            StatusIcon { width: 20; height: 20; kind: "focus"; color: cc.statusInk; anchors.verticalCenter: parent.verticalCenter; stroke: 2.1; useC: true; darkAsset: cc.statusDarkAsset; visible: cc.telemetry.focusActive }
            Text {
                id: statusDate
                anchors.verticalCenter: parent.verticalCenter
                text: cc.telemetry.dateText + "  " + cc.telemetry.timeText
                color: cc.statusInk
                font.pixelSize: 16
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideNone
            }
        }
        Connections {
            target: statusContent
            function onWidthChanged() { cc.updateShape() }
        }
        HoverHandler {
            id: statusHover
            onHoveredChanged: {
                if (hovered) hideTimer.stop()
                else if (!cc.expanded && !cc.platform.controlCenterEdgeHovered) hideTimer.restart()
            }
        }
        TapHandler {
            objectName: "controlCenterStatusTapHandler"
            enabled: cc.statusInteractive
            onTapped: {
                edgeRevealTimer.stop()
                cc.edgePeek = true
                cc.expanded = !cc.expanded
                if (cc.expanded) hideTimer.stop()
                else hideTimer.restart()
            }
        }
    }

    Item {
        id: panel
        objectName: "controlCenterTools"
        visible: cc.expanded || cc.panelReveal > .001
        x: cc.width - width - 14 + (!cc.genieActive ? (1 - cc.panelReveal) * 24 : 0)
        y: 72 - (!cc.genieActive ? (1 - cc.panelReveal) * 14 : 0)
        width: Math.min(426, cc.width - 28)
        height: Math.min(cc.height - 86, toolsContent.implicitHeight + 8)
        opacity: cc.genieActive ? 1 : cc.panelReveal
        scale: cc.genieActive ? 1 : .92 + .08 * cc.panelReveal
        transformOrigin: Item.TopRight
        // Native Genie warps the live surface in KWin. Keeping an intermediate
        // QML texture here freezes hover lighting and creates square seams.
        layer.enabled: cc.genieActive && !cc.nativeMotionActive
        layer.smooth: true
        layer.effect: ShaderEffect {
            property var source
            property real openness: cc.panelReveal
            property vector2d panelSize: Qt.vector2d(panel.width, panel.height)
            property vector4d iconRect: Qt.vector4d(
                statusContent.x + statusContent.width / 2 - 24 - panel.x,
                statusBar.y + statusBar.height / 2 - 14 - panel.y, 48, 28)
            mesh: GridMesh { resolution: Qt.size(32, 64) }
            vertexShader: "qrc:/effects/panel-genie.vert.qsb"
            fragmentShader: "qrc:/effects/panel-genie.frag.qsb"
        }
        Column {
            id: toolsContent
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.topMargin: 4
            anchors.bottomMargin: 4
            spacing: 11
            Grid {
                width: parent.width
                columns: 2
                columnSpacing: 10
                rowSpacing: 10
                ControlTile { shadowStrength: 0; width: (parent.width - 10) / 2; settings: cc.settings; title: "Wi‑Fi"; subtitle: cc.telemetry.wifiEnabled ? cc.telemetry.wifiName : "Выкл."; kind: "wifi"; active: cc.telemetry.wifiEnabled; onToggled: cc.telemetry.setWifiEnabled(!cc.telemetry.wifiEnabled) }
                ControlTile { shadowStrength: 0; width: (parent.width - 10) / 2; settings: cc.settings; title: "Bluetooth"; subtitle: cc.telemetry.bluetoothEnabled ? "Вкл." : "Выкл."; kind: "bluetooth"; active: cc.telemetry.bluetoothEnabled; onToggled: cc.telemetry.setBluetoothEnabled(!cc.telemetry.bluetoothEnabled) }
                ControlTile { shadowStrength: 0; width: (parent.width - 10) / 2; settings: cc.settings; title: "Focus"; subtitle: cc.telemetry.focusActive ? "Вкл." : "Выкл."; kind: "focus"; active: cc.telemetry.focusActive; onToggled: cc.telemetry.setFocusMode(!cc.telemetry.focusActive) }
                ControlTile { shadowStrength: 0; width: (parent.width - 10) / 2; settings: cc.settings; title: "Экран"; subtitle: "Дублирование"; kind: "mirroring"; active: false }
            }
            Item {
                width: parent.width
                height: 92
                VolumetricGlassSurface { shadowStrength: 0; anchors.fill: parent; settings: cc.settings; cornerRadius: 24; density: 1.8; opticScale: .5 }
                Rectangle { anchors.fill: parent; radius: 24; color: cc.settings.systemDark ? "#68111d2d" : "#a8e8eef5" }
                Text { x: 16; y: 12; text: "Сейчас играет"; color: cc.ink; font.pixelSize: 15; font.weight: Font.DemiBold; style: Text.Outline; styleColor: cc.settings.systemDark ? "#80101b2b" : "#a8ffffff" }
                Text { anchors.centerIn: parent; text: "◀◀     ▶     ▶▶"; color: cc.ink; font.pixelSize: 21 }
            }
            ControlSlider { shadowStrength: 0; objectName: "brightnessControl"; width: parent.width; settings: cc.settings; title: ""; kind: "display"; value: cc.telemetry.brightness / 100; onMoved: function(newValue) { cc.telemetry.setBrightness(Math.round(newValue * 100)) } }
            ControlSlider { shadowStrength: 0; objectName: "volumeControl"; width: parent.width; settings: cc.settings; title: ""; kind: "sound"; value: cc.telemetry.volume / 100; onMoved: function(newValue) { cc.telemetry.setVolume(Math.round(newValue * 100)) } }
            ControlSlider { shadowStrength: 0; objectName: "keyboardBrightnessControl"; width: parent.width; settings: cc.settings; title: ""; kind: "keyboard"; value: cc.telemetry.keyboardBrightness / 100; onMoved: function(newValue) { cc.telemetry.setKeyboardBrightness(Math.round(newValue * 100)) } }
        }
    }
    NumberAnimation {
        id: panelOpenAnimation
        target: cc
        property: "panelReveal"
        from: 0
        to: 1
        duration: cc.settings.reducedMotion ? 0 : 620 / Math.pow(cc.settings.animationSpeed, .35)
        easing.type: Easing.InOutSine
    }
    NumberAnimation {
        id: panelCloseAnimation
        target: cc
        property: "panelReveal"
        to: 0
        duration: cc.settings.reducedMotion ? 0 : 650 / Math.pow(cc.settings.animationSpeed, .35)
        easing.type: Easing.InOutSine
    }
}
