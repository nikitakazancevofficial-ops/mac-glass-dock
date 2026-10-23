pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

GlassPanelWindow {
    id: launcher
    objectName: "launcherWindow"
    required property ApplicationCatalogue catalogue
    signal settingsRequested
    property string contextDesktopId: ""
    property bool contextPinned: false
    kind: "launcher"
    title: "Приложения — Mac Glass Dock"
    // The transparent travel canvas has a moving, rounded input region: clicks
    // outside the actual panel still reach the desktop/application underneath.
    width: screen.width
    height: screen.height
    frameWidth: Math.min(922, screen.width - 64)
    frameHeight: Math.min(692, screen.height - 110)
    animateToDock: true
    materialDensity: Math.max(1,.58 / Math.max(.01,settings.surfaceOpacity))
    materialOpticScale: .65
    readonly property color ink: (settings.theme === "light" || (settings.theme === "auto" && !settings.systemDark)) ? "#172b42" : "#f0f7ff"
    onOpened: {
        search.forceActiveFocus();
        catalogue.refresh();
    }
    onRevealProgressChanged: if (presented && revealProgress >= .999) search.forceActiveFocus()

    Column {
        anchors.fill: parent
        spacing: 20
        Row {
            width: parent.width
            spacing: 12
            Text {
                style: Text.Outline
                styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                renderType: Text.QtRendering

                text: "Приложения"
                color: launcher.ink
                font.pixelSize: 28
                font.weight: Font.DemiBold
                width: parent.width - 120
                anchors.verticalCenter: parent.verticalCenter
            }
            GlassButton {
                settings: launcher.settings
                text: "Настройки"
                onClicked: {
                    launcher.dismiss();
                    launcher.settingsRequested();
                }
            }
        }
        Item {
            width: parent.width
            height: 48
            VolumetricGlassSurface {
                anchors.fill: parent
                settings: launcher.settings
                cornerRadius: 24
                density: .55
                opticScale: .55
            }
            TextField {
                id: search
                objectName: "launcherSearch"
                anchors.fill: parent
                anchors.leftMargin: 12
                anchors.rightMargin: 12
                placeholderText: "Поиск приложений"
                color: launcher.ink
                placeholderTextColor: "#9fb7d1"
                font.pixelSize: 16
                selectByMouse: true
                property bool caretOn: true
                cursorDelegate: Item {}
                Rectangle {
                    objectName: "launcherSearchCaret"
                    x: search.cursorRectangle.x
                    y: search.cursorRectangle.y
                    width: 2
                    height: search.cursorRectangle.height
                    radius: 1
                    color: launcher.ink
                    visible: search.activeFocus && launcher.presented && !search.readOnly
                    opacity: search.caretOn ? 1 : 0
                }
                Timer {
                    id: caretBlink
                    interval: 500; repeat: true
                    running: search.activeFocus && launcher.presented
                    onRunningChanged: search.caretOn = true
                    onTriggered: search.caretOn = !search.caretOn
                }
                onCursorPositionChanged: if(activeFocus) {caretOn=true;caretBlink.restart()}
                background: Item {}
                text: launcher.catalogue.query
                onTextEdited: {launcher.catalogue.query = text;caretOn=true;caretBlink.restart()}
                Keys.onEscapePressed: event => {
                    event.accepted = true
                    launcher.dismiss()
                }
            }
        }
        Flickable {
            width: parent.width
            height: 38
            contentWidth: categories.width
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: categories
                spacing: 9
                Repeater {
                    model: launcher.catalogue.categories
                    delegate: GlassButton {
                        required property string modelData
                        settings: launcher.settings
                        text: modelData
                        selected: launcher.catalogue.category === modelData
                        onClicked: launcher.catalogue.category = modelData
                    }
                }
            }
        }
        GridView {
            id: grid
            objectName: "applicationGrid"
            width: parent.width
            height: parent.height - y - 26
            clip: true
            cellWidth: width / Math.max(1, Math.floor(width / 120))
            cellHeight: 132
            cacheBuffer: 130
            boundsBehavior: Flickable.StopAtBounds
            model: launcher.catalogue
            ScrollBar.vertical: ScrollBar {
                policy: ScrollBar.AsNeeded
            }
            delegate: Item {
                id: tile
                objectName: "launcherResult"+index
                required property string appName
                required property int index
                required property string iconName
                required property string desktopId
                required property bool pinned
                width: grid.cellWidth
                height: grid.cellHeight
                property real arrival: 0
                opacity: arrival
                scale: .92 + .08*arrival
                transform: Translate { y: 8*(1-tile.arrival) }
                Component.onCompleted: resultArrival.start()
                SequentialAnimation {
                    id: resultArrival
                    PauseAnimation { duration: launcher.settings.reducedMotion ? 0 : Math.max(0,Math.min(tile.index,7))*18 }
                    NumberAnimation {
                        target: tile; property: "arrival"; to: 1
                        duration: launcher.settings.reducedMotion ? 0 : 190 / Math.min(1.5,launcher.settings.animationSpeed)
                        easing.type: Easing.OutCubic
                    }
                }
                AdaptiveAppIcon {
                    id: tileIcon
                    anchors.top: parent.top
                    anchors.topMargin: 6
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 70
                    height: 70
                    name: tile.iconName
                    settings: launcher.settings
                    pointer: Qt.vector2d((tileHover.point.position.x - x) / width, (tileHover.point.position.y - y) / height)
                    hoverAmount: tileHover.hovered ? 1 : 0
                    Behavior on hoverAmount { NumberAnimation { duration: launcher.settings.reducedMotion ? 0 : 80 } }
                    scale: appTap.pressed ? .91 : 1
                    Behavior on scale {
                        NumberAnimation {
                            duration: 90
                        }
                    }
                }
                Text {
                    style: Text.Outline
                    styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                    renderType: Text.QtRendering

                    x: 6
                    y: 84
                    width: parent.width - 12
                    height: 38
                    text: tile.appName
                    color: launcher.ink
                    font.pixelSize: 12
                    horizontalAlignment: Text.AlignHCenter
                    maximumLineCount: 2
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                }
                HoverHandler {
                    id: tileHover
                }
                TapHandler {
                    id: appTap
                    acceptedButtons: Qt.LeftButton
                    onTapped: {
                        launcher.catalogue.launch(tile.desktopId);
                        launcher.dismiss();
                    }
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: eventPoint => {
                        launcher.contextDesktopId = tile.desktopId;
                        launcher.contextPinned = tile.pinned;
                        appContextMenu.popup(tile.mapToGlobal(eventPoint.position));
                    }
                }
                GlassButton {
                    anchors.top: parent.top
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    width: 27
                    height: 27
                    settings: launcher.settings
                    text: tile.pinned ? "−" : "+"
                    visible: tileHover.hovered || tile.pinned
                    onClicked: launcher.catalogue.togglePin(tile.desktopId)
                    helpText: tile.pinned ? "Убрать из Dock" : "Закрепить в Dock"
                }
            }
            Text {
                style: Text.Outline
                styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                renderType: Text.QtRendering

                anchors.centerIn: parent
                visible: launcher.catalogue.count === 0
                text: launcher.catalogue.category === "Избранное"
                    ? "Избранное пусто — нажмите + на карточке приложения"
                    : "Ничего не найдено"
                color: launcher.ink
                font.pixelSize: 18
                width: parent.width - 48
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }

    Menu {
        id: appContextMenu
        MenuItem {
            text: launcher.contextPinned ? "Открепить из Dock" : "Закрепить в Dock"
            onTriggered: launcher.catalogue.togglePin(launcher.contextDesktopId)
        }
    }
}
