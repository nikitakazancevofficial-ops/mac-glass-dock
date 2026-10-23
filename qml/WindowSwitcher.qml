pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock

GlassPanelWindow {
    id: panel
    objectName: "windowSwitcher"
    required property WindowTracker windows
    property string desktopId: ""
    property string appName: ""
    property string iconName: ""
    property bool anchorHovered: false
    property var windowList: []
    kind: "windows"
    focusOnOpen: false
    glassMaterial: true
    contentPadding: 14
    materialDensity: Math.max(1,.28 / Math.max(.01,settings.surfaceOpacity))
    materialOpticScale: .3
    materialFinish: .4
    materialBlurRadius: 2.5
    materialRefraction: Math.min(6,settings.refractionStrength)
    materialRefractionWidth: 10
    panelRadius: 22
    closeOnDeactivate: false
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowDoesNotAcceptFocus
    width: Math.min(screen.width-36, windowList.length*214+Math.max(0,windowList.length-1)*12+76)
    height: 228
    readonly property color ink: settings.theme === "light" || settings.theme === "auto" && !settings.systemDark ? "#172b42" : "#edf5ff"
    function refresh() {
        const next=windows.windowsFor(desktopId)
        if(JSON.stringify(next)!==JSON.stringify(windowList)) windowList=next
        if(windowList.length===0 && visible) dismiss()
    }
    function present(id,name,icon) {
        desktopId=id;appName=name;iconName=icon
        refresh()
        if(windowList.length>0) open()
    }
    Connections {target: panel.windows; function onChanged() {if(panel.visible)panel.refresh()}}
    Timer {
        interval: 550
        running: panel.visible && !panel.anchorHovered && !hover.hovered
        onTriggered: panel.dismiss()
    }
    HoverHandler {id: hover; parent: panel.contentItem}
    Column {
        anchors.fill: parent
        Flickable {
            id: viewport
            width: parent.width
            height: 152
            contentWidth: cards.width
            contentHeight: height
            clip: true
            boundsBehavior: Flickable.StopAtBounds
            Row {
                id: cards
                spacing: 12
                Repeater {
                    model: panel.windowList
                    delegate: Item {
                        id: card
                        required property var modelData
                        objectName: "windowCard"+modelData.token
                        width: 214
                        height: 152
                        Item {
                            x: 10; y: 10
                            width: parent.width-20; height: 102
                            Text {
                                anchors.centerIn: parent
                                width: parent.width-12
                                visible: !stream.preview || !stream.preview.ready
                                color: panel.ink
                                font.pixelSize: 12
                                opacity: .75
                                horizontalAlignment: Text.AlignHCenter
                                wrapMode: Text.WordWrap
                                text: !panel.windows.thumbnailAvailable ? "Предпросмотр недоступен в этом сеансе"
                                    : stream.status === Loader.Error || stream.preview && stream.preview.failed ? "Не удалось получить изображение окна"
                                    : "Подключение предпросмотра…"
                            }
                            Loader {
                                id: stream
                                property var preview: item
                                anchors.fill: parent
                                active: panel.visible && panel.windows.thumbnailAvailable
                                        && card.x+card.width>viewport.contentX && card.x<viewport.contentX+viewport.width
                                source: "components/WindowThumbnail.qml"
                                onLoaded: {
                                    preview.objectName="windowPreview"+card.modelData.token
                                    preview.uuid=card.modelData.uuid
                                }
                            }
                        }
                        Text {
                            objectName: "windowCaption"+card.modelData.token
                            x: 12; y: 118
                            width: parent.width-24
                            text: card.modelData.title
                            color: panel.ink
                            font.pixelSize: 12
                            font.weight: cardHover.hovered ? Font.DemiBold : Font.Normal
                            elide: Text.ElideRight
                        }
                        HoverHandler {id: cardHover; cursorShape: Qt.PointingHandCursor}
                        TapHandler {
                            onTapped: {panel.windows.perform(card.modelData.token,"activate");panel.dismiss()}
                        }
                        Accessible.role: Accessible.Button
                        Accessible.name: card.modelData.title
                        Accessible.onPressAction: {panel.windows.perform(card.modelData.token,"activate");panel.dismiss()}
                    }
                }
            }
        }
    }
}
