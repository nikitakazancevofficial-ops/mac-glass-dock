pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import MacGlassDock

GlassPanelWindow {
    id: panel
    objectName: "settingsWindow"
    required property WindowTracker windows
    kind: "settings"
    closeOnDeactivate: false
    width: Math.min(492, screen.width - 36)
    height: Math.min(880, screen.height - 70)
    title: "Стекло и Dock"
    property bool advancedOptics: false
    property bool iconOptics: false
    property bool backdropOptics: false
    onOpened: platform.desktop.apply("status")
    readonly property color ink: (settings.theme === "light" || (settings.theme === "auto" && !settings.systemDark)) ? "#172b42" : "#f0f7ff"

    Column {
        anchors.fill: parent
        spacing: 16
        Row {
            width: parent.width
            Text {
                style: Text.Outline
                styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                renderType: Text.QtRendering

                width: parent.width
                text: "Стекло и Dock"
                color: panel.ink
                font.pixelSize: 25
                font.weight: Font.DemiBold
                anchors.verticalCenter: parent.verticalCenter
            }
        }
        Row {
            width: parent.width
            spacing: 8
            Repeater {
                model: ["Со скринов", "Crystal Clear", "Soft Glass"]
                delegate: GlassButton {
                    required property string modelData
                    width: (panel.width - 112) / 3
                    settings: panel.settings
                    text: modelData
                    onClicked: modelData === "Со скринов" ? panel.settings.screenshotDefaults() : panel.settings.preset(modelData)
                }
            }
        }
        ScrollView {
            width: parent.width
            height: parent.height - y
            clip: true
            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
            Column {
                width: panel.width - 106
                spacing: 16
                Column {
                    width: parent.width
                    spacing: 8
                    Text {
                        text: "РАБОЧИЙ СТОЛ"
                        color: panel.ink
                        font.pixelSize: 11
                        font.letterSpacing: 2
                    }
                    Text {
                        width: parent.width
                        text: panel.platform.desktop.active
                            ? panel.platform.desktop.autostart ? "Наш Dock запускается при входе в KDE. Панель Plasma, часы и лоток остаются слева. Кнопка возврата отключит автозапуск и восстановит панель."
                                : "Dock заменён до следующего входа. Панель Plasma слева. Включите автозапуск ниже, чтобы сохранить этот режим после перезагрузки."
                            : "Сделать Dock основным: панель Plasma переместится влево. Без автозапуска прежний вид восстановится при следующем входе."
                        color: panel.ink
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                    GlassButton {
                        objectName: "desktopReplacementButton"
                        width: parent.width
                        settings: panel.settings
                        text: panel.platform.desktop.active ? "Вернуть панель Plasma" : "Сделать основным Dock"
                        enabled: !panel.platform.desktop.busy && !panel.platform.preview
                        onClicked: panel.platform.desktop.apply(panel.platform.desktop.active ? "restore" : "activate")
                    }
                    GlassButton {
                        objectName: "dockAutostartButton"
                        width: parent.width
                        settings: panel.settings
                        text: panel.platform.desktop.autostart ? "Автозапуск включён — отключить" : "Запускать Dock при входе в KDE"
                        enabled: !panel.platform.desktop.busy && !panel.platform.preview
                        onClicked: panel.platform.desktop.apply(panel.platform.desktop.autostart ? "disable-autostart" : "enable-autostart")
                    }
                    GlassButton {
                        width: parent.width
                        settings: panel.settings
                        text: panel.platform.desktop.decorated ? "Вернуть прежние кнопки окон" : "Круглые кнопки и скругления окон"
                        enabled: !panel.platform.desktop.busy && !panel.platform.preview
                        onClicked: panel.platform.desktop.apply(panel.platform.desktop.decorated ? "restore-decoration" : "decorate")
                    }
                    Text {
                        width: parent.width
                        visible: text.length > 0
                        text: panel.platform.desktop.error
                        color: panel.ink
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
                Item {
                    width: parent.width
                    height: optical.height + 32
                    VolumetricGlassSurface {
                        anchors.fill: parent
                        settings: panel.settings
                        cornerRadius: 27
                        density: .8
                    }
                    Column {
                        id: optical
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 2
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            text: "МАТЕРИАЛ"
                            color: panel.ink
                            font.pixelSize: 11
                            font.letterSpacing: 2
                            height: 26
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Плотность стекла"
                            settingKey: "SurfaceOpacity"
                            currentValue: panel.settings.surfaceOpacity
                            from: .04
                            to: .65
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Толщина кромки"
                            settingKey: "Thickness"
                            currentValue: panel.settings.thickness
                            from: 3
                            to: 24
                            stepSize: 1
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Отражение по краю"
                            settingKey: "EdgeReflection"
                            currentValue: panel.settings.edgeReflection
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Световые блики"
                            settingKey: "HighlightIntensity"
                            currentValue: panel.settings.highlightIntensity
                        }
                        GlassButton { settings: panel.settings; text: panel.advancedOptics ? "Скрыть тонкую настройку" : "Больше настроек кромки"; onClicked: panel.advancedOptics = !panel.advancedOptics }
                        Column { width: parent.width; visible: panel.advancedOptics; spacing: 2
GlassSlider { settings: panel.settings; label: "Ширина светлого контура"; settingKey: "RimWidth"; currentValue: panel.settings.rimWidth; from: 0.3; to: 3; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Профиль объёмной кромки"; settingKey: "BevelCurve"; currentValue: panel.settings.bevelCurve; from: 0.3; to: 4; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Угол отражения (Fresnel)"; settingKey: "FresnelPower"; currentValue: panel.settings.fresnelPower; from: 0.5; to: 6; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Цветовой сдвиг кромки"; settingKey: "EdgeDispersion"; currentValue: panel.settings.edgeDispersion; from: 0; to: 0.12; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Внутренняя тень"; settingKey: "InnerShadow"; currentValue: panel.settings.innerShadow; from: 0; to: 0.3; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Плотность внешней тени"; settingKey: "ShadowOpacity"; currentValue: panel.settings.shadowOpacity; from: 0; to: 0.4; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Мягкость внешней тени"; settingKey: "ShadowRadius"; currentValue: panel.settings.shadowRadius; from: 2; to: 24; stepSize: 1; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Размер светового блика"; settingKey: "HighlightSize"; currentValue: panel.settings.highlightSize; from: 40; to: 360; stepSize: 1; enabled: true; opacity: enabled ? 1 : .4 }
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "BlurEnabled"
                            text: "Размытие фона KWin"
                            checked: panel.settings.blurEnabled
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Мягкое размытие стекла, px"
                            settingKey: "BlurRadius"
                            currentValue: panel.settings.blurRadius
                            from: 0
                            to: 12
                            stepSize: .05
                            enabled: panel.platform.refractionAvailable && panel.settings.blurEnabled
                            opacity: enabled ? 1 : .4
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "GpuEffects"
                            text: "GPU-оптика"
                            checked: panel.settings.gpuEffects
                        }
                        Row {
                            spacing: 9
                            height: 38
                            Repeater {
                                model: ["#a7cbe5", "#b8b7e6", "#a3d6ce", "#e7c6ad", "#d5dce5"]
                                delegate: Rectangle {
                                    required property string modelData
                                    width: 29
                                    height: 29
                                    radius: 15
                                    color: modelData
                                    border.width: panel.settings.tint.toString() === modelData ? 2 : 0
                                    border.color: "white"
                                    TapHandler {
                                        onTapped: panel.settings.setValue("Tint", parent.modelData)
                                    }
                                }
                            }
                        }
                        Row {
                            spacing: 8
                            Repeater {
                                model: ["auto", "light", "dark"]
                                delegate: GlassButton {
                                    required property string modelData
                                    settings: panel.settings
                                    text: modelData === "auto" ? "Авто" : modelData === "light" ? "Светлая" : "Тёмная"
                                    selected: panel.settings.theme === modelData
                                    onClicked: panel.settings.setValue("Theme", modelData)
                                }
                            }
                        }
                    }
                }
                Item {
                    width: parent.width
                    height: dockControls.height + 32
                    VolumetricGlassSurface {
                        anchors.fill: parent
                        settings: panel.settings
                        cornerRadius: 27
                        density: .8
                    }
                    Column {
                        id: dockControls
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 2
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            text: "ПОВЕДЕНИЕ"
                            color: panel.ink
                            font.pixelSize: 11
                            font.letterSpacing: 2
                            height: 26
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Размер иконок"
                            settingKey: "IconSize"
                            currentValue: panel.settings.iconSize
                            from: 24
                            to: 96
                            stepSize: 1
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Увеличение"
                            settingKey: "MaxScale"
                            currentValue: panel.settings.maxScale
                            from: 1
                            to: 2
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Скорость анимации"
                            settingKey: "AnimationSpeed"
                            currentValue: panel.settings.animationSpeed
                            from: .25
                            to: 3
                            stepSize: .05
                        }
                        GlassSlider {
                            settings: panel.settings
                            label: "Демпфирование"
                            settingKey: "SpringDamping"
                            currentValue: panel.settings.springDamping
                            from: .25
                            to: 1
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "AutoHide"
                            text: "Автоскрытие Dock"
                            checked: panel.settings.autoHide
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "SmartAutoHide"
                            text: "Скрывать при перекрытии окном"
                            checked: panel.settings.smartAutoHide
                            enabled: panel.settings.autoHide
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "BounceEnabled"
                            text: "Подпрыгивание при запуске"
                            checked: panel.settings.bounceEnabled
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "ReducedMotion"
                            text: "Уменьшить движение"
                            checked: panel.settings.reducedMotion
                        }
                        Row {
                            spacing: 6
                            Repeater {
                                model: ["original", "adaptive", "glass"]
                                delegate: GlassButton {
                                    required property string modelData
                                    settings: panel.settings
                                    text: modelData === "original" ? "Original" : modelData === "adaptive" ? "Rounded" : "Glass"
                                    selected: panel.settings.iconStyle === modelData
                                    onClicked: panel.settings.setValue("IconStyle", modelData)
                                }
                            }
                        }
                    }
                }
                Item {
                    width: parent.width
                    height: extraOptics.height + 32
                    VolumetricGlassSurface { anchors.fill: parent; settings: panel.settings; cornerRadius: 27; density: .8 }
                    Column {
                        id: extraOptics; x: 16; y: 16; width: parent.width - 32; spacing: 4
                        GlassButton { width: parent.width; settings: panel.settings; text: panel.iconOptics ? "Стекло под иконками ▴" : "Стекло под иконками ▾"; onClicked: panel.iconOptics = !panel.iconOptics }
                        Column { width: parent.width; visible: panel.iconOptics; spacing: 2
                            GlassSwitch { width: parent.width; settings: panel.settings; settingKey: "IconBlurEnabled"; text: "Отдельные области blur под иконками"; checked: panel.settings.iconBlurEnabled }
GlassSlider { settings: panel.settings; label: "Плотность стекла иконок"; settingKey: "IconGlassOpacity"; currentValue: panel.settings.iconGlassOpacity; from: 0; to: 0.5; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Толщина стекла иконок"; settingKey: "IconThickness"; currentValue: panel.settings.iconThickness; from: 1; to: 24; stepSize: 1; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Скругление иконок"; settingKey: "IconRadius"; currentValue: panel.settings.iconRadius; from: 0.1; to: 0.5; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Поля вокруг логотипа"; settingKey: "IconPadding"; currentValue: panel.settings.iconPadding; from: 0; to: 0.25; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Отражение под иконкой"; settingKey: "IconReflection"; currentValue: panel.settings.iconReflection; from: 0; to: 1.5; stepSize: 0.01; enabled: true; opacity: enabled ? 1 : .4 }
                        }
                        GlassButton { width: parent.width; settings: panel.settings; text: panel.backdropOptics ? "Линзы и слабый blur KWin ▴" : "Линзы и слабый blur KWin ▾"; onClicked: panel.backdropOptics = !panel.backdropOptics }
                        Text { width: parent.width; text: panel.platform.refractionAvailable ? "KWin: настоящее преломление подключено" : "Точный blur и преломление требуют отдельного эффекта KWin. Сейчас работает обычный системный blur."; color: panel.ink; font.pixelSize: 12; wrapMode: Text.WordWrap }
                        Column { width: parent.width; visible: panel.backdropOptics; spacing: 2
GlassSlider { settings: panel.settings; label: "Искажение у кромки, px"; settingKey: "RefractionStrength"; currentValue: panel.settings.refractionStrength; from: 0; to: 40; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Ширина зоны искажения"; settingKey: "RefractionWidth"; currentValue: panel.settings.refractionWidth; from: 2; to: 60; stepSize: 1; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Профиль линзы"; settingKey: "RefractionCurve"; currentValue: panel.settings.refractionCurve; from: 0.25; to: 4; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Дисперсия фоновой картинки"; settingKey: "ChromaticAberration"; currentValue: panel.settings.chromaticAberration; from: 0; to: 3; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Увеличение фона в центре"; settingKey: "CenterZoom"; currentValue: panel.settings.centerZoom; from: 0.9; to: 1.15; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Дополнительный blur иконок"; settingKey: "IconBlurRadius"; currentValue: panel.settings.iconBlurRadius; from: 0; to: 12; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Искажение под иконками, px"; settingKey: "IconRefractionStrength"; currentValue: panel.settings.iconRefractionStrength; from: 0; to: 40; stepSize: 0.01; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
GlassSlider { settings: panel.settings; label: "Ширина линзы иконок"; settingKey: "IconRefractionWidth"; currentValue: panel.settings.iconRefractionWidth; from: 1; to: 32; stepSize: 1; enabled: panel.platform.refractionAvailable; opacity: enabled ? 1 : .4 }
                        }
                    }
                }
                Item {
                    width: parent.width
                    height: integration.height + 32
                    VolumetricGlassSurface {
                        anchors.fill: parent
                        settings: panel.settings
                        cornerRadius: 27
                        density: .8
                    }
                    Column {
                        id: integration
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 8
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "WindowIntegration"
                            text: "Показывать запущенные приложения"
                            checked: panel.settings.windowIntegration
                        }
                        GlassSwitch {
                            width: parent.width
                            settings: panel.settings
                            settingKey: "WindowAnimations"
                            text: "Появление и закрытие окон"
                            checked: panel.settings.windowAnimations
                            enabled: panel.settings.windowIntegration && panel.platform.refractionAvailable && !panel.settings.reducedMotion
                            opacity: enabled ? 1 : .45
                        }
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            width: parent.width
                            text: panel.windows.status
                            color: panel.ink
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            text: "Экран Dock"
                            color: panel.ink
                            font.pixelSize: 12
                        }
                        Repeater {
                            model: panel.platform.screenNames
                            delegate: GlassButton {
                                required property string modelData
                                width: integration.width
                                settings: panel.settings
                                text: modelData
                                selected: panel.settings.screenName === modelData
                                onClicked: panel.settings.setValue("ScreenName", modelData)
                            }
                        }
                        Text {
                            style: Text.Outline
                            styleColor: color.hslLightness > .5 ? "#80101a28" : "#70ffffff"
                            renderType: Text.QtRendering
                            width: parent.width
                            text: panel.settings.saveError
                            visible: text.length > 0
                            color: "#ffb6b6"
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
