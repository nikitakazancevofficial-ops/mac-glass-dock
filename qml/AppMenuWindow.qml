pragma ComponentBehavior: Bound
import QtQuick
import MacGlassDock

GlassPanelWindow {
    id: menu
    objectName: "appMenuWindow"
    required property ApplicationModel applications
    required property WindowTracker windows
    property string desktopId: ""
    property string appName: ""
    property bool pinned: false
    property var windowList: []
    signal settingsRequested
    kind: "menu"
    width: 370
    height: Math.min(620, 260 + windowList.length * 94)
    title: "Действия приложения"
    function refreshWindows() {
        windowList = windows.windowsFor(desktopId);
    }
    onOpened: refreshWindows()
    Connections {
        target: menu.windows
        function onChanged() {
            menu.refreshWindows();
        }
    }
    Column {
        anchors.fill: parent
        spacing: 12
        Text {
            style: Text.Outline
            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
            renderType: Text.QtRendering
            width: parent.width
            text: menu.appName
            elide: Text.ElideRight
            color: (menu.settings.theme === "light" || (menu.settings.theme === "auto" && !menu.settings.systemDark)) ? "#172b42" : "#f0f7ff"
            font.pixelSize: 19
            font.weight: Font.DemiBold
        }
        GlassButton {
            width: parent.width
            settings: menu.settings
            text: "Открыть новое окно"
            enabled: menu.desktopId.length > 0
            onClicked: {
                menu.applications.launchDesktop(menu.desktopId);
                menu.dismiss();
            }
        }
        GlassButton {
            objectName: "pinApplicationButton"
            width: parent.width
            settings: menu.settings
            text: menu.pinned ? "✓ Закреплено — убрать из Dock" : "Закрепить в Dock"
            enabled: menu.desktopId.length > 0
            onClicked: {
                menu.applications.togglePin(menu.desktopId);
                menu.dismiss();
            }
        }
        Flickable {
            width: parent.width
            height: Math.min(320, Math.max(0, menu.height - 260))
            contentHeight: windowColumn.height
            clip: true
            Column {
                id: windowColumn
                width: parent.width
                spacing: 8
                Repeater {
                    model: menu.windowList
                    delegate: Column {
                        id: entry
                        required property var modelData
                        width: windowColumn.width
                        spacing: 5
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            width: parent.width
                            text: entry.modelData.title
                            elide: Text.ElideRight
                            color: (menu.settings.theme === "light" || (menu.settings.theme === "auto" && !menu.settings.systemDark)) ? "#172b42" : "#edf5ff"
                            font.pixelSize: 12
                        }
                        Row {
                            spacing: 8
                            GlassButton {
                                settings: menu.settings
                                text: entry.modelData.minimized ? "Восстановить" : "Показать"
                                onClicked: {
                                    menu.windows.perform(entry.modelData.token, "activate");
                                    menu.dismiss();
                                }
                            }
                            GlassButton {
                                settings: menu.settings
                                text: "−"
                                enabled: entry.modelData.canMinimize && !entry.modelData.minimized
                                onClicked: {
                                    menu.windows.perform(entry.modelData.token, "minimize");
                                    menu.refreshWindows();
                                }
                            }
                            GlassButton {
                                settings: menu.settings
                                text: "Закрыть окно"
                                enabled: entry.modelData.canClose
                                onClicked: menu.windows.perform(entry.modelData.token, "close")
                            }
                        }
                    }
                }
            }
        }
        Row {
            spacing: 10
            GlassButton {
                settings: menu.settings
                text: "Настройки"
                onClicked: {
                    menu.dismiss();
                    menu.settingsRequested();
                }
            }
            GlassButton {
                settings: menu.settings
                text: "Выход"
                onClicked: Qt.quit()
            }
        }
    }
}
