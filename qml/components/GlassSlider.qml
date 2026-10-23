pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

Item {
    id: control
    objectName: settingKey
    required property DockSettings settings
    required property string label
    required property string settingKey
    required property real currentValue
    property real from: 0
    property real to: 1
    property real stepSize: .01
    property string suffix: ""
    width: parent ? parent.width : 300
    height: 62
    Text {
        style: Text.Outline
        styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
        renderType: Text.QtRendering

        text: control.label
        color: (control.settings.theme === "light" || (control.settings.theme === "auto" && !control.settings.systemDark)) ? "#172b42" : "#edf5ff"
        font.pixelSize: 13
        anchors.left: parent.left
        anchors.top: parent.top
    }
    Text {
        style: Text.Outline
        styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
        renderType: Text.QtRendering

        text: (control.stepSize >= 1 ? control.currentValue.toFixed(0) : control.currentValue.toFixed(2)) + control.suffix
        color: (control.settings.theme === "light" || (control.settings.theme === "auto" && !control.settings.systemDark)) ? "#476078" : "#a9c4df"
        font.pixelSize: 12
        anchors.right: parent.right
    }
    Slider {
        id: slider
        objectName: "settingSlider"
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 30
        from: control.from
        to: control.to
        stepSize: control.stepSize
        value: control.currentValue
        onMoved: control.settings.setValue(control.settingKey, value)
        background: Rectangle {
            x: slider.leftPadding
            y: (slider.height - height) / 2
            width: slider.availableWidth
            height: 5
            radius: 3
            color: "#2d95b5d5"
            Rectangle {
                width: slider.visualPosition * parent.width
                height: parent.height
                radius: 3
                color: "#d4eaff"
            }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: (slider.height - height) / 2
            width: 19
            height: 19
            radius: 10
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: "#ffffff"
                }
                GradientStop {
                    position: 1
                    color: "#b5cce3"
                }
            }
            border.color: "#8affffff"
            scale: slider.pressed ? 1.12 : 1
        }
    }
}
