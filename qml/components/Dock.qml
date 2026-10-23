pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock

Item {
    id: dock
    objectName: "dock"
    required property DockSettings settings
    required property ApplicationModel applications
    required property DockPlatform platform
    property real baseSize: settings.iconSize
    readonly property real spacing: settings.spacing
    readonly property real maximumScale: settings.maxScale
    property real pointerX: width / 2
    property bool hoverSuppressed: false
    property bool tooltipsSuppressed: false
    readonly property point launcherAnchor: launcherItem.mapToItem(dock, launcherItem.width/2, launcherItem.height/2)
    property bool pointerInside: hover.hovered && !hoverSuppressed
    function cancelHover() {
        hoverSuppressed = true
        pointerX = width / 2
        referencePointer = referenceWidth / 2
    }
    signal launcherRequested()
    signal settingsRequested()
    signal appHoverChanged(string desktopId, bool inside)
    signal windowsRequested(string desktopId, string appName, string iconName)
    signal appContextRequested(string desktopId, string appName, bool pinned)
    readonly property real referenceWidth: (applications.count + 2) * baseSize + (applications.count + 1) * spacing
    // Stable center-relative coordinates: resizing the layer surface must not
    // feed magnification back into the cursor's base-space position.
    property real referencePointer: referenceWidth / 2
    function influence(slot: int): real {
        if (!pointerInside || settings.reducedMotion) return 1
        const center = slot * (baseSize + spacing) + baseSize / 2
        const distance = (referencePointer - center) / (baseSize + spacing)
        return 1 + (maximumScale - 1) * Math.exp(-.5 * distance * distance)
    }
    readonly property rect surfaceRect: Qt.rect(4, height - baseSize - 36, width - 8, baseSize + 28)
    readonly property var iconRects: {
        let regions = []
        for (let item of [launcherItem, settingsItem])
            regions.push(Qt.rect(row.x + item.x, row.y + item.y, item.width, item.height))
        for (let i = 0; i < icons.count; ++i) {
            let item = icons.itemAt(i)
            if (item) regions.push(Qt.rect(row.x + item.x, row.y + item.y, item.width, item.height))
        }
        return regions
    }
    readonly property var applicationAnchors: {
        let targets = []
        for (let i=0; i<icons.count; ++i) {
            let item=icons.itemAt(i)
            if(item) targets.push({id:dock.applications.desktopId(i), rect:Qt.rect(row.x+item.x,row.y+item.y,item.width,item.height)})
        }
        return targets
    }
    signal geometryUpdated()
    width: Math.max(80, row.width + 36)
    height: Math.ceil(baseSize * maximumScale + 62)
    onWidthChanged: Qt.callLater(geometryUpdated)
    onIconRectsChanged: Qt.callLater(geometryUpdated)
    onApplicationAnchorsChanged: Qt.callLater(geometryUpdated)

    GlassSurface {
        settings: dock.settings
        platform: dock.platform
        x: dock.surfaceRect.x; y: dock.surfaceRect.y
        width: dock.surfaceRect.width; height: dock.surfaceRect.height
        cornerRadius: Math.min(30, height / 2)
        pointer: Qt.vector2d(dock.pointerX / Math.max(1, dock.width), 0.35)
        hoverAmount: dock.pointerInside ? 1 : 0
        // Surface optics are pointer-owned state; clearing them immediately
        // avoids the short right-edge afterimage when KWin reports leave.
        Behavior on hoverAmount { NumberAnimation { duration: dock.settings.reducedMotion || !dock.pointerInside ? 0 : 180 } }
    }

    Row {
        id: row
        objectName: "iconRow"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 23
        height: dock.baseSize * dock.maximumScale
        spacing: dock.spacing

        DockItem {
            id: launcherItem
            objectName: "launcherButton"
            settings: dock.settings
            hoverEnabled: dock.pointerInside
            tooltipSuppressed: dock.tooltipsSuppressed
            baseSize: dock.baseSize
            y: row.height - height
            label: "Приложения"; icon: "view-grid"
            animationSpeed: dock.settings.animationSpeed
            targetScale: dock.influence(0)
            onActivated: dock.launcherRequested()
            onContextRequested: dock.appContextRequested("", "Mac Glass Dock", false)
        }

        Repeater {
            id: icons
            model: dock.applications
            delegate: DockItem {
                settings: dock.settings
                hoverEnabled: dock.pointerInside
                tooltipSuppressed: dock.tooltipsSuppressed
                pointerSample: Qt.point(dock.referencePointer,hover.point.position.y)
                required property int index
                required property string appName
                required property string iconName
                required property bool available
                required property bool launching
                required property string launchError
                required property string desktopId
                required property int runningCount
                required property bool activeApp
                required property bool pinned
                objectName: "dockItem" + index
                y: row.height - height
                label: launchError.length ? appName + " — " + launchError : appName
                icon: iconName
                appAvailable: available
                busy: launching
                windowCount: runningCount
                activeApplication: activeApp
                baseSize: dock.baseSize
                animationSpeed: dock.settings.animationSpeed
                targetScale: dock.influence(index + 1)
                onActivated: dock.applications.activate(index)
                onHoverChanged: inside => dock.appHoverChanged(desktopId, inside)
                onWindowsRequested: dock.windowsRequested(desktopId, appName, iconName)
                onContextRequested: dock.appContextRequested(desktopId, appName, pinned)
            }
        }
        DockItem {
            id: settingsItem
            objectName: "settingsButton"
            settings: dock.settings
            hoverEnabled: dock.pointerInside
            tooltipSuppressed: dock.tooltipsSuppressed
            baseSize: dock.baseSize
            y: row.height - height
            label: "Стекло и Dock"; icon: "preferences-system"
            animationSpeed: dock.settings.animationSpeed
            targetScale: dock.influence(dock.applications.count + 1)
            onActivated: dock.settingsRequested()
            onContextRequested: dock.appContextRequested("", "Mac Glass Dock", false)
        }
    }

    HoverHandler {
        id: hover
        onHoveredChanged: {
            dock.hoverSuppressed = !hovered
            if(!hovered) dock.cancelHover()
        }
        onPointChanged: {
            if(hover.hovered) dock.hoverSuppressed=false
            dock.pointerX = point.position.x
            dock.referencePointer = point.position.x - dock.width / 2 + dock.referenceWidth / 2
        }
    }
}
