#include "docksettings.h"
#include <QDir>
#include <QSettings>
#include <QTemporaryDir>
#include <QTest>

class SettingsTests : public QObject
{
    Q_OBJECT
private slots:
    void defaultsAndValidation()
    {
        QTemporaryDir directory(QStringLiteral(DOCK_TEST_DIR "/settings-XXXXXX"));
        QVERIFY(directory.isValid());
        const QByteArray oldConfig = qgetenv("XDG_CONFIG_HOME");
        qputenv("XDG_CONFIG_HOME", directory.path().toUtf8());
        DockSettings defaults;
        QCOMPARE(defaults.iconSize, 56);
        QCOMPARE(defaults.maxScale, 1.5);
        QCOMPARE(defaults.animationSpeed, 2.55);
        QCOMPARE(defaults.surfaceOpacity, .04);
        QCOMPARE(defaults.thickness, 24);
        QCOMPARE(defaults.edgeReflection, 1);
        QCOMPARE(defaults.highlightIntensity, .84);
        QCOMPARE(defaults.springDamping, .46);
        QVERIFY(defaults.autoHide);
        QCOMPARE(defaults.iconStyle, QStringLiteral("glass"));
        QCOMPARE(defaults.pinnedApps.size(), 5);
        QVERIFY(!QFileInfo::exists(defaults.fileName));

        QDir().mkpath(directory.path() + QStringLiteral("/mac-glass-dock"));
        QSettings file(defaults.fileName, QSettings::IniFormat);
        file.setValue(QStringLiteral("Appearance/IconSize"), -200);
        file.setValue(QStringLiteral("Appearance/MaxScale"), 999);
        file.setValue(QStringLiteral("Appearance/SurfaceOpacity"), QStringLiteral("nan"));
        file.setValue(QStringLiteral("Appearance/Theme"), QStringLiteral("invalid"));
        file.setValue(QStringLiteral("Appearance/GpuEffects"), false);
        QStringList pins;
        for (int i = 0; i < 40; ++i)
            pins << QStringLiteral("app%1.desktop").arg(i);
        file.setValue(QStringLiteral("Applications/Pinned"), pins);
        file.sync();
        DockSettings validated;
        QCOMPARE(validated.iconSize, 24);
        QCOMPARE(validated.maxScale, 2);
        QCOMPARE(validated.surfaceOpacity, 0.04);
        QCOMPARE(validated.theme, QStringLiteral("auto"));
        QVERIFY(!validated.gpuEffects);
        QCOMPARE(validated.pinnedApps.size(), 24);
        validated.setValue("BlurRadius",999);
        QCOMPARE(validated.blurRadius,12);
        validated.setValue("RefractionStrength",-1);
        QCOMPARE(validated.refractionStrength,0);
        validated.setValue("IconRadius",.35);
        validated.setValue("IconRefractionStrength",18);
        validated.flush();
        DockSettings roundTrip;
        QCOMPARE(roundTrip.iconRadius,.35);
        QCOMPARE(roundTrip.iconRefractionStrength,18);
        const auto oldPins=roundTrip.pinnedApps;
        roundTrip.screenshotDefaults();
        QCOMPARE(roundTrip.surfaceOpacity,.04);
        QCOMPARE(roundTrip.maxScale,1.5);
        QCOMPARE(roundTrip.iconStyle,QStringLiteral("glass"));
        QCOMPARE(roundTrip.pinnedApps,oldPins);
        qputenv("XDG_CONFIG_HOME", oldConfig);
    }
};

QTEST_MAIN(SettingsTests)
#include "settingstests.moc"
