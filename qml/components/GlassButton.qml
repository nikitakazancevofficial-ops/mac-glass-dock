pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

AbstractButton {
    id: button
    required property DockSettings settings
    property bool selected: false
    property string helpText: ""
    implicitWidth: Math.max(40, label.implicitWidth + 30)
    implicitHeight: 38
    hoverEnabled: true
    opacity: enabled ? 1 : .35
    scale: down ? .96 : 1
    Behavior on scale {
        NumberAnimation {
            duration: 100
        }
    }
    background: VolumetricGlassSurface {
        settings: button.settings
        cornerRadius: Math.min(20, button.height / 2)
        density: button.selected ? 1.3 : .6
        opticScale: .45
        hoverAmount: button.hovered ? 1 : 0
    }
    contentItem: Text {
        id: label
        style: Text.Outline
        styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
        renderType: Text.QtRendering
        text: button.text
        color: (button.settings.theme === "light" || (button.settings.theme === "auto" && !button.settings.systemDark)) ? "#172b42" : "#f3f8ff"
        font.pixelSize: 13
        font.weight: button.selected ? Font.DemiBold : Font.Medium
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    ToolTip {
        visible: button.hovered && button.helpText.length > 0
        delay: 500
        text: button.helpText
        background: VolumetricGlassSurface {
            settings: button.settings
            cornerRadius: 14
            density: 1.8
        }
        contentItem: Text {
            style: Text.Outline
            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
            renderType: Text.QtRendering
            text: button.helpText
            color: (button.settings.theme === "light" || (button.settings.theme === "auto" && !button.settings.systemDark)) ? "#172b42" : "#edf5ff"
            font.pixelSize: 12
        }
    }
}
