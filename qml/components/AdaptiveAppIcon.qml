pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Window
import MacGlassDock

Item {
    id: icon
    required property DockSettings settings
    property string name: "application-x-executable"
    property bool available: true
    property vector2d pointer: Qt.vector2d(.5, .3)
    property real hoverAmount: 0
    readonly property bool original: settings.iconStyle === "original"
    readonly property bool glassMode: settings.iconStyle === "glass"
    readonly property bool dark: settings.theme === "dark" || (settings.theme === "auto" && settings.systemDark)
    readonly property bool gpu: settings.gpuEffects && GraphicsInfo.api !== GraphicsInfo.Software
    VolumetricGlassSurface {
        anchors.fill: parent
        anchors.margins: 1
        objectName: "iconGlassSurface"
        settings: icon.settings
        iconSurface: true
        cornerRadius: Math.min(width, height) * icon.settings.iconRadius
        pointer: icon.pointer
        hoverAmount: icon.hoverAmount
        visible: !icon.original
        density: 1
    }
    Image {
        id: sourceImage
        objectName: "appIcon"
        anchors.fill: parent
        anchors.margins: icon.original ? 0 : parent.width * icon.settings.iconPadding
        source: "image://dockicons/" + (icon.original ? "original/" : icon.dark ? "dark/" : "light/") + encodeURIComponent(icon.name)
        sourceSize: Qt.size(Math.ceil(192 * Math.max(1, Screen.devicePixelRatio)),
                            Math.ceil(192 * Math.max(1, Screen.devicePixelRatio)))
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: icon.original || icon.glassMode || !icon.gpu || adaptive.status === ShaderEffect.Error
        opacity: icon.available ? 1 : .35
    }
    ShaderEffect {
        id: adaptive
        objectName: "adaptiveIconShader"
        anchors.fill: parent
        visible: !icon.original && !icon.glassMode && icon.gpu
        property variant source: sourceImage
        property real darkness: icon.dark ? 1 : 0
        property real availability: icon.available ? 1 : .35
        property real padding: icon.settings.iconPadding
        // One shared material owns the contour. The logo shader must not draw
        // a second, differently rounded plate over the compositor lens.
        property real plateOpacity: 0
        fragmentShader: "qrc:/effects/adaptive-icon.frag.qsb"
    }
}
