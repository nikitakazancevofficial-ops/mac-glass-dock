#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QCoreApplication>
#include <QQuickWindow>
#include "dockplatform.h"
#include "applicationmodel.h"
#include "docksettings.h"
#include "applicationcatalogue.h"
#include "windowtracker.h"
#include "framediagnostics.h"
#include "controlcenterstate.h"
#include <QIcon>
#include <QTimer>
#include <QStandardPaths>
#include <QDir>
#include <QCommandLineParser>
#include <QImage>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusMessage>
#ifdef DOCK_TESTING
#include "docktests.h"
#include "stage4tests.h"
#endif

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Mac Glass Dock"));
    app.setApplicationVersion(QStringLiteral("0.4.0"));
    app.setOrganizationName(QStringLiteral("MacGlassDock"));
    app.setDesktopFileName(QStringLiteral("org.macglass.Dock"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Floating Qt Quick dock for KDE Plasma / Wayland"));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({QStringLiteral("preview"), QStringLiteral("Open a regular preview window")});
    parser.addOption({QStringLiteral("smoke-test"), QStringLiteral("Verify QML creates a root window, then exit")});
    parser.addOption({QStringLiteral("launcher"), QStringLiteral("Open the application launcher")});
    parser.addOption({QStringLiteral("settings"), QStringLiteral("Open the appearance panel")});
    parser.addOption({QStringLiteral("enable-window-tracking"), QStringLiteral("Enable optional KWin window tracking for this run")});
    parser.addOption({QStringLiteral("diagnostics"), QStringLiteral("Report active frame intervals every five seconds")});
#ifdef DOCK_TESTING
    parser.addOption({QStringLiteral("interaction-test"), QStringLiteral("Run Qt Test using the build test fixtures")});
    parser.addOption({QStringLiteral("stage4-test"), QStringLiteral("Run launcher, settings and isolated window tests")});
    parser.addOption({QStringLiteral("control-center-test"), QStringLiteral("Run focused Control Center status-row click test")});
    parser.addOption({QStringLiteral("capture"), QStringLiteral("Save this window's framebuffer (test builds only)"), QStringLiteral("file")});
    QCommandLineOption missingOption(QStringLiteral("test-missing-qml"), QStringLiteral("Negative QML loading test"));
    missingOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(missingOption);
#endif
    parser.process(app);

    // Some QPA backends (notably offscreen) do not supply XDG theme paths.
    QStringList iconPaths = QIcon::themeSearchPaths();
    for (const QString &data : QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation)) {
        const QString icons = data + QStringLiteral("/icons");
        if (QDir(icons).exists())
            iconPaths.append(icons);
    }
    iconPaths.removeDuplicates();
    QIcon::setThemeSearchPaths(iconPaths);
    QIcon::setFallbackThemeName(QStringLiteral("breeze"));
    if (QIcon::themeName().isEmpty())
        QIcon::setThemeName(QStringLiteral("breeze"));
    DockSettings settings;
    const bool forceWindowTracking=parser.isSet(QStringLiteral("enable-window-tracking"));
    ApplicationModel applications(settings.pinnedApps);
    WindowTracker windows;
    bool testing=parser.isSet(QStringLiteral("smoke-test"));
#ifdef DOCK_TESTING
    testing=testing || parser.isSet(QStringLiteral("interaction-test")) || parser.isSet(QStringLiteral("stage4-test")) || parser.isSet(QStringLiteral("control-center-test")) || parser.isSet(QStringLiteral("capture"));
#endif
    applications.attach(&settings, &windows);
    ApplicationCatalogue catalogue(&settings, &applications);
    // Keep the tracker available for fullscreen-aware overlays. The control
    // center must disappear above fullscreen clients even when Dock window
    // integration is not otherwise used by the launcher.
    windows.setEnabled(!testing || settings.windowIntegration || forceWindowTracking);
    QObject::connect(&settings, &DockSettings::changed, &windows, [&] { windows.setEnabled(!testing || settings.windowIntegration || forceWindowTracking); });
    DockPlatform platform;
    ControlCenterState controlCenterState;
    platform.setSettings(&settings);
    platform.setWindowTracker(&windows);
    // Reopening settings must not create a second dock that competes for the backend.
    if(!testing && !parser.isSet(QStringLiteral("preview")) && !platform.ownsBus()
        && QGuiApplication::platformName().startsWith("wayland")
        && QDBusConnection::sessionBus().interface()->isServiceRegistered("org.macglass.Dock").value()) {
        auto call=QDBusMessage::createMethodCall("org.macglass.Dock","/Dock","org.macglass.Dock","openPanel");
        call << (parser.isSet(QStringLiteral("launcher")) ? QString("launcher") : QString("settings"));
        const auto result=QDBusConnection::sessionBus().call(call);
        if(result.type()!=QDBusMessage::ErrorMessage) return EXIT_SUCCESS;
    }
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::warnings, &app, [](const QList<QQmlError> &errors) {
        for (const auto &error : errors) qWarning().noquote() << error.toString();
    });
    engine.setInitialProperties({
        {QStringLiteral("platform"), QVariant::fromValue(&platform)},
        {QStringLiteral("previewMode"), app.arguments().contains(QStringLiteral("--preview"))},
        {QStringLiteral("settings"), QVariant::fromValue(&settings)},
        {QStringLiteral("applications"), QVariant::fromValue(&applications)},
        {QStringLiteral("catalogue"), QVariant::fromValue(&catalogue)},
        {QStringLiteral("windows"), QVariant::fromValue(&windows)}
        ,{QStringLiteral("controlCenterState"), QVariant::fromValue(&controlCenterState)}
    });
    engine.addImageProvider(QStringLiteral("dockicons"), new ThemeIconProvider);
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
                     &app, [] { QCoreApplication::exit(EXIT_FAILURE); },
                     Qt::QueuedConnection);
    QString module = QStringLiteral("MacGlassDock");
#ifdef DOCK_TESTING
    if (app.arguments().contains(QStringLiteral("--test-missing-qml")))
        module = QStringLiteral("IntentionallyMissingModule");
#endif
    engine.loadFromModule(module, QStringLiteral("Main"));

    // A successful process exit is not enough: the root QML object must exist.
    if (engine.rootObjects().isEmpty()) {
        return EXIT_FAILURE;
    }

    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!window)
        return EXIT_FAILURE;
    if (app.arguments().contains(QStringLiteral("--smoke-test")))
        return EXIT_SUCCESS;
    platform.configure(window, app.arguments().contains(QStringLiteral("--preview")));
    QObject::connect(&platform,&DockPlatform::panelRequested,window,[window](const QString &kind){
        QMetaObject::invokeMethod(window, kind=="launcher" ? "openLauncher" : kind=="controlcenter" ? "openControlCenter" : "openSettings");
    });
    window->show();
    if(parser.isSet(QStringLiteral("diagnostics")))
        for(auto *w:qGuiApp->allWindows()) if(auto *quick=qobject_cast<QQuickWindow *>(w)) new FrameDiagnostics(quick, &app);
    if(parser.isSet(QStringLiteral("launcher"))) QTimer::singleShot(100, window, [window] { QMetaObject::invokeMethod(window, "openLauncher"); });
    if(parser.isSet(QStringLiteral("settings"))) QTimer::singleShot(100, window, [window] { QMetaObject::invokeMethod(window, "openSettings"); });

#ifdef DOCK_TESTING
    if(parser.isSet(QStringLiteral("control-center-test"))) {
        QTimer::singleShot(350, &app, [&] {
            app.setQuitOnLastWindowClosed(false);
            app.exit(runControlCenterTests(window,&platform));
        });
    } else if(parser.isSet(QStringLiteral("stage4-test"))) {
        QTimer::singleShot(350, &app, [&] {
            app.setQuitOnLastWindowClosed(false);
            app.exit(runStage4Tests(window, &settings, &applications, &catalogue, &windows));
        });
    } else if (app.arguments().contains(QStringLiteral("--interaction-test"))) {
        QTimer::singleShot(300, &app, [&] {
            app.setQuitOnLastWindowClosed(false);
            app.exit(runDockTests(window, &applications));
        });
    } else if (parser.isSet(QStringLiteral("capture"))) {
        QTimer::singleShot(1000, &app, [&] {
            QQuickWindow *target = window;
            const QString name = parser.isSet(QStringLiteral("launcher")) ? QStringLiteral("launcherWindow") : parser.isSet(QStringLiteral("settings")) ? QStringLiteral("settingsWindow") : QString();
            if(!name.isEmpty()) for(auto *w:qGuiApp->allWindows()) if(w->objectName()==name) target=qobject_cast<QQuickWindow *>(w);
            const bool saved = target && target->grabWindow().save(parser.value(QStringLiteral("capture")));
            window->close();
            app.exit(saved ? EXIT_SUCCESS : EXIT_FAILURE);
        });
    }
#endif

    return app.exec();
}
