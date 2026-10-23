pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock
Window {
    id: panel
    required property DockSettings settings
    required property DockPlatform platform
    property string kind: "launcher"
    property bool closeOnDeactivate: true
    property bool hadFocus: false
    property bool presented: false
    property bool focusOnOpen: true
    property real panelRadius: 32
    property bool glassMaterial: true
    property real contentPadding: 24
    property real materialDensity: 1
    property real materialOpticScale: 1
    property real materialFinish: 1
    property real materialBlurRadius: -1
    property real materialRefraction: -1
    property real materialRefractionWidth: -1
    property real frameWidth: width - 48
    property real frameHeight: height - 48
    property bool animateToDock: false
    property point sourceDockPoint: Qt.point(0,0)
    property point animationAnchor: Qt.point(width/2,height/2)
    property real revealProgress: 0
    property bool genieFailed: false
    property bool compositorMotion: false
    property bool waitingForNative: false
    property bool nativePrepared: true
    property bool terminalHidden: false
    property int hideGeneration: 0
    property real animationIconSize: settings.iconSize
    readonly property bool genieActive: animateToDock && visible && revealProgress < .99999
        && settings.gpuEffects && !settings.reducedMotion && !genieFailed && GraphicsInfo.api !== GraphicsInfo.Software
    readonly property bool nativeMotionActive: genieActive && compositorMotion && platform.nativePanelGenie
    // Cache the finished panel while it is moving. Without this, every frame
    // re-runs all of its individual glass shaders even though the controls
    // themselves have not changed.
    readonly property bool transitionLayerActive: visible && !genieActive
        && (showAnimation.running || hideAnimation.running)
    readonly property rect destinationIcon: Qt.rect(animationAnchor.x-animationIconSize/2,animationAnchor.y-animationIconSize/2,animationIconSize,animationIconSize)
    readonly property rect restingFrame: Qt.rect(frame.x,frame.y,frame.width,frame.height)
    readonly property rect interactiveRect: genieActive ? platform.geniePanelBounds(restingFrame,destinationIcon,revealProgress,panelRadius)
        : Qt.rect(frame.x+frame.width*(1-frame.scale)/2+travel.x,
        frame.y+frame.height*(1-frame.scale)/2+travel.y,frame.width*frame.scale,frame.height*frame.scale)
    default property alias body: content.data
    signal opened()
    color: "transparent"
    visible: false
    flags: Qt.Window | Qt.FramelessWindowHint
    width: 840; height: 640
    function open() {
        const wasVisible=visible
        ++hideGeneration
        terminalHidden=false
        hideAnimation.stop()
        if(!genieActive) compositorMotion=platform.nativePanelGenie
        waitingForNative = !wasVisible && animateToDock && compositorMotion && platform.nativePanelGenie
            && settings.gpuEffects && !settings.reducedMotion && GraphicsInfo.api !== GraphicsInfo.Software
        nativePrepared = !waitingForNative
        platform.preparePanel(panel, kind)
        animationAnchor = platform.panelAnchor(panel, sourceDockPoint)
        animationIconSize = settings.iconSize
        updateRegion() // Install the shaped input region before the first map.
        hadFocus = false
        show()
        presented = true
        if(waitingForNative) prepareTimeout.restart()
        else showAnimation.restart()
        if(focusOnOpen) requestActivate()
        opened()
    }
    function dismiss() {
        if (!visible || !presented) return
        prepareTimeout.stop();waitingForNative=false;nativePrepared=true
        if(!genieActive) compositorMotion=platform.nativePanelGenie
        if (animateToDock) animationAnchor = platform.panelAnchor(panel, sourceDockPoint)
        presented = false
        showAnimation.stop()
        if(revealProgress<=.0001) {completeDismiss();return}
        hideAnimation.restart()
    }
    function completeDismiss() {
        // Commit a transparent client buffer before unmapping. The compositor
        // may keep the old surface alive for a generic popup fade.
        terminalHidden=true
        ++hideGeneration
        if(animateToDock && visible && GraphicsInfo.api !== GraphicsInfo.Software)
            platform.hidePanelAfterFrame(panel,hideGeneration)
        else finishDismiss(hideGeneration)
    }
    function finishDismiss(generation) {
        if(generation!==hideGeneration || presented || !terminalHidden)return
        hide();hadFocus=false
    }
    function updateRegion() {
        if(genieActive) platform.updateGeniePanelRegion(panel,restingFrame,destinationIcon,revealProgress,panelRadius)
        else platform.updatePanelRegion(panel, interactiveRect, panelRadius * frame.scale)
    }
    onWidthChanged: Qt.callLater(updateRegion)
    onHeightChanged: Qt.callLater(updateRegion)
    onVisibleChanged: if (visible) Qt.callLater(updateRegion)
    onInteractiveRectChanged: if (visible) Qt.callLater(updateRegion)
    onGenieActiveChanged: if (visible) Qt.callLater(updateRegion)
    // The bounding rect can remain constant while the interior neck changes.
    // Publish EVERY progress step, not only changes of its outer bounds.
    onRevealProgressChanged: if(visible) Qt.callLater(updateRegion)
    Connections {
        target: panel.platform
        function onPanelOutsidePressed(name: string) {
            if (name === panel.objectName && panel.presented) panel.dismiss()
        }
        function onPanelMotionPrepared(name: string) {
            if(name!==panel.objectName || !panel.waitingForNative || !panel.presented)return
            prepareTimeout.stop();panel.nativePrepared=true;panel.waitingForNative=false;showAnimation.restart()
        }
    }
    onActiveChanged: {
        if (active) hadFocus = true
        else if (hadFocus && closeOnDeactivate && visible) dismiss()
    }
    data: [
        Item {
            id: frame
            objectName: "panelFrame"
            Keys.onEscapePressed: event => {
                event.accepted = true
                panel.dismiss()
            }
            width: panel.frameWidth; height: panel.frameHeight
            x: (panel.width-width)/2
            y: (panel.height-height)/2
            opacity: panel.terminalHidden || !panel.nativePrepared && panel.nativeMotionActive ? 0 : panel.genieActive ? 1 : Math.min(1,panel.revealProgress*4)
            scale: panel.genieActive ? 1 : panel.animateToDock ? .055 + .945*panel.revealProgress : .965 + .035*panel.revealProgress
            layer.enabled: panel.genieActive ? !panel.nativeMotionActive : panel.transitionLayerActive
            layer.smooth: true
            layer.effect: ShaderEffect {
                objectName: "launcherGenieShader"
                property var source
                property real openness: panel.revealProgress
                property vector2d panelSize: Qt.vector2d(frame.width,frame.height)
                property vector4d iconRect: Qt.vector4d(panel.destinationIcon.x-frame.x,panel.destinationIcon.y-frame.y,panel.destinationIcon.width,panel.destinationIcon.height)
                mesh: GridMesh { resolution: Qt.size(32,64) }
                vertexShader: "qrc:/effects/panel-genie.vert.qsb"
                fragmentShader: "qrc:/effects/panel-genie.frag.qsb"
                onStatusChanged: if(status===ShaderEffect.Error) {console.warn("Launcher Genie shader:",log);panel.genieFailed=true}
            }
            transform: Translate {
                id: travel
                x: panel.animateToDock && !panel.genieActive ? (panel.animationAnchor.x-panel.width/2)*(1-panel.revealProgress) : 0
                y: panel.animateToDock && !panel.genieActive ? (panel.animationAnchor.y-panel.height/2)*(1-panel.revealProgress) : 0
            }
            VolumetricGlassSurface {
                objectName: "panelMaterial"
                visible: panel.glassMaterial
                anchors.fill: parent
                settings: panel.settings
                blurAvailable: panel.platform.blurAvailable
                cornerRadius: panel.panelRadius
                density: panel.materialDensity
                opticScale: panel.materialOpticScale
                finishStrength: panel.materialFinish
                backdropBlurRadius: panel.materialBlurRadius
                backdropRefractionStrength: panel.materialRefraction
                backdropRefractionWidth: panel.materialRefractionWidth
                pointer: Qt.vector2d(hover.point.position.x / Math.max(1,width), hover.point.position.y / Math.max(1,height))
                hoverAmount: hover.hovered ? 1 : 0
            }
            Rectangle {
                anchors.fill: parent
                visible: !panel.glassMaterial
                radius: panel.panelRadius
                color: panel.settings.theme === "light" || panel.settings.theme === "auto" && !panel.settings.systemDark ? "#f1f3f6" : "#242830"
                border.width: 1
                border.color: panel.settings.theme === "light" || panel.settings.theme === "auto" && !panel.settings.systemDark ? "#c9cfd8" : "#48505d"
            }
            HoverHandler { id: hover }
            Item {
                id: content
                anchors.fill: parent; anchors.margins: panel.contentPadding
                // Search must accept the first keystroke even while opening.
                enabled: panel.presented
            }
        },
        NumberAnimation {
            id: showAnimation
            target: panel; property: "revealProgress"; to: 1
            duration: panel.settings.reducedMotion ? 0 : panel.animateToDock ? 620/Math.pow(panel.settings.animationSpeed,.35) : 210/Math.min(1.5,panel.settings.animationSpeed)
            easing.type: panel.animateToDock ? Easing.InOutSine : Easing.OutCubic
        },
        NumberAnimation {
            id: hideAnimation
            target: panel; property: "revealProgress"; to: 0
            duration: panel.settings.reducedMotion ? 0 : panel.animateToDock ? 650/Math.pow(panel.settings.animationSpeed,.35) : 140/Math.min(1.5,panel.settings.animationSpeed)
            easing.type: panel.animateToDock ? Easing.InOutSine : Easing.InOutCubic
            onFinished: panel.completeDismiss()
        },
        Timer {
            id: prepareTimeout
            interval: 700
            onTriggered: {
                // Never expose an unredirected first frame if a compositor is
                // missing/unresponsive. The client mesh begins at opacity zero.
                panel.compositorMotion=false;panel.nativePrepared=true;panel.waitingForNative=false
                if(panel.presented)showAnimation.restart()
            }
        },
        Shortcut { sequence: "Esc"; enabled: panel.visible; onActivated: panel.dismiss() }
    ]
}
