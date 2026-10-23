pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import QtQuick.Controls
import MacGlassDock

Item {
    id: item
    required property DockSettings settings
    property string label: "Item"
    property string icon: "application-x-executable"
    property bool appAvailable: true
    property bool busy: false
    property int windowCount: 0
    property bool activeApplication: false
    property bool hoverEnabled: true
    property bool tooltipSuppressed: false
    property point pointerSample: Qt.point(0,0)
    property point lastPointerSample: pointerSample
    property real bounceOffset: 0
    property real baseSize: 56
    property real targetScale: 1
    property real animationSpeed: 1
    property real zoom: targetScale
    readonly property bool hovered: hover.hovered && hoverEnabled
    signal hoverChanged(bool inside)
    signal windowsRequested()
    onHoveredChanged: {
        lastPointerSample = pointerSample
        hoverChanged(hovered)
    }
    onPointerSampleChanged: {
        // Moving/resizing the spring-driven delegate is not mouse movement.
        // Ignore subpixel layer-shell rounding; accumulate real displacement.
        if (hovered && Math.hypot(pointerSample.x-lastPointerSample.x,pointerSample.y-lastPointerSample.y)>1) {
            lastPointerSample=pointerSample
            if(windowCount>0) windowsDelay.restart()
        }
    }
    Timer {
        id: windowsDelay
        objectName: "windowPreviewDelay"
        interval: 2000
        running: item.hovered && item.windowCount>0
        onTriggered: item.windowsRequested()
    }
    signal activated
    signal contextRequested
    width: baseSize * zoom
    height: width
    Behavior on zoom {
        enabled: !item.settings.reducedMotion
        SpringAnimation {
            spring: 5 * item.animationSpeed
            damping: item.settings.springDamping
            mass: 1
            epsilon: 0.002
        }
    }
    onBusyChanged: {
        if (busy && settings.bounceEnabled && !settings.reducedMotion)
            bounce.restart();
        else {
            bounce.stop();
            bounceOffset = 0;
        }
    }
    SequentialAnimation {
        id: bounce
        loops: 2
        NumberAnimation {
            target: item
            property: "bounceOffset"
            to: -10
            duration: 180
            easing.type: Easing.OutQuad
        }
        NumberAnimation {
            target: item
            property: "bounceOffset"
            to: 0
            duration: 220
            easing.type: Easing.InOutQuad
        }
    }

    AdaptiveAppIcon {
        id: image
        anchors.fill: parent
        settings: item.settings
        name: item.icon
        available: item.appAvailable
        pointer: Qt.vector2d(hover.point.position.x / Math.max(1, width), hover.point.position.y / Math.max(1, height))
        hoverAmount: hover.hovered && item.hoverEnabled ? 1 : 0
        // The optical lens must disappear on the same frame as the pointer
        // leaves the Dock.  Keep the spring on `zoom` for the pleasant return
        // motion, but do not leave a stale edge highlight behind.
        Behavior on hoverAmount { NumberAnimation { duration: item.settings.reducedMotion || !item.hoverEnabled ? 0 : 80 } }
        transform: Translate {
            y: item.bounceOffset
        }
        scale: tap.pressed ? 0.88 : 1
        Behavior on scale {
            SpringAnimation {
                spring: 6
                damping: 0.5
            }
        }
    }

    // Running state comes only from the compositor. Pulsing means a launch job.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.bottom
        anchors.topMargin: 6
        width: item.windowCount > 1 ? 10 : 4
        height: 4
        radius: 2
        color: item.activeApplication ? "#8dcfff" : "#e8f4ff"
        visible: item.busy || item.windowCount > 0
        SequentialAnimation on opacity {
            running: item.busy && !item.settings.reducedMotion
            loops: Animation.Infinite
            NumberAnimation {
                from: 0.25
                to: 1
                duration: 320
            }
            NumberAnimation {
                from: 1
                to: 0.25
                duration: 320
            }
        }
    }

    ToolTip {
        objectName: "applicationTooltip"
        parent: item
        visible: hover.hovered && item.hoverEnabled && !item.tooltipSuppressed
        delay: 350
        text: item.label
        y: -height - 10
        background: VolumetricGlassSurface {
            settings: item.settings
            cornerRadius: 14
            density: 2
        }
        contentItem: Text {
            style: Text.Outline
            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
            renderType: Text.QtRendering
            text: item.label
            color: (item.settings.theme === "light" || (item.settings.theme === "auto" && !item.settings.systemDark)) ? "#172b42" : "#edf5ff"
            font.pixelSize: 12
        }
    }
    HoverHandler {
        id: hover
        cursorShape: item.appAvailable ? Qt.PointingHandCursor : Qt.ForbiddenCursor
    }
    TapHandler {
        id: tap
        onTapped: item.activated()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: item.contextRequested()
    }
    Accessible.role: Accessible.Button
    Accessible.name: label
    Accessible.onPressAction: item.activated()
}
