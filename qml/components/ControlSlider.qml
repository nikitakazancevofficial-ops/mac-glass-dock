import QtQuick
import MacGlassDock

Item {
    id: slider
    required property DockSettings settings
    property real shadowStrength: 1
    required property string title
    required property string kind
    required property real value
    signal moved(real value)
    property real liveValue: value
    property bool dragging: false
    onValueChanged: if (!dragging) liveValue = value
    function setFromPosition(x) {
        const trackLeft = 52
        const trackWidth = Math.max(1, width - 108)
        liveValue = Math.max(0, Math.min(1, (x - trackLeft) / trackWidth))
        moved(liveValue)
    }
    height: 48
    VolumetricGlassSurface { shadowStrength: slider.shadowStrength; anchors.fill: parent; settings: slider.settings; cornerRadius: 24; density: 1.55; opticScale: .46 }
    Rectangle { anchors.fill: parent; radius: 24; color: slider.settings.systemDark ? "#68111d2d" : "#a8e8eef5" }
    ControlIcon { x: 14; anchors.verticalCenter: parent.verticalCenter; width: 22; height: 22; kind: slider.kind; color: "#ffffff"; stroke: 2.4; useC: true }
    Text { anchors.right: parent.right; anchors.rightMargin: 14; anchors.verticalCenter: parent.verticalCenter; text: Math.round(slider.liveValue * 100) + "%"; color: slider.settings.systemDark ? "#c7d5e6" : "#526a82"; font.pixelSize: 13; style: Text.Outline; styleColor: slider.settings.systemDark ? "#80101b2b" : "#a8ffffff" }
    Rectangle { x: 52; anchors.verticalCenter: parent.verticalCenter; width: parent.width - 108; height: 6; radius: 3; color: Qt.rgba(1, 1, 1, .18); Rectangle { width: parent.width * slider.liveValue; height: parent.height; radius: 3; color: slider.settings.systemDark ? "#f0f7ff" : "#172b42" } }
    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
        onPressed: function(mouse) { slider.dragging = true; slider.setFromPosition(mouse.x) }
        onPositionChanged: function(mouse) { if (pressed) slider.setFromPosition(mouse.x) }
        onReleased: slider.dragging = false
        onCanceled: slider.dragging = false
    }
}
