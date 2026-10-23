pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock

Item {
    id: glass
    readonly property bool glassSurface: true
    required property DockSettings settings
    property bool blurAvailable: true
    property real cornerRadius: 30
    property vector2d pointer: Qt.vector2d(.5, .25)
    property real hoverAmount: 0
    property real density: 1
    property real opticScale: 1
    // Negative values inherit the Dock's optics. Panels may be gentler without
    // rewriting the user's carefully tuned Dock material.
    property real backdropBlurRadius: -1
    property real backdropRefractionStrength: -1
    property real backdropRefractionWidth: -1
    property real shadowStrength: 1
    property real finishStrength: 1
    property bool iconSurface: false
    readonly property bool dark: settings.theme === "dark" || (settings.theme === "auto" && settings.systemDark)
    readonly property bool useGpu: settings.gpuEffects && GraphicsInfo.api !== GraphicsInfo.Software
    readonly property real tintOpacity: Math.min(.85, (iconSurface ? settings.iconGlassOpacity : settings.surfaceOpacity) * density + (blurAvailable && settings.blurEnabled ? 0 : .16))

    Rectangle {
        anchors.fill: parent
        anchors.margins: -6
        radius: glass.cornerRadius + 6
        color: Qt.rgba(0, 0.03, 0.08, .07 * glass.shadowStrength)
        visible: !glass.useGpu && glass.shadowStrength > 0
    }
    Rectangle {
        anchors.fill: parent
        radius: glass.cornerRadius
        visible: !glass.useGpu || shader.status === ShaderEffect.Error
        gradient: Gradient {
            GradientStop { position: 0; color: glass.dark ? Qt.rgba(.17,.23,.31,glass.tintOpacity+.12) : Qt.rgba(.96,.98,1,glass.tintOpacity+.12) }
            GradientStop { position: .18; color: glass.dark ? Qt.rgba(.07,.13,.22,glass.tintOpacity) : Qt.rgba(.75,.84,.93,glass.tintOpacity) }
            GradientStop { position: .85; color: glass.dark ? Qt.rgba(.04,.08,.14,glass.tintOpacity) : Qt.rgba(.8,.87,.95,glass.tintOpacity) }
            GradientStop { position: 1; color: glass.dark ? Qt.rgba(.25,.31,.4,glass.tintOpacity+.12) : Qt.rgba(1,1,1,glass.tintOpacity+.14) }
        }
        border.width: 1
        border.color: Qt.rgba(1,1,1,.28 * glass.settings.edgeReflection + .06)
        Rectangle {
            anchors.fill: parent; anchors.margins: 3
            color: "transparent"; radius: Math.max(2, glass.cornerRadius - 3)
            border.width: 1; border.color: "#14ffffff"
        }
    }
    ShaderEffect {
        id: shader
        objectName: "glassShader"
        anchors.fill: parent
        anchors.margins: -18
        visible: glass.useGpu
        property real darkness: glass.dark ? 1 : 0
        property real glassOpacity: glass.tintOpacity
        property real highlightIntensity: glass.settings.highlightIntensity * glass.finishStrength
        property real thickness: Math.min((glass.iconSurface ? glass.settings.iconThickness : glass.settings.thickness) * glass.opticScale, glass.height * .45)
        property real edgeReflection: (glass.iconSurface ? glass.settings.iconReflection : glass.settings.edgeReflection) * glass.finishStrength
        property real cornerRadius: glass.cornerRadius
        property vector2d resolution: Qt.vector2d(width, height)
        property vector2d pointer: glass.pointer
        property color glassTint: glass.settings.tint
        property real hoverAmount: glass.hoverAmount
        property real rimWidth: glass.settings.rimWidth
        property real bevelCurve: glass.settings.bevelCurve
        property real fresnelPower: glass.settings.fresnelPower
        property real edgeDispersion: glass.settings.edgeDispersion
        property real innerShadowStrength: glass.settings.innerShadow * glass.finishStrength
        property real shadowOpacity: glass.settings.shadowOpacity * glass.finishStrength * glass.shadowStrength
        property real shadowRadius: glass.settings.shadowRadius
        property real highlightSize: glass.settings.highlightSize
        fragmentShader: "qrc:/effects/glass.frag.qsb"
        onStatusChanged: if (status === ShaderEffect.Error) console.warn("Glass shader:", log)
    }
}
