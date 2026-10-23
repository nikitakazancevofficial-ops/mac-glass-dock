import QtQuick
import MacGlassDock

Item {
    id: statusIcon
    property string kind: "wifi"
    property color color: "white"
    property real stroke: 2.8
    property bool useC: false
    property bool darkAsset: false
    property real batteryLevel: 1
    property bool charging: false

    ControlIcon {
        anchors.fill: parent
        kind: statusIcon.kind
        color: statusIcon.color
        stroke: statusIcon.stroke
        useC: statusIcon.useC
        darkAsset: statusIcon.darkAsset
        batteryLevel: statusIcon.batteryLevel
        charging: statusIcon.charging
    }
}
