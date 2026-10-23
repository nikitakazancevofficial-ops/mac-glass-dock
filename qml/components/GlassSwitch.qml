pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

Switch {
    id: toggle
    required property DockSettings settings
    required property string settingKey
    implicitHeight: 42
    onToggled: settings.setValue(settingKey, checked)
    indicator: Rectangle {
        x: toggle.width - width
        y: (toggle.height - height) / 2
        width: 42
        height: 25
        radius: 13
        color: toggle.checked ? "#7192c9f4" : "#3397adc4"
        border.color: "#45ffffff"
        Rectangle {
            width: 19
            height: 19
            radius: 10
            x: toggle.checked ? 20 : 3
            y: 3
            color: "#edf6ff"
            Behavior on x {
                NumberAnimation {
                    duration: toggle.settings.reducedMotion ? 0 : 130
                }
            }
        }
    }
    contentItem: Text {
        style: Text.Outline
        styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
        renderType: Text.QtRendering

        text: toggle.text
        rightPadding: 58
        font.pixelSize: 13
        color: (toggle.settings.theme === "light" || (toggle.settings.theme === "auto" && !toggle.settings.systemDark)) ? "#172b42" : "#edf5ff"
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
