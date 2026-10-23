import QtQuick
import MacGlassDock

Item {
    id: tile
    required property DockSettings settings
    property real shadowStrength: 1
    required property string title
    required property string subtitle
    required property string kind
    property bool active: false
    signal toggled
    height: 76
    readonly property real pillRadius: height / 2
    VolumetricGlassSurface { shadowStrength: tile.shadowStrength; anchors.fill: parent; settings: tile.settings; cornerRadius: tile.pillRadius; density: tile.active ? 2.2 : 1.8; opticScale: .55 }
    Rectangle { anchors.fill: parent; radius: tile.pillRadius; color: tile.settings.systemDark ? "#68111d2d" : "#a8e8eef5" }
    Row {
        anchors.fill: parent
        anchors.margins: 11
        spacing: 10
        Rectangle {
            width: 44; height: 44; radius: 22
            color: tile.active ? "#1976d2" : "#65758a"
            anchors.verticalCenter: parent.verticalCenter
            ControlIcon { anchors.fill: parent; anchors.margins: 9; kind: tile.kind; color: "#ffffff"; stroke: 3.8; useC: true }
        }
        Column {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            Text { text: tile.title; color: tile.settings.systemDark ? "#f0f7ff" : "#172b42"; font.pixelSize: 15; font.weight: Font.DemiBold; style: Text.Outline; styleColor: tile.settings.systemDark ? "#80101b2b" : "#a8ffffff" }
            Text { text: tile.subtitle; color: tile.settings.systemDark ? "#c7d5e6" : "#526a82"; font.pixelSize: 13; elide: Text.ElideRight; width: 110; style: Text.Outline; styleColor: tile.settings.systemDark ? "#80101b2b" : "#a8ffffff" }
        }
    }
    TapHandler { onTapped: tile.toggled() }
}
