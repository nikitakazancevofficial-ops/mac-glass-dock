#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QProcess>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTest>
#include <QTimer>
#include <QElapsedTimer>
#include <QSettings>
#include <QDir>
#include <QScreen>
#include <QScopeGuard>
#include <QFile>
#include <QSaveFile>
#include <LayerShellQt/Window>
class Pattern : public QQuickPaintedItem {
    int phase=0;
    QTimer timer;
public:
    Pattern(QQuickItem *p):QQuickPaintedItem(p){
        setAntialiasing(true);
        timer.setInterval(16);
        QObject::connect(&timer,&QTimer::timeout,this,[this]{++phase;update();});timer.start();
    }
    void pause() {timer.stop();}
    void paint(QPainter *p) override {
        p->fillRect(boundingRect(),QColor("#17354f"));
        for(int y=0;y<height();y+=32)for(int x=0;x<width();x+=32)
            p->fillRect(x,y,32,32,QColor::fromHsv((x/64*23+y/64*31+phase*2)%360,160,90+(x+y)%80));
        p->setPen(QColor("#d8f4ff"));
        for(int x=0;x<width();x+=24)p->drawLine(x,0,x,height());
        for(int y=0;y<height();y+=24)p->drawLine(0,y,width(),y);
        p->setFont(QFont("Sans Serif",18));
        p->drawText(28,44,"Isolated KWin test — owned grid; no desktop capture");
    }
};
class SessionTests : public QObject {
    Q_OBJECT
    QProcess dock;
    bool bootstrapped=false;
public:
    Pattern *pattern=nullptr;
private:
    QString rpc(const QString &method) {
        QDBusReply<QString> reply=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction",method));
        return reply.isValid()?reply.value():QString();
    }
    QJsonObject diagnostics(){return QJsonDocument::fromJson(rpc("diagnostics").toUtf8()).object();}
    QJsonObject clientState(){
        QDBusReply<QString> r=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall("org.macglass.Dock","/Dock","org.macglass.Dock","opticalState"));
        return QJsonDocument::fromJson(r.value().toUtf8()).object();
    }
    QString captureOwnedScene() {
        const auto before=diagnostics()["textures"].toObject()["qaSceneCaptures"].toInteger();
        auto call=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","testCaptureOwnedScene");
        if(QDBusConnection::sessionBus().call(call).type()==QDBusMessage::ErrorMessage)return {};
        QElapsedTimer elapsed;elapsed.start();
        while(elapsed.elapsed()<2000) {
            const auto info=diagnostics()["textures"].toObject();
            if(info["qaSceneCaptures"].toInteger()>before)return info["qaSceneHash"].toString();
            QTest::qWait(10);
        }
        return {};
    }
private slots:
    void bootstrap() {
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_INSTALLED_LOADER")) {
            QProcess manage;
            manage.setProgram(QDir(QStringLiteral(DOCK_KWIN_BUILD)).absoluteFilePath("../scripts/refraction.sh"));
            for(const QString &action:QStringList{"install","enable","enable"}) {
                manage.setArguments({action});manage.start();QVERIFY(manage.waitForFinished(10000));
                qInfo().noquote()<<manage.readAllStandardOutput()<<manage.readAllStandardError();
                QCOMPARE(manage.exitCode(),0);
            }
            QTRY_COMPARE(rpc("status"),QString("ready:v1"));
            bootstrapped=true;return;
        }
        auto call=QDBusMessage::createMethodCall("org.kde.KWin","/Scripting","org.kde.kwin.Scripting","loadDeclarativeScript");
        const QString pluginRoot = qEnvironmentVariable("MACGLASS_TEST_PLUGIN_ROOT",
                                                         QStringLiteral(DOCK_KWIN_BUILD));
        call << QDir(pluginRoot).filePath(QStringLiteral("bootstrap.qml"))
             << QStringLiteral("macglass-optics-test-loader");
        QDBusReply<int> reply=QDBusConnection::sessionBus().call(call);
        QVERIFY2(reply.isValid(),qPrintable(reply.error().message()));
        const QString path="/Scripting/Script"+QString::number(reply.value());
        auto introspect=QDBusMessage::createMethodCall("org.kde.KWin",path,"org.freedesktop.DBus.Introspectable","Introspect");
        qInfo().noquote()<<QDBusReply<QString>(QDBusConnection::sessionBus().call(introspect)).value();
        auto run=QDBusMessage::createMethodCall("org.kde.KWin",path,"org.kde.kwin.Script","run");
        const auto result=QDBusConnection::sessionBus().call(run);
        QVERIFY2(result.type()!=QDBusMessage::ErrorMessage,qPrintable(result.errorMessage()));
        QTRY_COMPARE_WITH_TIMEOUT(rpc("status"),QString("ready:v1"),8000);
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_OUTPUT_SCALE")) {
            QTest::qWait(500);qInfo().noquote()<<"Scale diagnostics:"<<rpc("diagnostics");
            QTRY_VERIFY_WITH_TIMEOUT(qAbs(pattern->window()->devicePixelRatio()-qEnvironmentVariable("MACGLASS_TEST_OUTPUT_SCALE").toDouble())<.01,6000);
        }
        bootstrapped=true;
    }
    void isolatedDecoration() {
        if(!bootstrapped)QSKIP("Bootstrap failed");
        QProcess setup;
        auto env=QProcessEnvironment::systemEnvironment();
        if(!env.contains("XDG_STATE_HOME"))env.insert("XDG_STATE_HOME",QDir(qEnvironmentVariable("XDG_CONFIG_HOME")).filePath("state"));
        setup.setProcessEnvironment(env);
        setup.start("python3",{QDir(QStringLiteral(DOCK_KWIN_BUILD)).absoluteFilePath("../scripts/desktop-session.py"),"decorate"});
        QVERIFY(setup.waitForFinished(10000));
        qInfo().noquote()<<setup.readAllStandardOutput()<<setup.readAllStandardError();
        QCOMPARE(setup.exitCode(),0);
    }
    void realCompositorLenses() {
        if(!bootstrapped) QSKIP("Bootstrap failed");
        const QString config=QDir(qEnvironmentVariable("XDG_CONFIG_HOME")).filePath("mac-glass-dock/settings.ini");
        QDir().mkpath(QFileInfo(config).absolutePath());
        QSettings settings(config,QSettings::IniFormat);
        settings.setValue("Appearance/AutoHide",false);
        settings.setValue("Appearance/WindowIntegration",true);
        settings.setValue("Appearance/WindowAnimations",true);settings.sync();
        settings.setValue("Applications/Pinned",QStringList{"mac-glass-dock-probe.desktop","org.kde.dolphin.desktop","firefox.desktop","org.kde.konsole.desktop","org.kde.kate.desktop","systemsettings.desktop"});settings.sync();
        dock.setProgram(QStringLiteral(DOCK_BINARY_DIR "/mac-glass-dock"));
        dock.setArguments({"--settings"});dock.start();QVERIFY(dock.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["draws"].toInteger()>0,10000);
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["iconDraws"].toInteger()>0,10000);
        QTRY_VERIFY_WITH_TIMEOUT(clientState()["backend"].toBool(),8000);
        QCOMPARE(diagnostics()["pid"].toInteger(),dock.processId());
        qInfo().noquote()<<"LIVE BACKEND:"<<rpc("diagnostics");
        QCOMPARE(diagnostics()["revision"].toString(),QString("desktop-v11-isolated-shaders"));
        QVERIFY(diagnostics()["shaderPrefix"].toString().startsWith(":/macglass/"));
        QVERIFY(diagnostics()["panelGenie"].toBool());
        QVERIFY(diagnostics()["motionShader"].toBool());
        const auto tex=diagnostics()["textures"].toObject()["dockWindow"].toObject();
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_OUTPUT_SCALE"))
            QCOMPARE(tex["scale"].toDouble(),qEnvironmentVariable("MACGLASS_TEST_OUTPUT_SCALE").toDouble());
        if(qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE")) {
            QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["textures"].toObject()["dockWindow"].toObject()["backgroundChanges"].toInteger()>=15,4000);
            const auto changes=[this]{return diagnostics()["textures"].toObject()["dockWindow"].toObject()["backgroundChanges"].toInteger();};
            const auto before=changes();
            QElapsedTimer clock;clock.start();QTest::qWait(1000);
            const auto frames=changes()-before;
            qInfo()<<"Real backdrop changes:"<<frames<<"in"<<clock.elapsed()<<"ms";
            QVERIFY2(frames>=15,"Backdrop is stale or updating slower than 15 Hz in the QA compositor");
            qInfo().noquote()<<"LIVE BACKGROUND:"<<rpc("diagnostics");
        }
        // An actual normal application, not the Dock, must exercise open/close.
        QProcess probe;
        const auto opened=diagnostics()["openings"].toInteger();
        const auto closed=diagnostics()["closings"].toInteger();
        probe.start(QStringLiteral(DOCK_BINARY_DIR "/dock-window-probe"));QVERIFY(probe.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["openings"].toInteger()>opened,5000);
        QTRY_COMPARE_WITH_TIMEOUT(diagnostics()["activeMotions"].toInt(),0,5000);
        QTest::qWait(500);
        probe.terminate();QVERIFY(probe.waitForFinished(3000));
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["closings"].toInteger()>closed,5000);
        QTRY_COMPARE_WITH_TIMEOUT(diagnostics()["activeMotions"].toInt(),0,5000);
        QVERIFY(diagnostics()["motionFrames"].toInteger()>3);
        QVERIFY(diagnostics()["anchoredMotions"].toInteger()>=2);
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_TAIL_HOLD")) {
            QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["tailSuppressed"].toInteger()>0,3000);
            qInfo()<<"Suppressed undeformed tail frames:"<<diagnostics()["tailSuppressed"];
        }
        // Closing while opening must keep the last texture alive, then release it.
        const auto rapidOpen=diagnostics()["openings"].toInteger();
        const auto rapidClose=diagnostics()["closings"].toInteger();
        probe.start();QVERIFY(probe.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["openings"].toInteger()>rapidOpen,4000);
        probe.terminate();QVERIFY(probe.waitForFinished(3000));
        QTRY_VERIFY_WITH_TIMEOUT(diagnostics()["closings"].toInteger()>rapidClose,4000);
        QTRY_COMPARE_WITH_TIMEOUT(diagnostics()["activeMotions"].toInt(),0,4000);
        qInfo().noquote()<<"WINDOW MOTION:"<<rpc("diagnostics");
        // QA deliberately retains a deleted probe for 1600 ms. That transient
        // scene damage is not idle; wait for the fixture's reference to expire.
        pattern->pause();QTest::qWait(1800);
        const auto idleDraws=diagnostics()["draws"].toInteger();QTest::qWait(600);
        const auto extraDraws=diagnostics()["draws"].toInteger()-idleDraws;
        qInfo()<<"Idle additional lens draws:"<<extraDraws;
        QVERIFY2(extraDraws<100,"Effect keeps repainting an idle compositor");
        dock.terminate();QVERIFY(dock.waitForFinished(3000));
        QTRY_COMPARE_WITH_TIMEOUT(diagnostics()["pid"].toInteger(),qint64(0),4000);
    }
    void ordinaryClientPanelLifecycle() {
        if(!bootstrapped)QSKIP("Bootstrap failed");
        const QString config=QDir(qEnvironmentVariable("XDG_CONFIG_HOME")).filePath("mac-glass-dock/settings.ini");
        QFile original(config);QVERIFY(original.open(QIODevice::ReadOnly));const auto saved=original.readAll();original.close();
        QProcess underlay;
        const auto cleanup=qScopeGuard([&]{
            if(underlay.state()!=QProcess::NotRunning){underlay.terminate();if(!underlay.waitForFinished(2000)){underlay.kill();underlay.waitForFinished();}}
            if(dock.state()!=QProcess::NotRunning){dock.terminate();if(!dock.waitForFinished(2500)){dock.kill();dock.waitForFinished();}}
            QSaveFile restore(config);if(restore.open(QIODevice::WriteOnly)){restore.write(saved);restore.commit();}
        });
        {
            QSettings settings(config,QSettings::IniFormat);
            settings.setValue("Appearance/CenterZoom",.94);
            settings.setValue("Appearance/AnimationSpeed",2.55);
            settings.setValue("Appearance/SurfaceOpacity",.19);
            settings.setValue("Appearance/Theme","light");
            settings.setValue("Appearance/RefractionStrength",34.9);
            settings.setValue("Appearance/RefractionWidth",22);
            settings.setValue("Appearance/BlurRadius",2.5);
            settings.setValue("Appearance/ReducedMotion",false);
            settings.setValue("Appearance/AutoHide",false);settings.sync();
        }
        dock.setProcessEnvironment(QProcessEnvironment::systemEnvironment());
        dock.setArguments({});dock.start();QVERIFY(dock.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(clientState()["backend"].toBool(),8000);
        QVERIFY(!clientState()["syntheticInput"].toBool()); // No --stage4-test, --capture or QtTest client.
        const bool qa=qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE");
        if(qa) {
            auto env=QProcessEnvironment::systemEnvironment();env.remove("MACGLASS_TEST_WINDOW_THUMBNAILS");
            underlay.setProcessEnvironment(env);underlay.start(QStringLiteral(DOCK_BINARY_DIR "/dock-window-probe"));
            QVERIFY(underlay.waitForStarted());QTest::qWait(800);
            auto place=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","placeProbe");
            place<<false;QCOMPARE(QDBusConnection::sessionBus().call(place).type(),QDBusMessage::ReplyMessage);
        }
        QTest::qWait(800); // Static backdrop; no animated wallpaper helps damage.
        const auto baseline=qa?captureOwnedScene():QString();
        if(qa)QVERIFY(!baseline.isEmpty());
        const auto tailBefore=diagnostics()["panelTailSuppressed"].toInteger();
        const auto launcher=[&]{return clientState()["windows"].toObject()["launcherWindow"].toObject();};
        const auto toggle=[] {
            auto call=QDBusMessage::createMethodCall("org.macglass.Dock","/Dock","org.macglass.Dock","openPanel");
            call<<QString("launcher");return QDBusConnection::sessionBus().call(call).type();
        };
        for(int cycle=0;cycle<3;++cycle) {
            QCOMPARE(toggle(),QDBusMessage::ReplyMessage);
            QTRY_VERIFY_WITH_TIMEOUT(!launcher().isEmpty() && launcher()["presented"].toBool() && !launcher().contains("genie"),3000);
            const auto repaints=diagnostics()["panelBackgroundRepaints"].toInteger();
            QElapsedTimer closing;closing.start();
            QCOMPARE(toggle(),QDBusMessage::ReplyMessage);
            QSet<int> progress;
            for(int frame=0;frame<14;++frame) {
                const auto moving=launcher()["genie"].toObject();
                if(!moving.isEmpty())progress.insert(qRound(moving["openness"].toDouble()*1000));
                QTest::qWait(25);
            }
            QVERIFY2(progress.size()>=6,"Ordinary client stopped publishing interior Genie progress");
            QVERIFY(diagnostics()["panelBackgroundRepaints"].toInteger()-repaints>=6);
            QTRY_VERIFY_WITH_TIMEOUT(launcher().isEmpty(),2000);
            QVERIFY2(closing.elapsed()<1100,"Transparent-frame callback stalled until the safety timeout");
            if(qa) {
                // Read a genuine compositor frame while a QA-only second
                // effect deliberately retains the closed surface for 600 ms.
                QCOMPARE(captureOwnedScene(),baseline);
            }
            QTest::qWait(800);
        }
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_PANEL_TAIL_HOLD"))
            QVERIFY2(diagnostics()["panelTailSuppressed"].toInteger()>tailBefore,"Retained popup tail was not exercised");
        qInfo().noquote()<<"ORDINARY CLIENT:"<<rpc("diagnostics");
    }
    void nativeRegression() {
        if(!bootstrapped) QSKIP("Bootstrap failed");
        // A normal fullscreen probe affects overlap detection. An optional
        // isolated background-layer pattern does not and permits visual QA of
        // panel refraction against real, non-black compositor pixels.
        if(!qEnvironmentVariableIsSet("MACGLASS_TEST_PATTERN_BACKGROUND"))pattern->window()->hide();
        auto env=QProcessEnvironment::systemEnvironment();
        env.insert("XDG_CONFIG_HOME",QStringLiteral(DOCK_BINARY_DIR "/test-config"));
        env.insert("XDG_DATA_HOME",QStringLiteral(DOCK_BINARY_DIR "/test-data"));
        env.insert("XDG_CACHE_HOME",QStringLiteral(DOCK_BINARY_DIR "/test-cache"));
        dock.setProcessEnvironment(env);dock.setArguments({"--stage4-test","--capture",QStringLiteral(DOCK_BINARY_DIR "/optics-nested.png")});
        const auto out=connect(&dock,&QProcess::readyReadStandardOutput,this,[this]{qInfo().noquote()<<dock.readAllStandardOutput();});
        const auto err=connect(&dock,&QProcess::readyReadStandardError,this,[this]{qInfo().noquote()<<dock.readAllStandardError();});
        const auto logging=qScopeGuard([&]{disconnect(out);disconnect(err);});
        dock.start();QVERIFY(dock.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(dock.state()==QProcess::NotRunning,90000);
        qInfo().noquote()<<dock.readAllStandardOutput()<<dock.readAllStandardError();
        QCOMPARE(dock.exitStatus(),QProcess::NormalExit);QCOMPARE(dock.exitCode(),0);
        QTRY_COMPARE(diagnostics()["pid"].toInteger(),qint64(0));
        QTRY_COMPARE(diagnostics()["windows"].toInt(),0);
        qInfo().noquote()<<"NATIVE TRANSITIONS:"<<rpc("diagnostics");
        QVERIFY(diagnostics()["minimizes"].toInteger()>=1);
        QVERIFY(diagnostics()["restores"].toInteger()>=1);
        QVERIFY(diagnostics()["minimizeStartFrames"].toInteger()>0);
        QCOMPARE(diagnostics()["minimizeStartFades"].toInteger(),qint64(0));
        QVERIFY(diagnostics()["panelMotionFrames"].toInteger()>0);
        QVERIFY(diagnostics()["warpedLenses"].toInteger()>0);
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_MINIMIZE_START")) {
            QVERIFY(diagnostics()["squashLoaded"].toBool());
            QVERIFY2(diagnostics()["minimizeConflicts"].toInteger()>0,"The competing Squash fade wasn't exercised");
        }
    }
    void unload() {
        if(!bootstrapped) QSKIP("Bootstrap failed");
        dock.setArguments({"--settings"});dock.start();QVERIFY(dock.waitForStarted());
        QTRY_VERIFY_WITH_TIMEOUT(clientState()["backend"].toBool(),8000);
        auto call=QDBusMessage::createMethodCall("org.kde.KWin","/Effects","org.kde.kwin.Effects","unloadEffect");
        call<<QString("macglassrefraction");QDBusConnection::sessionBus().call(call);
        QTRY_VERIFY(rpc("status").isEmpty());
        QTRY_VERIFY_WITH_TIMEOUT(!clientState()["backend"].toBool(),6000);
        dock.terminate();QVERIFY(dock.waitForFinished(3000));
        qInfo()<<"Session-only unload succeeded";
    }
    void cleanupTestCase() {
        if(dock.state()!=QProcess::NotRunning){dock.terminate();if(!dock.waitForFinished(2000)){dock.kill();dock.waitForFinished();}}
    }
};
int main(int argc,char **argv) {
    const bool backdrop=qEnvironmentVariableIsSet("MACGLASS_TEST_PATTERN_BACKGROUND");
    QGuiApplication app(argc,argv);app.setQuitOnLastWindowClosed(false);
    QQuickWindow window;window.setTitle("MacGlass isolated pattern");window.setColor(Qt::transparent);
    if(backdrop) {
        auto *layer=LayerShellQt::Window::get(&window);
        layer->setScope("desktop"); // KWin classifies this owned layer as a desktop, not an application.
        layer->setLayer(LayerShellQt::Window::LayerBackground);
        layer->setExclusiveZone(-1);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop)|LayerShellQt::Window::AnchorBottom|LayerShellQt::Window::AnchorLeft|LayerShellQt::Window::AnchorRight);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    }
    auto *pattern=new Pattern(window.contentItem());
    QObject::connect(&window,&QQuickWindow::widthChanged,pattern,[&]{pattern->setWidth(window.width());});
    QObject::connect(&window,&QQuickWindow::heightChanged,pattern,[&]{pattern->setHeight(window.height());});
    window.resize(1280,900);
    if(backdrop)window.show();else window.showFullScreen();
    QTimer::singleShot(500,&app,[&]{SessionTests test;test.pattern=pattern;app.exit(QTest::qExec(&test,QStringList{"nested-optics-tests"}));});
    return app.exec();
}
#include "opticssession.moc"
