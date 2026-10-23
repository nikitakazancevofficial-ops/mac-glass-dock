// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
import QtQuick
import org.kde.kwin.decoration

Decoration {
    id: root
    alpha: true
    readonly property bool expanded: decoration.client.maximized
    readonly property int inset: expanded ? 0 : 12
    DecorationOptions {id: options; deco: decoration}
    SystemPalette {id: palette}
    readonly property bool dark: palette.window.hslLightness < .5
    readonly property color barColor: options.titleBarColor
    readonly property color ink: options.fontColor
    // KWin consumes the transparent padding as the window's shadow atlas.
    Repeater {
        model: root.expanded ? 0 : 3
        Rectangle {
            required property int index
            x: root.inset - (index + 1) * 3
            y: root.inset - (index + 1) * 2 + 2
            width: root.width - x * 2
            height: root.height - root.inset * 2 + (index + 1) * 5
            radius: 10 + (index + 1) * 3
            color: "#09000000"
        }
    }
    Rectangle {
        id: frame
        x: root.inset
        y: root.inset
        width: parent.width - root.inset * 2
        height: parent.height - root.inset * 2
        radius: root.expanded ? 0 : 10
        color: root.barColor
        border.width: root.expanded ? 0 : 1
        border.color: root.dark ? "#36404c" : "#c6cad0"
        // Application content is composed by KWin over the centre. Only the
        // 34px titlebar and thin frame are painted by this decoration.
        Rectangle {
            x: 1; y: 25
            width: parent.width - 2
            height: Math.max(0, parent.height - 35)
            color: root.barColor
        }
        Row {
            id: buttons
            x: 10
            y: root.expanded ? 0 : 1
            TrafficButton {
                action: "close"; fillColor: "#ff6059"
                active: decoration.client.active
                available: decoration.client.closeable
                onTriggered: decoration.requestClose()
            }
            TrafficButton {
                action: "minimize"; fillColor: "#ffbd2e"
                active: decoration.client.active
                available: decoration.client.minimizeable
                onTriggered: decoration.requestMinimize()
            }
            TrafficButton {
                action: "maximize"; fillColor: "#28c840"
                active: decoration.client.active
                available: decoration.client.maximizeable
                onTriggered: decoration.requestToggleMaximization(Qt.LeftButton)
            }
        }
        Text {
            x: Math.max(buttons.x + buttons.width + 14, (parent.width - implicitWidth) / 2)
            y: 0
            width: Math.max(0, parent.width - x - 95)
            height: 34
            text: decoration.client.caption
            color: root.ink
            font: options.titleFont
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
            opacity: decoration.client.active ? 1 : .7
        }
    }
    Component.onCompleted: {
        borders.setBorders(1)
        borders.setTitle(34)
        maximizedBorders.setAllBorders(0)
        maximizedBorders.setTitle(32)
        extendedBorders.setAllBorders(5)
        padding.setAllBorders(12)
    }
}
