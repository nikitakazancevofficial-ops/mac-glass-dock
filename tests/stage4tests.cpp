#include "stage4tests.h"
#include "applicationmodel.h"
#include "applicationcatalogue.h"
#include "docksettings.h"
#include "windowtracker.h"
#include "dockplatform.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSet>
#include <QFile>
#include <QSaveFile>
#include <QQuickItem>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QProcess>
#include <QScopeGuard>
#include <QTest>
#include <QPainter>
#include <QLinearGradient>
#include <QSettings>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusReply>
#include <QEnterEvent>

static QQuickItem *item(QQuickItem *root,const QString &name) {
    if(root->objectName()==name) return root;
    for(auto *child:root->childItems()) if(auto *found=item(child,name)) return found;
    return nullptr;
}
static QQuickWindow *panel(const QString &name) {
    for(auto *w:qGuiApp->allWindows()) if(w->objectName()==name) return qobject_cast<QQuickWindow *>(w);
    return nullptr;
}
static void scrollTo(QQuickItem *control) {
    for(auto *ancestor=control->parentItem();ancestor;ancestor=ancestor->parentItem()) {
        if(!ancestor->property("contentY").isValid())continue;
        const qreal y=control->mapToItem(ancestor,QPointF(0,control->height()/2)).y();
        const qreal target=ancestor->property("contentY").toReal()+y-ancestor->height()/2;
        ancestor->setProperty("contentY",qBound(0.,target,qMax(0.,ancestor->property("contentHeight").toReal()-ancestor->height())));
        break;
    }
}
class Stage4Tests : public QObject {
    Q_OBJECT
public:
    QQuickWindow *root;
    DockSettings *settings;
    ApplicationModel *apps;
    ApplicationCatalogue *catalogue;
    WindowTracker *windows;
    QByteArray originalConfig;
    QString capturePath;
    void capture(QQuickWindow *w,const QString &suffix) {
        if(capturePath.isEmpty()) return;
        const QString path=QFileInfo(capturePath).absolutePath()+"/"+QFileInfo(capturePath).completeBaseName()+suffix+".png";
        const QImage frame=w->grabWindow();
        QVERIFY(frame.save(path));
        // A QA composite only: these colors are never used as a runtime desktop background.
        QImage contrast(frame.width()*2,frame.height(),QImage::Format_ARGB32_Premultiplied);
        contrast.fill(Qt::transparent);
        QPainter painter(&contrast);
        for(int side=0;side<2;++side) {
            QLinearGradient background(side*frame.width(),0,(side+1)*frame.width(),frame.height());
            background.setColorAt(0,side ? QColor("#13243a") : QColor("#e5edf2"));
            background.setColorAt(1,side ? QColor("#335874") : QColor("#b5cbd9"));
            painter.fillRect(QRect(side*frame.width(),0,frame.width(),frame.height()),background);
            painter.drawImage(side*frame.width(),0,frame);
        }
        painter.end();
        QVERIFY(contrast.save(path.left(path.size()-4)+"-contrast.png"));
    }
    void verifyMaterial(QQuickWindow *w) {
        QTRY_COMPARE(w->property("revealProgress").toDouble(),1.);
        auto *material=item(w->contentItem(),"panelMaterial");
        QVERIFY(material);
        auto *shader=item(material,"glassShader");
        QVERIFY(shader);
        QImage frame;
        // Fractional-scale configure/render commits are asynchronous. Wait for
        // the actual material pixel instead of accepting an initial empty FBO.
        const auto translucent=[&] {
            frame=w->grabWindow();
            if(frame.isNull())return false;
            const qreal pixelScale=qreal(frame.width())/w->width();
            const QPointF p=material->mapToScene(QPointF(material->width()/2,material->height()-12))*pixelScale;
            const auto alpha=frame.pixelColor(qBound(0,qRound(p.x()),frame.width()-1),qBound(0,qRound(p.y()),frame.height()-1)).alpha();
            return alpha>0 && alpha<255;
        };
        QTRY_VERIFY_WITH_TIMEOUT(translucent(),3000);
        QVERIFY(shader->property("status").toInt()!=2);
        if(w->rendererInterface()->graphicsApi()!=QSGRendererInterface::Software) {
            QVERIFY(material->property("useGpu").toBool());
            // Qt's per-instance status may remain Uncompiled on a shared shader-info cache hit.
            // Verify actual output changes with the fallback, not merely a status enum.
            settings->setValue("GpuEffects",false);
            QTest::qWait(50);
            const QImage fallback=w->grabWindow();
            settings->setValue("GpuEffects",true);
            QTest::qWait(50);
            QVERIFY(frame!=fallback);
        }
    }
    void shaders(QQuickItem *rootItem) {
        if(rootItem->objectName()=="glassShader" || rootItem->objectName()=="adaptiveIconShader") {
            QVERIFY(rootItem->property("status").toInt()!=2);
            // Clipped/virtualized delegates legitimately remain Uncompiled until drawn.
        }
        for(auto *child:rootItem->childItems()) shaders(child);
    }
private slots:
    void initTestCase() {
        QCOMPARE(settings->fileName,QStringLiteral(DOCK_TEST_CONFIG));
        QFile f(settings->fileName);
        QVERIFY(f.open(QIODevice::ReadOnly));
        originalConfig=f.readAll();
        const auto args=QCoreApplication::arguments();
        const int at=args.indexOf("--capture");
        if(at>=0 && at+1<args.size()) capturePath=args[at+1];
        QVERIFY(catalogue->count()>0);
    }
    void catalogueAndSearch() {
        catalogue->setQuery("Dock Launch Probe");
        QCOMPARE(catalogue->count(),1);
        QCOMPARE(catalogue->data(catalogue->index(0),ApplicationCatalogue::DesktopId).toString(),QStringLiteral("mac-glass-dock-probe.desktop"));
        catalogue->setQuery("NO_MATCH_4f15d8");
        QCOMPARE(catalogue->count(),0);
        catalogue->setQuery("");
        const int all=catalogue->count();
        catalogue->setCategory("Система");
        QVERIFY(catalogue->count()<=all);
        catalogue->setCategory("Все");
        QCOMPARE(catalogue->count(),all);
    }
    void launcherAndTyping() {
        const double previousZoom=settings->centerZoom;
        settings->setValue("CenterZoom",.94); // Regression: source-space zoom dragged the backdrop.
        const auto zoomCleanup=qScopeGuard([&]{settings->setValue("CenterZoom",previousZoom);});
        QProcess underlay;
        const auto underlayCleanup=qScopeGuard([&]{
            if(underlay.state()!=QProcess::NotRunning) {
                underlay.terminate();
                if(!underlay.waitForFinished(2000)){underlay.kill();underlay.waitForFinished();}
            }
        });
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_WINDOW_THUMBNAILS") && QGuiApplication::platformName().startsWith("wayland")) {
            // A real, updating small application behind the upper part of the
            // catalogue, not only a flat wallpaper pattern.
            underlay.start(QStringLiteral(DOCK_WINDOW_PROBE));QVERIFY(underlay.waitForStarted());
            QTest::qWait(300);
            auto place=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","placeProbe");
            place<<false;QCOMPARE(QDBusConnection::sessionBus().call(place).type(),QDBusMessage::ReplyMessage);
        }
        auto *button=item(root->contentItem(),"launcherButton");
        QVERIFY(button);
        QTest::mouseClick(root,Qt::LeftButton,Qt::NoModifier,button->mapToScene(QPointF(button->width()/2,button->height()/2)).toPoint());
        auto *w=panel("launcherWindow");
        QVERIFY(w); QTRY_VERIFY(w->isVisible());
        auto *search=item(w->contentItem(),"launcherSearch"); QVERIFY(search);
        search->forceActiveFocus();
        // The first character must not be discarded by the opening animation.
        QTest::keyClick(w,Qt::Key_D);
        QTRY_COMPARE(catalogue->query(),QStringLiteral("d"));
        QTest::qWait(300);
        verifyMaterial(w);
        for(auto key:{Qt::Key_O,Qt::Key_C,Qt::Key_K}) QTest::keyClick(w,key);
        QTRY_COMPARE(catalogue->query(),QStringLiteral("dock"));
        QVERIFY(catalogue->count()>0);
        auto *caret=item(w->contentItem(),"launcherSearchCaret");QVERIFY(caret);
        QVERIFY(caret->isVisible());QCOMPARE(caret->width(),2.);
        QVERIFY(caret->height()>0);
        const qreal blink=caret->opacity();
        QTRY_VERIFY_WITH_TIMEOUT(caret->opacity()!=blink,1100);
        QTest::keyClick(w,Qt::Key_Backspace);QCOMPARE(caret->opacity(),1.);
        QTest::keyClick(w,Qt::Key_K);
        QQuickItem *result=nullptr;
        QTRY_VERIFY((result=item(w->contentItem(),"launcherResult0")));
        QVERIFY(result->property("arrival").isValid());
        QTRY_COMPARE(result->property("arrival").toDouble(),1.);
        shaders(w->contentItem());
        catalogue->setQuery("");
        QTest::qWait(160);
        capture(w,"-launcher");
        QTest::keyClick(w,Qt::Key_Escape);
        QTRY_VERIFY(!w->isVisible());
    }
    void launcherReturnAndOutsidePress() {
        auto *platform=root->property("platform").value<DockPlatform *>();QVERIFY(platform);
        if(QGuiApplication::platformName().startsWith("wayland"))QTRY_VERIFY(platform->nativePanelGenie());
        QSignalSpy prepared(platform,&DockPlatform::panelMotionPrepared);
        QVERIFY(QMetaObject::invokeMethod(root,"openLauncher"));
        auto *w=panel("launcherWindow");QVERIFY(w);
        QTRY_COMPARE(w->property("revealProgress").toDouble(),1.);
        if(platform->nativePanelGenie()) {
            QVERIFY(prepared.count()>0);
            QVERIFY(w->property("compositorMotion").toBool());
            QVERIFY(w->property("nativePrepared").toBool());
            QVERIFY(!w->property("waitingForNative").toBool());
        }
        auto *material=item(w->contentItem(),"panelMaterial");QVERIFY(material);
        QVERIFY(material->property("tintOpacity").toDouble()>=.58);
        const QRectF expanded=w->property("interactiveRect").toRectF();
        QVERIFY(expanded.width()<w->width());
        QVERIFY(!w->mask().contains(QPoint(1,1)) || QGuiApplication::platformName()=="offscreen");
        const QPointF anchor=w->property("animationAnchor").toPointF();
        QVERIFY(QMetaObject::invokeMethod(w,"dismiss"));
        QTest::qWait(70);
        const double progress=w->property("revealProgress").toDouble();
        QVERIFY(progress>0 && progress<1);
        const auto shrinking=w->property("interactiveRect").toRectF();
        QVERIFY(!w->property("genieFailed").toBool());
        if(w->rendererInterface()->graphicsApi()!=QSGRendererInterface::Software)
            QVERIFY(w->property("genieActive").toBool());
        if(w->property("genieActive").toBool()) {
            QVERIFY(shrinking!=expanded); // Neck stretches before the sheet narrows.
            QVERIFY(!w->property("genieFailed").toBool());
            QTRY_VERIFY(w->property("revealProgress").toDouble()<=.65);
            if(platform->nativePanelGenie()) {
                QVERIFY(w->property("nativeMotionActive").toBool());
                const auto moving=[&]{return QJsonDocument::fromJson(platform->opticalState().toUtf8()).object()["windows"].toObject()["launcherWindow"].toObject();};
                QTRY_VERIFY(moving().contains("genie"));
                QVERIFY(!moving()["lenses"].toArray().isEmpty());
            } else capture(w,"-launcher-genie");
        } else {
            QVERIFY(shrinking.width()<expanded.width());
            QVERIFY(QLineF(shrinking.center(),anchor).length()<QLineF(expanded.center(),anchor).length());
        }
        // Reverse while still in flight; no reset, flash or late hide callback.
        QVERIFY(QMetaObject::invokeMethod(w,"open"));
        QTRY_COMPARE(w->property("revealProgress").toDouble(),1.);
        QVERIFY(w->isVisible());
        platform->handlePanelOutside("unrelatedPanel");
        QVERIFY(w->property("presented").toBool());
        if(QGuiApplication::platformName().startsWith("wayland")) {
            QTRY_VERIFY(QJsonDocument::fromJson(platform->opticalState().toUtf8()).object()["windows"].toObject().contains("launcherWindow"));
            const auto press=[&](bool inside) {
                auto call=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","testPanelPress");
                call<<QString("launcherWindow")<<inside;
                return QDBusConnection::sessionBus().call(call);
            };
            const auto reply=press(true);
            if(reply.type()==QDBusMessage::ErrorMessage) {
                QTest::keyClick(w,Qt::Key_Escape);
                QTRY_VERIFY(!w->isVisible());
                QSKIP("Outside-press injection exists only in the private QA compositor");
            }
            QTest::qWait(100);QVERIFY(w->property("presented").toBool());
            QCOMPARE(press(false).type(),QDBusMessage::ReplyMessage);
        } else platform->handlePanelOutside("launcherWindow");
        QTRY_VERIFY(!w->property("presented").toBool());
        QTRY_VERIFY(!w->isVisible());
    }
    void pinning() {
        const QString id="mac-glass-dock-probe.desktop";
        const int before=apps->count();
        QVERIFY(settings->pinnedApps.contains(id));
        catalogue->togglePin(id);
        QTRY_COMPARE(apps->count(),before-1);
        QVERIFY(!settings->pinnedApps.contains(id));
        catalogue->togglePin(id);
        QTRY_COMPARE(apps->count(),before);
        QVERIFY(settings->pinnedApps.contains(id));
        int row=-1;
        for(int i=0;i<apps->count();++i)if(apps->desktopId(i)==id)row=i;
        QVERIFY(row>=0);
        auto *icon=item(root->contentItem(),"dockItem"+QString::number(row));QVERIFY(icon);
        QTest::qWait(100); // Let Row polish the newly reinserted delegate.
        QTest::mouseClick(root,Qt::RightButton,Qt::NoModifier,icon->mapToScene(QPointF(icon->width()/2,icon->height()/2)).toPoint());
        auto *menu=panel("appMenuWindow");QVERIFY(menu);QTRY_VERIFY(menu->isVisible());
        QCOMPARE(menu->property("desktopId").toString(),id);
        QTRY_COMPARE(menu->property("revealProgress").toDouble(),1.);
        auto *pin=item(menu->contentItem(),"pinApplicationButton");QVERIFY(pin);
        QTest::mouseClick(menu,Qt::LeftButton,Qt::NoModifier,pin->mapToScene(QPointF(pin->width()/2,pin->height()/2)).toPoint());
        QTRY_VERIFY(!settings->pinnedApps.contains(id));
        catalogue->togglePin(id);
        QTRY_VERIFY(settings->pinnedApps.contains(id));
        settings->flush();
        QSettings saved(settings->fileName,QSettings::IniFormat);
        QVERIFY(saved.value("Applications/Pinned").toStringList().contains(id));
    }
    void settingsAndPresets() {
        QVERIFY(QMetaObject::invokeMethod(root,"openSettings"));
        auto *w=panel("settingsWindow"); QVERIFY(w); QTRY_VERIFY(w->isVisible());
        QTest::qWait(300);
        verifyMaterial(w);
        auto *control=item(w->contentItem(),"Thickness"); QVERIFY(control);
        scrollTo(control);QTest::qWait(80);
        auto *slider=item(control,"settingSlider"); QVERIFY(slider);
        const qreal old=settings->thickness;
        QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,slider->mapToScene(QPointF(slider->width()*.82,slider->height()/2)).toPoint());
        QTRY_VERIFY(settings->thickness!=old);
        auto *blur=item(w->contentItem(),"BlurRadius");QVERIFY(blur);
        if(blur->isEnabled()) {
            scrollTo(blur);QTest::qWait(80);
            auto *blurSlider=item(blur,"settingSlider");QVERIFY(blurSlider);
            const qreal oldBlur=settings->blurRadius;
            QTest::mouseClick(w,Qt::LeftButton,Qt::NoModifier,blurSlider->mapToScene(QPointF(blurSlider->width()*.72,blurSlider->height()/2)).toPoint());
            QTRY_VERIFY(settings->blurRadius!=oldBlur);
            auto *platform=root->property("platform").value<DockPlatform *>();QVERIFY(platform);
            QTRY_COMPARE(QJsonDocument::fromJson(platform->opticalState().toUtf8()).object()["options"].toObject()["blurRadius"].toDouble(),settings->blurRadius);
            settings->setValue("BlurRadius",oldBlur);
        }
        settings->preset("Crystal Clear"); QCOMPARE(settings->surfaceOpacity,.04);
        settings->preset("Deep Glass"); QCOMPARE(settings->thickness,17);
        settings->preset("Soft Glass"); QVERIFY(!settings->gpuEffects);
        settings->preset("Crystal Clear"); QVERIFY(settings->gpuEffects);
        shaders(w->contentItem());
        QTest::qWait(180); capture(w,"-settings");
        settings->setValue("Theme","light");
        QTest::qWait(80); capture(w,"-light");
        settings->setValue("Theme","dark");
        settings->setValue("BlurEnabled",false); QVERIFY(!settings->blurEnabled);
        QTest::qWait(80); capture(w,"-no-blur");
        settings->setValue("BlurEnabled",true);
        QVERIFY(QMetaObject::invokeMethod(w,"dismiss")); QTRY_VERIFY(!w->isVisible());
    }
    void iconModesAndReducedMotion() {
        for(const auto &mode:QStringList{"original","adaptive","glass"}) {
            settings->setValue("IconStyle",mode); QCOMPARE(settings->iconStyle,mode);
            QTest::qWait(120); shaders(root->contentItem()); capture(root,"-"+mode);
        }
        settings->setValue("IconStyle","adaptive");
        settings->setValue("ReducedMotion",true);
        auto *dock=item(root->contentItem(),"dock"); QVERIFY(dock);
        auto *icon=item(dock,"dockItem1"); QVERIFY(icon);
        QTest::mouseMove(root,icon->mapToScene(QPointF(icon->width()/2,icon->height()/2)).toPoint());
        QTRY_COMPARE(icon->property("targetScale").toReal(),1.0);
        settings->setValue("ReducedMotion",false);
    }
    void liveOpticalGeometry() {
        auto *platform=root->property("platform").value<DockPlatform *>();
        QVERIFY(platform);
        if(!platform->ownsBus()) QSKIP("Another Dock owns the live geometry service");
        auto *icon=item(root->contentItem(),"dockItem1");QVERIFY(icon);
        QSet<QString> samples;
        for(int step=0;step<12;++step) {
            const QPointF p=icon->mapToScene(QPointF(icon->width()*(step%2?.7:.3),icon->height()*.5));
            QTest::mouseMove(root,p.toPoint());QTest::qWait(20);
            samples.insert(platform->opticalState());
        }
        QVERIFY2(samples.size()>=4,"Geometry publication stalled during magnification");
        for(const auto &json:samples) {
            const auto windows=QJsonDocument::fromJson(json.toUtf8()).object()["windows"].toObject();
            QVERIFY(windows.contains("dockWindow"));
        }
    }
    void autoHideAndReveal() {
        if(root->property("previewMode").toBool()) QSKIP("Preview deliberately disables auto-hide");
        QEvent leave(QEvent::Leave); QCoreApplication::sendEvent(root,&leave);
        QTest::mouseMove(root,QPoint(-20,-20));
        QCoreApplication::sendEvent(root,&leave);
        settings->setValue("AutoHide",true);
        QTRY_VERIFY_WITH_TIMEOUT(root->property("compact").toBool(),12000);
        QCOMPARE(root->height(),2);
        QTest::mouseMove(root,QPoint(root->width()/2,1));
        QTRY_VERIFY(!root->property("compact").toBool());
        settings->setValue("AutoHide",false);
        QTRY_VERIFY(root->height()>2);
    }
    void actualWaylandWindows() {
        QVERIFY(!windows->perform(-1,"close"));
        if(!QGuiApplication::platformName().startsWith("wayland")) QSKIP("Needs a real Wayland session");
        settings->setValue("WindowIntegration",true);
        QTRY_VERIFY_WITH_TIMEOUT(windows->available(),4000);
        QSignalSpy resets(apps,&QAbstractItemModel::modelReset);
        auto *stableIcon=root->findChild<QQuickItem *>("launcherButton");
        QVERIFY(stableIcon);
        QProcess probe;
        probe.start(QStringLiteral(DOCK_WINDOW_PROBE));
        QVERIFY(probe.waitForStarted());
        const auto cleanup=qScopeGuard([&]{
            if(probe.state()!=QProcess::NotRunning) {probe.terminate();if(!probe.waitForFinished(2000)){probe.kill();probe.waitForFinished();}}
            settings->setValue("WindowIntegration",false);
        });
        const QString id="mac-glass-dock-probe.desktop";
        QTRY_VERIFY_WITH_TIMEOUT(!windows->windowsFor(id).isEmpty(),6000);
        const auto state=windows->windowsFor(id).first().toMap();
        const int token=state.value("token").toInt();
        QVERIFY(windows->perform(token,"activate"));
        QTRY_VERIFY(windows->windowsFor(id).value(0).toMap().value("active").toBool());
        auto *platform=root->property("platform").value<DockPlatform *>();QVERIFY(platform);
        const auto place=[](bool overlap){
            auto call=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","placeProbe");
            call<<overlap;return QDBusConnection::sessionBus().call(call);
        };
        if(place(false).type()!=QDBusMessage::ErrorMessage) {
            QTRY_VERIFY(!platform->obstructed());
            settings->setValue("AutoHide",true);settings->setValue("SmartAutoHide",true);
            QTest::mouseMove(root,QPoint(-20,-20));
            QTest::qWait(1100);QVERIFY(!root->property("compact").toBool());
            place(true);
            QTRY_VERIFY(platform->obstructed());
            QTRY_VERIFY_WITH_TIMEOUT(root->property("compact").toBool(),5000);
            place(false);
            QTRY_VERIFY(!platform->obstructed());
            QTRY_VERIFY(!root->property("compact").toBool());
            settings->setValue("AutoHide",false);settings->setValue("SmartAutoHide",false);
        }
        int hoverRow=-1;
        for(int i=0;i<apps->count();++i)if(apps->desktopId(i)==id)hoverRow=i;
        QVERIFY(hoverRow>=0);
        // Test dwell independently of magnification (covered by the dedicated
        // layout/optics tests). QtTest's window-local fake cursor otherwise
        // drifts when layer-shell recentres a fractionally scaled spring row.
        const bool motionPreference=settings->reducedMotion;
        const auto motionCleanup=qScopeGuard([&]{settings->setValue("ReducedMotion",motionPreference);});
        settings->setValue("ReducedMotion",true);
        auto *hoverIcon=item(root->contentItem(),"dockItem"+QString::number(hoverRow));QVERIFY(hoverIcon);
        QTRY_COMPARE(hoverIcon->property("zoom").toDouble(),1.);
        const auto *delay=hoverIcon->findChild<QObject *>("windowPreviewDelay");QVERIFY(delay);
        QCOMPARE(delay->property("interval").toInt(),2000);
        QTRY_VERIFY(root->property("hideProgress").toDouble()<.001);
        QTRY_VERIFY(hoverIcon->property("windowCount").toInt()>0);
        QTest::qWait(200);
        const QPoint hoverPoint=hoverIcon->mapToScene(QPointF(hoverIcon->width()/2,hoverIcon->height()/2)).toPoint();
        QEnterEvent enter(hoverPoint,hoverPoint,root->mapToGlobal(hoverPoint));QCoreApplication::sendEvent(root,&enter);
        QTest::mouseMove(root,hoverPoint);
        QTRY_VERIFY(hoverIcon->property("hovered").toBool());
        auto *switcher=panel("windowSwitcher");QVERIFY(switcher);
        QTest::qWait(800);
        QVERIFY(!switcher->isVisible()); // No early 650ms tooltip any more.
        qInfo()<<"Window hover:"<<hoverIcon->property("hovered")<<hoverIcon->property("windowCount")
               <<item(root->contentItem(),"dock")->property("pointerInside")<<root->property("hoveredApplication")<<switcher->isVisible()<<hoverPoint;
        QTRY_VERIFY_WITH_TIMEOUT(switcher->isVisible(),3200);
        QTRY_COMPARE(switcher->property("desktopId").toString(),id);
        QCOMPARE(switcher->property("glassMaterial").toBool(),true);
        QCOMPARE(hoverIcon->property("tooltipSuppressed").toBool(),true);
        QTRY_COMPARE(switcher->property("revealProgress").toDouble(),1.);
        const auto previewLenses=[&] {return QJsonDocument::fromJson(platform->opticalState().toUtf8()).object()["windows"].toObject()["windowSwitcher"].toObject()["lenses"].toArray();};
        QTRY_COMPARE(previewLenses().size(),1);
        QCOMPARE(previewLenses().first().toObject()["blurRadius"].toDouble(),2.5);
        QCOMPARE(previewLenses().first().toObject()["refractionWidth"].toDouble(),10.);
        auto *card=item(switcher->contentItem(),"windowCard"+QString::number(token));QVERIFY(card);
        QTest::mouseMove(switcher,card->mapToScene(QPointF(card->width()/2,card->height()/2)).toPoint());
        QTest::qWait(600);
        QVERIFY(switcher->isVisible());
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_WINDOW_THUMBNAILS")) {
            QVERIFY2(windows->thumbnailAvailable(),"KDE screencast protocol was not offered");
            QQuickItem *preview=nullptr;
            QTRY_VERIFY_WITH_TIMEOUT((preview=item(switcher->contentItem(),"windowPreview"+QString::number(token))),4000);
            QTRY_VERIFY_WITH_TIMEOUT(preview->property("ready").toBool(),8000);
            QVERIFY(preview->property("nodeId").toUInt()>0);
            const auto contentFrame=[&] {
                const auto frame=switcher->grabWindow();
                const qreal dpr=qreal(frame.width())/switcher->width();
                const auto bounds=preview->mapRectToScene(preview->boundingRect());
                return frame.copy(QRectF(bounds.topLeft()*dpr,bounds.size()*dpr).toAlignedRect());
            };
            const auto before=contentFrame();QVERIFY(!before.isNull());
            QTRY_VERIFY_WITH_TIMEOUT(contentFrame()!=before,3000);
            capture(switcher,"-real-window-preview");
        }
        QTest::mouseClick(switcher,Qt::LeftButton,Qt::NoModifier,card->mapToScene(QPointF(card->width()/2,card->height()/2)).toPoint());
        QTRY_VERIFY(!switcher->isVisible());
        settings->setValue("ReducedMotion",motionPreference);
        if(platform->refractionAvailable()) {
            // Settings reach KWin asynchronously. Do not minimize in the same
            // turn that re-enables motion: that would exercise only Squash,
            // then mistake the next one-frame reversal for a full Genie test.
            const auto compositorAnimations=[] {
                QDBusReply<QString> reply=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","diagnostics"));
                return reply.isValid() && QJsonDocument::fromJson(reply.value().toUtf8()).object()["options"].toObject()["windowAnimations"].toBool();
            };
            QTRY_COMPARE(compositorAnimations(),settings->windowIntegration && settings->windowAnimations && !settings->reducedMotion);
        }
        if(state.value("canMinimize").toBool()) {
            const auto motions=[] {
                QDBusReply<QString> reply=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","diagnostics"));
                return reply.isValid()?QJsonDocument::fromJson(reply.value().toUtf8()).object()["activeMotions"].toInt():0;
            };
            QVERIFY(windows->activateApp(id));
            QTRY_VERIFY(windows->windowsFor(id).value(0).toMap().value("minimized").toBool());
            QTRY_COMPARE_WITH_TIMEOUT(motions(),0,3000);
            QTest::qWait(250); // Exercise cleanup after the last minimized frame.
            QVERIFY(windows->activateApp(id));
            QTRY_VERIFY(!windows->windowsFor(id).isEmpty() && !windows->windowsFor(id).value(0).toMap().value("minimized").toBool());
            QTRY_COMPARE_WITH_TIMEOUT(motions(),0,3000);
            // Reversing in flight must not briefly release the original window.
            QVERIFY(windows->activateApp(id));
            QTRY_VERIFY(windows->windowsFor(id).value(0).toMap().value("minimized").toBool());
            QVERIFY(windows->activateApp(id));
            QTRY_VERIFY(!windows->windowsFor(id).value(0).toMap().value("minimized").toBool());
            QTRY_COMPARE_WITH_TIMEOUT(motions(),0,3000);
            QCOMPARE(windows->windowsFor(id).size(),1);
        }
        int row=-1;
        for(int i=0;i<apps->count();++i) if(apps->desktopId(i)==id) row=i;
        QVERIFY(row>=0);
        QTRY_VERIFY(apps->data(apps->index(row),ApplicationModel::RunningRole).toInt()>0);
        QVERIFY(state.value("canClose").toBool());
        QVERIFY(windows->perform(token,"close"));
        QTRY_VERIFY_WITH_TIMEOUT(probe.state()==QProcess::NotRunning,5000);
        QTRY_VERIFY(windows->windowsFor(id).isEmpty());
        QTRY_COMPARE(apps->data(apps->index(row),ApplicationModel::RunningRole).toInt(),0);
        QCOMPARE(resets.count(),0);
        QCOMPARE(root->findChild<QQuickItem *>("launcherButton"),stableIcon);
        QVERIFY(settings->pinnedApps.contains(id));
    }
    void physicalBottomEdge() {
        if(!QGuiApplication::platformName().startsWith("wayland"))QSKIP("Requires KWin QA compositor");
        const auto pointer=[](double x,double y){
            auto call=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","testPointer");
            call<<x<<y;return QDBusConnection::sessionBus().call(call);
        };
        if(pointer(-200,-200).type()==QDBusMessage::ErrorMessage)QSKIP("Production intentionally has no pointer test endpoint");
        settings->setValue("AutoHide",true);settings->setValue("SmartAutoHide",false);
        auto *dock=item(root->contentItem(),"dock");QVERIFY(dock);
        for(const double fraction:{.1,.5,.9}) {
            QMetaObject::invokeMethod(dock,"cancelHover");
            pointer(-200,-200);
            QTest::mouseMove(root,QPoint(-20,-20));
            QTRY_VERIFY_WITH_TIMEOUT(root->property("compact").toBool(),5000);
            QTest::qWait(150); // Await the Wayland configure and metadata commit.
            QDBusReply<QString> reply=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","diagnostics"));
            const auto frame=QJsonDocument::fromJson(reply.value().toUtf8()).object()["dockFrame"].toObject();
            const auto *platform=root->property("platform").value<DockPlatform *>();QVERIFY(platform);
            const auto screen=QJsonDocument::fromJson(platform->opticalState().toUtf8()).object()["dock"].toObject()["screen"].toObject();
            QVERIFY(frame["w"].toDouble()>100);
            pointer(frame["x"].toDouble()+frame["w"].toDouble()*fraction,screen["y"].toDouble()+screen["h"].toDouble()-1);
            QTRY_VERIFY_WITH_TIMEOUT(!root->property("compact").toBool(),2000);
            QTest::qWait(1050);
            QVERIFY(!root->property("compact").toBool());
        }
        pointer(-200,-200);settings->setValue("AutoHide",false);
    }
    void cleanupTestCase() {
        settings->flush();
        for(const QString &name:QStringList{"launcherWindow","settingsWindow","appMenuWindow"})
            if(auto *w=panel(name)) w->hide();
        QSaveFile f(settings->fileName); QVERIFY(f.open(QIODevice::WriteOnly));
        QCOMPARE(f.write(originalConfig),originalConfig.size()); QVERIFY(f.commit());
        root->close();
    }
};
class ControlCenterTests : public QObject {
    Q_OBJECT
public:
    QQuickWindow *root=nullptr;
    DockPlatform *platform=nullptr;
private slots:
    void statusRowClickOpensPanel() {
        auto *cc=panel("controlCenterWindow");
        QVERIFY(cc);
        QTRY_VERIFY(cc->isVisible());
        platform->handleControlCenterEdgeHover(true);
        QTRY_VERIFY(cc->property("statusInteractive").toBool());
        auto *status=item(cc->contentItem(),"controlCenterStatusBar");
        QVERIFY(status);
        const QPoint point=cc->property("statusInputRect").toRectF().center().toPoint();
        const bool native=QGuiApplication::platformName().startsWith("wayland");
        if(native) QTRY_VERIFY_WITH_TIMEOUT(platform->nativePanelGenie(),5000);
        auto *tools=item(cc->contentItem(),"controlCenterTools");
        QVERIFY(tools);
        auto verifyInput=[&] {
            return !native || (!cc->flags().testFlag(Qt::WindowTransparentForInput)
                               && cc->mask().contains(point));
        };
        for(int cycle=0;cycle<3;++cycle) {
            platform->handleControlCenterEdgeHover(false);
            platform->handleControlCenterEdgeHover(true);
            QTRY_VERIFY(cc->property("statusInteractive").toBool());
            QTRY_VERIFY2(verifyInput(),"Visible status row has no Wayland input region");
            QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,point);
            QTRY_VERIFY(cc->property("expanded").toBool());
            if(native) {
                QTest::qWait(50);
                QVERIFY(cc->property("nativeMotionActive").toBool());
            }
            QTRY_COMPARE_WITH_TIMEOUT(cc->property("revealProgress").toDouble(),1.,2500);
            QVERIFY(tools->isVisible());
            QTRY_VERIFY2(verifyInput(),"Expanded panel no longer accepts a click on its status row");
            if(native) {
                const QImage frame=cc->grabWindow();
                QVERIFY(!frame.isNull());
                const auto center=tools->mapToScene(QPointF(tools->width()/2,tools->height()/2));
                const auto pixel=center*(qreal(frame.width())/cc->width());
                QVERIFY(frame.pixelColor(pixel.toPoint()).alpha()>0);
                QVERIFY(frame.save(QStringLiteral(DOCK_TEST_CONFIG)+"-control-center-open.png"));
            }
            QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,point);
            QTRY_VERIFY(!cc->property("expanded").toBool());
            QTRY_COMPARE_WITH_TIMEOUT(cc->property("revealProgress").toDouble(),0.,2500);
            QVERIFY(!tools->isVisible());
            platform->handlePanelOutside("controlCenterWindow");
        }
        // Reverse a close while it is still animating.
        platform->handleControlCenterEdgeHover(false);
        platform->handleControlCenterEdgeHover(true);
        QTRY_VERIFY(cc->property("statusInteractive").toBool());
        QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,point);
        QTest::qWait(100);
        QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,point);
        QTest::qWait(50);
        QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,point);
        QTRY_COMPARE_WITH_TIMEOUT(cc->property("revealProgress").toDouble(),1.,2500);
        QVERIFY(cc->property("expanded").toBool());

    }
    void fullscreenSliderInput() {
        auto *cc=panel("controlCenterWindow");QVERIFY(cc);
        cc->setProperty("expanded",false);
        QTRY_COMPARE(cc->property("revealProgress").toDouble(),0.);
        const bool native=QGuiApplication::platformName().startsWith("wayland");
        QProcess underlay;
        const auto underlayCleanup=qScopeGuard([&]{
            if(underlay.state()!=QProcess::NotRunning){underlay.terminate();if(!underlay.waitForFinished(2000)){underlay.kill();underlay.waitForFinished();}}
        });
        if(native) {
            underlay.start(QStringLiteral(DOCK_WINDOW_PROBE),{"--fullscreen"});
            QVERIFY(underlay.waitForStarted());
            QTRY_VERIFY_WITH_TIMEOUT(platform->statusObstructed(),5000);
        }
        // Exercise the real slider bindings without writing to audio/display devices.
        QQmlComponent component(qmlEngine(cc));
        component.setData(R"(
            import QtQml
            QtObject {
                property int volume: 30
                property int brightness: 40
                property int keyboardBrightness: 20
                property int battery: 80
                property bool charging: false
                property bool focusActive: false
                property bool wifiEnabled: true
                property bool bluetoothEnabled: false
                property string wifiName: "Test Wi-Fi"
                property string timeText: "12:00"
                property string dateText: "Test"
                function setVolume(v) { volume=v }
                function setBrightness(v) { brightness=v }
                function setKeyboardBrightness(v) { keyboardBrightness=v }
            }
        )",QUrl());
        QScopedPointer<QObject> telemetry(component.create());
        QVERIFY2(telemetry,qPrintable(component.errorString()));
        const auto original=cc->property("telemetry");
        const auto restore=qScopeGuard([&]{cc->setProperty("telemetry",original);});
        cc->setProperty("telemetry",QVariant::fromValue(telemetry.data()));
        platform->handleControlCenterEdgeHover(false);
        platform->handleControlCenterEdgeHover(true);
        QTRY_VERIFY(cc->property("statusInteractive").toBool());
        const auto row=cc->property("statusInputRect").toRectF().center().toPoint();
        QTest::mouseClick(cc,Qt::LeftButton,Qt::NoModifier,row);
        if(native) {
            QTest::qWait(50);
            for(const QString &name:QStringList{"brightnessControl","volumeControl"}) {
                auto *control=item(cc->contentItem(),name);QVERIFY(control);
                const auto center=control->mapToScene(QPointF(control->width()/2,control->height()/2)).toPoint();
                QVERIFY2(cc->mask().contains(center),qPrintable(name+" leaks clicks during opening"));
            }
        }
        QTRY_COMPARE(cc->property("revealProgress").toDouble(),1.);
        for(const QString &name:QStringList{"brightnessControl","volumeControl"}) {
            auto *slider=item(cc->contentItem(),name);QVERIFY(slider);
            const auto start=slider->mapToScene(QPointF(52+(slider->width()-108)*.2,slider->height()/2)).toPoint();
            const auto finish=slider->mapToScene(QPointF(52+(slider->width()-108)*.8,slider->height()/2)).toPoint();
            if(native) {
                for(int x=start.x();x<=finish.x();++x)
                    QVERIFY2(cc->mask().contains(QPoint(x,start.y())),qPrintable(name+" track passes input through to fullscreen client"));
                QVERIFY(!cc->flags().testFlag(Qt::WindowTransparentForInput));
            }
            QSignalSpy moved(slider,SIGNAL(moved(double)));
            QVERIFY(moved.isValid());
            QTest::mousePress(cc,Qt::LeftButton,Qt::NoModifier,start);
            QVERIFY(slider->property("dragging").toBool());
            QTest::mouseMove(cc,finish,30);
            QTest::mouseRelease(cc,Qt::LeftButton,Qt::NoModifier,finish);
            QVERIFY(!slider->property("dragging").toBool());
            QVERIFY(moved.count()>=2);
            const char *key=name=="brightnessControl"?"brightness":"volume";
            QTRY_VERIFY(qAbs(telemetry->property(key).toInt()-80)<=1);
            QVERIFY(cc->property("expanded").toBool());
        }
    }

    void panelBoundsAndHoverRepaints() {
        auto *cc=panel("controlCenterWindow");QVERIFY(cc);
        cc->setProperty("expanded",true);
        QTRY_COMPARE(cc->property("revealProgress").toDouble(),1.);
        auto *tools=item(cc->contentItem(),"controlCenterTools");QVERIFY(tools);
        auto *last=item(cc->contentItem(),"keyboardBrightnessControl");QVERIFY(last);
        const QPointF bottom=last->mapToScene(QPointF(last->width()/2,last->height()));
        const QRectF bounds=cc->property("restingFrame").toRectF();
        QVERIFY2(bounds.bottom()<=bottom.y()+8,"Invisible panel extends below its last control");
        const QPoint outside(qRound(bottom.x()),qCeil(bottom.y()+12));
        QVERIFY(!bounds.contains(outside));
        const bool native=QGuiApplication::platformName().startsWith("wayland");
        if(native) QVERIFY2(!cc->mask().contains(outside),"Click below visible controls is swallowed");
        if(native) {
            const QImage frame=cc->grabWindow();QVERIFY(!frame.isNull());
            const auto sample=(bottom+QPointF(0,8))*(qreal(frame.width())/cc->width());
            QCOMPARE(frame.pixelColor(sample.toPoint()).alpha(),0);
        }
        const auto repaintCount=[] {
            QDBusReply<QString> reply=QDBusConnection::sessionBus().call(QDBusMessage::createMethodCall(
                "org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","diagnostics"));
            return QJsonDocument::fromJson(reply.value().toUtf8()).object()["controlCenterRepaints"].toInteger();
        };
        auto *slider=item(cc->contentItem(),"volumeControl");QVERIFY(slider);
        const auto original=slider->property("liveValue");
        const auto restore=qScopeGuard([&]{slider->setProperty("liveValue",original);});
        for(int step=0;step<5;++step) {
            const auto before=native?repaintCount():0;
            const auto point=slider->mapToScene(QPointF(60+step*30,24)).toPoint();
            QTest::mouseMove(cc,point);
            // Small slider damage, like a hover/drag update, must refresh the
            // whole sampled backdrop without feeding previous glass into it.
            slider->setProperty("liveValue",.21+step*.11);
            if(native) QTRY_VERIFY_WITH_TIMEOUT(repaintCount()>before,2000);
        }
        platform->handlePanelOutside("controlCenterWindow");
        QTRY_COMPARE(cc->property("revealProgress").toDouble(),0.);
        QVERIFY(!tools->isVisible());
        if(native) {
            const auto closed=repaintCount();
            QTest::qWait(150);
            QCOMPARE(repaintCount(),closed);
        }
    }

};
int runStage4Tests(QQuickWindow *r,DockSettings *s,ApplicationModel *a,ApplicationCatalogue *c,WindowTracker *w) {
    Stage4Tests test; test.root=r;test.settings=s;test.apps=a;test.catalogue=c;test.windows=w;
    return QTest::qExec(&test,QStringList{"stage4-tests"});
}
int runControlCenterTests(QQuickWindow *r,DockPlatform *p) {
    ControlCenterTests test;test.root=r;test.platform=p;
    const int result=QTest::qExec(&test,QStringList{"control-center-tests"});
    r->close();
    return result;
}
#include "stage4tests.moc"
