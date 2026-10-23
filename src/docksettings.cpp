#include "docksettings.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStyleHints>
#include <cmath>

DockSettings::DockSettings(QObject *parent) : QObject(parent)
{
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged,
            this, &DockSettings::systemThemeChanged);
    fileName = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation)
        + QStringLiteral("/mac-glass-dock/settings.ini");
    read();
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(250);
    connect(&m_saveTimer, &QTimer::timeout, this, &DockSettings::save);
}

DockSettings::~DockSettings() { flush(); }
void DockSettings::flush() { if (m_saveTimer.isActive()) { m_saveTimer.stop(); save(); } }
void DockSettings::scheduleSave() { emit changed(); m_saveTimer.start(); }

void DockSettings::read()
{
    QSettings c(fileName, QSettings::IniFormat);
    const auto number = [&c](const QString &key, qreal fallback, qreal min, qreal max) {
        bool ok = false;
        const qreal value = c.value(QStringLiteral("Appearance/") + key, fallback).toDouble(&ok);
        return ok && std::isfinite(value) ? qBound(min, value, max) : fallback;
    };
    iconSize = number("IconSize", 56, 24, 96);
    maxScale = number("MaxScale", maxScale, 1, 2);
    spacing = number("Spacing", 10, 4, 24);
    animationSpeed = number("AnimationSpeed", animationSpeed, 0.25, 3);
    surfaceOpacity = number("SurfaceOpacity", surfaceOpacity, 0.04, 0.85);
    highlightIntensity = number("HighlightIntensity", highlightIntensity, 0, 1);
    thickness = number("Thickness", thickness, 3, 24);
    edgeReflection = number("EdgeReflection", edgeReflection, 0, 1);
    springDamping = number("SpringDamping", springDamping, 0.25, 1);
    rimWidth = number("RimWidth", rimWidth, 0.3, 3);
    bevelCurve = number("BevelCurve", bevelCurve, 0.3, 4);
    fresnelPower = number("FresnelPower", fresnelPower, 0.5, 6);
    edgeDispersion = number("EdgeDispersion", edgeDispersion, 0, 0.12);
    innerShadow = number("InnerShadow", innerShadow, 0, 0.3);
    shadowOpacity = number("ShadowOpacity", shadowOpacity, 0, 0.4);
    shadowRadius = number("ShadowRadius", shadowRadius, 2, 24);
    highlightSize = number("HighlightSize", highlightSize, 40, 360);
    iconGlassOpacity = number("IconGlassOpacity", iconGlassOpacity, 0, 0.5);
    iconThickness = number("IconThickness", iconThickness, 1, 24);
    iconRadius = number("IconRadius", iconRadius, 0.1, 0.5);
    iconPadding = number("IconPadding", iconPadding, 0, 0.25);
    iconReflection = number("IconReflection", iconReflection, 0, 1.5);
    blurRadius = number("BlurRadius", blurRadius, 0, 12);
    refractionStrength = number("RefractionStrength", refractionStrength, 0, 40);
    refractionWidth = number("RefractionWidth", refractionWidth, 2, 60);
    refractionCurve = number("RefractionCurve", refractionCurve, 0.25, 4);
    chromaticAberration = number("ChromaticAberration", chromaticAberration, 0, 3);
    centerZoom = number("CenterZoom", centerZoom, 0.9, 1.15);
    iconBlurRadius = number("IconBlurRadius", iconBlurRadius, 0, 12);
    iconRefractionStrength = number("IconRefractionStrength", iconRefractionStrength, 0, 40);
    iconRefractionWidth = number("IconRefractionWidth", iconRefractionWidth, 1, 32);
    iconBlurEnabled = c.value("Appearance/IconBlurEnabled", true).toBool();
    gpuEffects = c.value("Appearance/GpuEffects", true).toBool();
    blurEnabled = c.value("Appearance/BlurEnabled", true).toBool();
    autoHide = c.value("Appearance/AutoHide", true).toBool();
    reducedMotion = c.value("Appearance/ReducedMotion", false).toBool();
    bounceEnabled = c.value("Appearance/BounceEnabled", true).toBool();
    windowIntegration = c.value("Appearance/WindowIntegration", false).toBool();
    windowAnimations = c.value("Appearance/WindowAnimations", true).toBool();
    smartAutoHide = c.value("Appearance/SmartAutoHide", true).toBool();
    theme = c.value("Appearance/Theme", "auto").toString();
    if (theme != "light" && theme != "dark") theme = "auto";
    iconStyle = c.value("Appearance/IconStyle", "glass").toString();
    if (iconStyle != "original" && iconStyle != "glass") iconStyle = "adaptive";
    screenName = c.value("Appearance/ScreenName").toString();
    const QColor color(c.value("Appearance/Tint", "#a7cbe5").toString());
    tint = color.isValid() ? color : QColor("#a7cbe5");
    pinnedApps = c.value("Applications/Pinned", QStringList{
        "org.kde.dolphin.desktop", "firefox.desktop", "org.kde.konsole.desktop",
        "org.kde.kate.desktop", "systemsettings.desktop"}).toStringList();
    pinnedApps.removeDuplicates();
    pinnedApps = pinnedApps.mid(0, 24);
}

void DockSettings::save()
{
    QDir().mkpath(QFileInfo(fileName).absolutePath());
    QSettings c(fileName, QSettings::IniFormat);
#define SAVE(name, member) c.setValue(QStringLiteral("Appearance/" name), member)
    SAVE("IconSize", iconSize); SAVE("MaxScale", maxScale); SAVE("Spacing", spacing);
    SAVE("AnimationSpeed", animationSpeed); SAVE("SurfaceOpacity", surfaceOpacity);
    SAVE("HighlightIntensity", highlightIntensity); SAVE("Thickness", thickness);
    SAVE("EdgeReflection", edgeReflection); SAVE("SpringDamping", springDamping);
    SAVE("GpuEffects", gpuEffects); SAVE("BlurEnabled", blurEnabled); SAVE("AutoHide", autoHide);
    SAVE("ReducedMotion", reducedMotion); SAVE("BounceEnabled", bounceEnabled);
    SAVE("WindowIntegration", windowIntegration); SAVE("Theme", theme); SAVE("IconStyle", iconStyle);
    SAVE("WindowAnimations", windowAnimations);
    SAVE("SmartAutoHide", smartAutoHide);
    SAVE("ScreenName", screenName); SAVE("Tint", tint.name());
    SAVE("RimWidth", rimWidth);
    SAVE("BevelCurve", bevelCurve);
    SAVE("FresnelPower", fresnelPower);
    SAVE("EdgeDispersion", edgeDispersion);
    SAVE("InnerShadow", innerShadow);
    SAVE("ShadowOpacity", shadowOpacity);
    SAVE("ShadowRadius", shadowRadius);
    SAVE("HighlightSize", highlightSize);
    SAVE("IconGlassOpacity", iconGlassOpacity);
    SAVE("IconThickness", iconThickness);
    SAVE("IconRadius", iconRadius);
    SAVE("IconPadding", iconPadding);
    SAVE("IconReflection", iconReflection);
    SAVE("BlurRadius", blurRadius);
    SAVE("RefractionStrength", refractionStrength);
    SAVE("RefractionWidth", refractionWidth);
    SAVE("RefractionCurve", refractionCurve);
    SAVE("ChromaticAberration", chromaticAberration);
    SAVE("CenterZoom", centerZoom);
    SAVE("IconBlurRadius", iconBlurRadius);
    SAVE("IconRefractionStrength", iconRefractionStrength);
    SAVE("IconRefractionWidth", iconRefractionWidth);
    SAVE("IconBlurEnabled", iconBlurEnabled);
#undef SAVE
    c.setValue("Applications/Pinned", pinnedApps);
    c.sync();
    m_saveError = c.status() == QSettings::NoError ? QString() : tr("Не удалось сохранить настройки");
    emit changed();
}

void DockSettings::setValue(const QString &key, const QVariant &value)
{
    bool ok = false;
    const qreal v = value.toDouble(&ok);
#define NUMBER(name, member, lo, hi) if (key == name) { if (!ok || !std::isfinite(v)) return; member = qBound(qreal(lo), v, qreal(hi)); }
    NUMBER("IconSize", iconSize, 24, 96)
    else NUMBER("MaxScale", maxScale, 1, 2)
    else NUMBER("Spacing", spacing, 4, 24)
    else NUMBER("AnimationSpeed", animationSpeed, .25, 3)
    else NUMBER("SurfaceOpacity", surfaceOpacity, .04, .85)
    else NUMBER("HighlightIntensity", highlightIntensity, 0, 1)
    else NUMBER("Thickness", thickness, 3, 24)
    else NUMBER("EdgeReflection", edgeReflection, 0, 1)
    else NUMBER("SpringDamping", springDamping, .25, 1)
    else NUMBER("RimWidth", rimWidth, 0.3, 3)
    else NUMBER("BevelCurve", bevelCurve, 0.3, 4)
    else NUMBER("FresnelPower", fresnelPower, 0.5, 6)
    else NUMBER("EdgeDispersion", edgeDispersion, 0, 0.12)
    else NUMBER("InnerShadow", innerShadow, 0, 0.3)
    else NUMBER("ShadowOpacity", shadowOpacity, 0, 0.4)
    else NUMBER("ShadowRadius", shadowRadius, 2, 24)
    else NUMBER("HighlightSize", highlightSize, 40, 360)
    else NUMBER("IconGlassOpacity", iconGlassOpacity, 0, 0.5)
    else NUMBER("IconThickness", iconThickness, 1, 24)
    else NUMBER("IconRadius", iconRadius, 0.1, 0.5)
    else NUMBER("IconPadding", iconPadding, 0, 0.25)
    else NUMBER("IconReflection", iconReflection, 0, 1.5)
    else NUMBER("BlurRadius", blurRadius, 0, 12)
    else NUMBER("RefractionStrength", refractionStrength, 0, 40)
    else NUMBER("RefractionWidth", refractionWidth, 2, 60)
    else NUMBER("RefractionCurve", refractionCurve, 0.25, 4)
    else NUMBER("ChromaticAberration", chromaticAberration, 0, 3)
    else NUMBER("CenterZoom", centerZoom, 0.9, 1.15)
    else NUMBER("IconBlurRadius", iconBlurRadius, 0, 12)
    else NUMBER("IconRefractionStrength", iconRefractionStrength, 0, 40)
    else NUMBER("IconRefractionWidth", iconRefractionWidth, 1, 32)
#undef NUMBER
    else if (key == "IconBlurEnabled") iconBlurEnabled = value.toBool();
    else if (key == "GpuEffects") gpuEffects = value.toBool();
    else if (key == "BlurEnabled") blurEnabled = value.toBool();
    else if (key == "AutoHide") autoHide = value.toBool();
    else if (key == "ReducedMotion") reducedMotion = value.toBool();
    else if (key == "BounceEnabled") bounceEnabled = value.toBool();
    else if (key == "WindowIntegration") windowIntegration = value.toBool();
    else if (key == "WindowAnimations") windowAnimations = value.toBool();
    else if (key == "SmartAutoHide") smartAutoHide = value.toBool();
    else if (key == "Theme" && QStringList{"auto", "light", "dark"}.contains(value.toString())) theme = value.toString();
    else if (key == "IconStyle" && QStringList{"original", "adaptive", "glass"}.contains(value.toString())) iconStyle = value.toString();
    else if (key == "Tint" && QColor(value.toString()).isValid()) tint = QColor(value.toString());
    else if (key == "ScreenName") screenName = value.toString();
    else return;
    scheduleSave();
}

void DockSettings::screenshotDefaults()
{
    iconSize=56; maxScale=1.5; animationSpeed=2.55; springDamping=.46;
    surfaceOpacity=.04; thickness=24; edgeReflection=1; highlightIntensity=.84;
    autoHide=true; bounceEnabled=true; reducedMotion=false;
    // Appearance presets must not disable the user's running-window integration.
    blurEnabled=true; gpuEffects=true;
    theme="auto"; iconStyle="glass"; tint=QColor("#a7cbe5"); iconBlurEnabled=true;
    rimWidth=1.2;
    bevelCurve=1.35;
    fresnelPower=2.1;
    edgeDispersion=0.025;
    innerShadow=0.06;
    shadowOpacity=0.16;
    shadowRadius=8;
    highlightSize=170;
    iconGlassOpacity=0.08;
    iconThickness=12;
    iconRadius=0.25;
    iconPadding=0.105;
    iconReflection=1;
    blurRadius=2;
    refractionStrength=12;
    refractionWidth=24;
    refractionCurve=1.6;
    chromaticAberration=0.5;
    centerZoom=1;
    iconBlurRadius=3;
    iconRefractionStrength=10;
    iconRefractionWidth=14;
    scheduleSave();
}

void DockSettings::preset(const QString &name)
{
    if (name == "Crystal Clear") {
        surfaceOpacity = .04; thickness = 24; edgeReflection = 1; highlightIntensity = .84; gpuEffects = true;
    } else if (name == "Deep Glass") {
        surfaceOpacity = .32; thickness = 17; edgeReflection = .85; highlightIntensity = .55; gpuEffects = true;
    } else if (name == "Soft Glass") {
        surfaceOpacity = .22; thickness = 8; edgeReflection = .48; highlightIntensity = .3; gpuEffects = false;
    } else return;
    scheduleSave();
}

void DockSettings::setPinnedApps(const QStringList &ids)
{
    pinnedApps = ids;
    pinnedApps.removeDuplicates();
    pinnedApps = pinnedApps.mid(0, 24);
    scheduleSave();
    emit pinsChanged();
}

bool DockSettings::systemDark() const
{
    return QGuiApplication::styleHints()->colorScheme() != Qt::ColorScheme::Light;
}
