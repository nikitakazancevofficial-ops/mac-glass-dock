// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
import QtQuick

Item {
    id: button
    property color fillColor: "#ff6059"
    property string action: "close"
    property bool active: true
    property bool available: true
    signal triggered()
    width: 22
    height: 32
    Rectangle {
        anchors.centerIn: parent
        width: 13
        height: 13
        radius: 6.5
        color: !button.available || !button.active && !hover.hovered ? "#999ca3" : button.fillColor
        border.width: .6
        border.color: Qt.darker(color, 1.16)
        scale: tap.pressed ? .9 : 1
        opacity: !button.available ? .45 : 1
        Text {
            anchors.centerIn: parent
            text: button.action === "close" ? "×" : button.action === "minimize" ? "−" : "+"
            font.family: "Sans Serif"
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: "#70231c"
            visible: hover.hovered && button.available
        }
    }
    HoverHandler {id: hover}
    TapHandler {id: tap; enabled: button.available; onTapped: button.triggered()}
    Accessible.role: Accessible.Button
    Accessible.name: action === "close" ? "Закрыть" : action === "minimize" ? "Свернуть" : "Развернуть или восстановить"
    Accessible.onPressAction: if(button.available) button.triggered()
}
