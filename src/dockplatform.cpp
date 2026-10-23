#include "dockplatform.h"
#include "docksettings.h"
#include "windowtracker.h"
#include "../kwin/geniegeometry.h"
#include <LayerShellQt/Window>
#include <KWindowEffects>
#include <QGuiApplication>
#include <QPainterPath>
#include <QQuickWindow>
#include <QRunnable>
#include <QScreen>
#include <QTimer>
#include <QQuickItem>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <KService>
#ifdef HAVE_KWAYLAND
#include <KWayland/Client/surface.h>
#include <wayland-client-core.h>
#endif
namespace {
QRegion rounded(const QRectF &rect,qreal radius) {
    QPainterPath path; path.addRoundedRect(rect,radius,radius);
    return QRegion(path.toFillPolygon().toPolygon());
}
QPainterPath genieShape(const QRectF &frame,const QRectF &icon,qreal openness,qreal radius) {
    QPainterPath original,result;
    original.addRoundedRect(frame,radius,radius);
    for(int i=0;i<=160;++i) {
        const QPointF point=original.pointAtPercent(i/160.);
        const QPointF warped=MacGlassGenie::map(point-frame.topLeft(),frame,icon,1.-qBound(0.,openness,1.));
        if(i==0)result.moveTo(warped);else result.lineTo(warped);
    }
    result.closeSubpath();return result;
}
bool wayland() { return QGuiApplication::platformName().startsWith("wayland"); }
void mask(QQuickWindow *window,const QRegion &region) {
    if(QGuiApplication::platformName()=="offscreen") return;
    const bool transparentForInput=window->flags().testFlag(Qt::WindowTransparentForInput);
    if(region.isEmpty()) {
        // On Wayland an empty wl_surface input region means "infinite". Qt's
        // empty QWindow mask therefore restores input to the whole surface.
        // This flag asks QtWayland to send a deliberately empty input region.
        if(!transparentForInput) window->setFlag(Qt::WindowTransparentForInput,true);
        return;
    }
    // While hidden, update the stored mask before restoring input so a stale
    // visible region cannot receive events for even one compositor commit.
    if(window->mask()!=region) window->setMask(region);
    if(transparentForInput) window->setFlag(Qt::WindowTransparentForInput,false);
}
}
DockPlatform::DockPlatform(QObject *parent) : QObject(parent) {
    // Coalesce within an event-loop turn. Restarting a 16ms debounce on each
    // animated geometry change can starve publication for the entire animation.
    m_publishTimer.setSingleShot(true); m_publishTimer.setInterval(0);
    connect(&m_publishTimer,&QTimer::timeout,this,&DockPlatform::publishOptics);
    auto bus=QDBusConnection::sessionBus();
    m_busOwner=bus.registerService("org.macglass.Dock");
    if(m_busOwner) bus.registerObject("/Dock",this,QDBusConnection::ExportScriptableSlots|QDBusConnection::ExportScriptableSignals);
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","revealRequested",this,SLOT(handleReveal()));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","pointerOutsideDock",this,SLOT(handlePointerOutside()));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","edgeHoverChanged",this,SLOT(handleEdgeHover(bool)));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","controlCenterEdgeHoverChanged",this,SLOT(handleControlCenterEdgeHover(bool)));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","obstructedChanged",this,SLOT(handleObstructed(bool)));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","statusObstructedChanged",this,SLOT(handleStatusObstructed(bool)));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","panelOutsidePressed",this,SLOT(handlePanelOutside(QString)));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","controlCenterStatusPressed",this,SLOT(handleControlCenterStatusPressed()));
    bus.connect("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","panelMotionReady",this,SLOT(handlePanelPrepared(QString,uint)));
    m_probeTimer.setInterval(3000);
    connect(&m_probeTimer,&QTimer::timeout,this,&DockPlatform::probeRefraction);
    m_probeTimer.start();
    QTimer::singleShot(0,this,&DockPlatform::probeRefraction);
    connect(qGuiApp,&QGuiApplication::screenAdded,this,[this]{emit screensChanged();selectScreen();});
    connect(qGuiApp,&QGuiApplication::screenRemoved,this,[this]{emit screensChanged();QTimer::singleShot(0,this,&DockPlatform::selectScreen);});
}
void DockPlatform::setSettings(DockSettings *settings) {
    m_settings=settings;
    connect(&m_desktop,&DesktopIntegration::activated,this,[settings]{
        settings->setValue("WindowIntegration",true);
        settings->setValue("AutoHide",true);
        settings->setValue("SmartAutoHide",false);
    });
    connect(&m_desktop,&DesktopIntegration::restored,qGuiApp,&QCoreApplication::quit);
    connect(settings,&DockSettings::changed,this,[this]{
        scheduleOptics();
        selectScreen(); applyRegion();
        for(auto *w:m_panels.keys()) applyPanel(w);
    });
}
QStringList DockPlatform::screenNames() const {
    QStringList result;
    for(auto *s:qGuiApp->screens()) result.append(s->name());
    return result;
}
void DockPlatform::setWindowTracker(WindowTracker *windows) {
    m_windows=windows;
    connect(windows,&WindowTracker::changed,this,&DockPlatform::scheduleOptics);
}
void DockPlatform::selectScreen() {
    if(!m_window || !m_settings) return;
    QScreen *selected=qGuiApp->primaryScreen();
    for(auto *screen:qGuiApp->screens()) if(screen->name()==m_settings->screenName) selected=screen;
    if(!selected || m_window->screen()==selected) return;
    m_window->setScreen(selected);
    if(!m_preview && wayland()) LayerShellQt::Window::get(m_window)->setScreen(selected);
}
void DockPlatform::configure(QQuickWindow *window,bool preview) {
    m_window=window; m_preview=preview;
    emit effectsChanged();
    if(!preview && wayland()) {
        auto *layer=LayerShellQt::Window::get(window);
        layer->setScope("mac-glass-dock:dockWindow");
        layer->setAnchors(LayerShellQt::Window::AnchorBottom);
        // A revealed Dock must also be visible above a fullscreen application.
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        // -1 explicitly anchors to the physical output, ignoring the exclusive
        // zone of another panel. 0 anchors above a reserving Plasma panel.
        layer->setExclusiveZone(-1);
        layer->setMargins(QMargins(0,0,0,4));
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
        layer->setCloseOnDismissed(false);
    } else if(!preview && QGuiApplication::platformName()!="offscreen") {
        const QRect area=window->screen()->availableGeometry();
        window->setPosition(area.center().x()-window->width()/2,area.bottom()-window->height()-10);
    }
    selectScreen();
    // Auxiliary overlay panels can be created by QML before the main Dock
    // window is configured. Keep them on the same output after that handoff.
    for (auto *panel : m_panels.keys()) {
        if (panel && window->screen()) panel->setScreen(window->screen());
        applyPanel(panel);
    }
    connect(window,&QQuickWindow::frameSwapped,this,[this]{if(!m_publishTimer.isActive())m_publishTimer.start();});
    connect(window,&QWindow::visibleChanged,this,&DockPlatform::applyRegion);
    connect(window,&QWindow::widthChanged,this,&DockPlatform::applyRegion);
    connect(window,&QWindow::heightChanged,this,&DockPlatform::applyRegion);
    connect(window,&QQuickWindow::sceneGraphInitialized,this,[this]{QTimer::singleShot(0,this,&DockPlatform::applyRegion);});
    QTimer::singleShot(1000,this,&DockPlatform::applyRegion);
}
void DockPlatform::updateRegion(const QRectF &surface,qreal radius,const QVariantList &icons) {
    m_surface=surface; m_radius=radius; m_icons=icons; applyRegion(); scheduleOptics();
}
void DockPlatform::setHidden(bool hidden) {
    if(m_hidden==hidden) return;
    m_hidden=hidden;
    if(m_window && !m_preview && wayland()) LayerShellQt::Window::get(m_window)->setMargins(QMargins(0,0,0,hidden?0:4));
    applyRegion();scheduleOptics();
}
void DockPlatform::handleEdgeHover(bool hovered) {
    if(hovered==m_edgeHovered)return;
    m_edgeHovered=hovered;emit edgeHoverChanged();
}
void DockPlatform::handleControlCenterEdgeHover(bool hovered) {
    if(hovered==m_controlCenterEdgeHovered)return;
    m_controlCenterEdgeHovered=hovered;
    emit controlCenterEdgeHoverChanged(hovered);
}
void DockPlatform::updateTargets(const QVariantList &targets,const QRectF &surface) {
    if(!m_window || (targets==m_targets && surface==m_targetSurface))return;
    m_targets=targets;m_targetSurface=surface;scheduleOptics();
}
void DockPlatform::updateStatusRegion(const QRectF &surface) {
    if (surface == m_statusRegion) return;
    m_statusRegion = surface;
    scheduleOptics();
}
void DockPlatform::setPanelKeyboardFocus(QQuickWindow *window, bool focused) {
    if (!window || m_preview || !wayland()) return;
    auto *layer = LayerShellQt::Window::get(window);
    if (!layer) return;
    layer->setKeyboardInteractivity(focused ? LayerShellQt::Window::KeyboardInteractivityExclusive
                                            : LayerShellQt::Window::KeyboardInteractivityOnDemand);
    if (focused) window->requestActivate();
}
void DockPlatform::handleObstructed(bool obstructed) {
    if(obstructed==m_obstructed)return;
    m_obstructed=obstructed;emit obstructionChanged();
}
void DockPlatform::handleStatusObstructed(bool obstructed) {
    if(obstructed==m_statusObstructed)return;
    m_statusObstructed=obstructed;emit statusObstructionChanged();
}
void DockPlatform::applyRegion() {
    if(!m_window || m_surface.isEmpty()) return;
    const QRegion glass=rounded(m_surface,m_radius);
    QRegion input=glass;
    QRegion blur=glass;
    for(const auto &icon:m_icons) {
        const auto rect=icon.toRectF();
        const auto iconRegion=rounded(rect,rect.width()*(m_settings?m_settings->iconRadius:.25));
        input|=iconRegion;
        if(m_settings && m_settings->iconBlurEnabled && m_settings->iconStyle!="original") blur|=iconRegion;
    }
    mask(m_window,input);
    const bool available=KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind);
    if(available!=m_blurAvailable) {m_blurAvailable=available;emit effectsChanged();}
    KWindowEffects::enableBlurBehind(m_window,available && !refractionAvailable() && !m_hidden && (!m_settings || m_settings->blurEnabled),blur);
    if(KWindowEffects::isEffectAvailable(KWindowEffects::BackgroundContrast))
        KWindowEffects::enableBackgroundContrast(m_window,!refractionAvailable() && !m_hidden && (!m_settings || m_settings->blurEnabled),1.03,1,1.02,glass);
}
void DockPlatform::hidePanelAfterFrame(QQuickWindow *window,int generation) {
    if(!window || !m_panels.contains(window))return;
    const QPointer<QQuickWindow> guarded=window;
    const auto finish=[guarded,generation]{
        if(guarded)QMetaObject::invokeMethod(guarded,"finishDismiss",Q_ARG(QVariant,QVariant(generation)));
    };
    // Two stages: the sync job belongs to the frame which sees terminalHidden,
    // then the swap job queues GUI-side hide after that buffer was submitted.
    window->scheduleRenderJob(QRunnable::create([guarded,finish]{
        if(guarded)guarded->scheduleRenderJob(QRunnable::create([guarded,finish]{
            if(guarded)QMetaObject::invokeMethod(guarded,finish,Qt::QueuedConnection);
        }),QQuickWindow::AfterSwapStage);
    }),QQuickWindow::AfterSynchronizingStage);
    window->update();
    // An unexposed/minimized output may never render; don't retain an input
    // surface forever. The compositor-side terminal guard also covers this.
    QTimer::singleShot(750,window,finish);
}
void DockPlatform::preparePanel(QQuickWindow *window,const QString &kind) {
    if(m_panels.contains(window)) return;
    m_panels.insert(window,{QRectF(24,24,window->width()-48,window->height()-48),30});
    if(m_window) window->setScreen(m_window->screen());
    if(!m_preview && wayland()) {
        auto *layer=LayerShellQt::Window::get(window);
        layer->setScope("mac-glass-dock:"+window->objectName());
        // The Control Center behaves like the Dock: edge reveal above normal
        // windows. Its edge handle is empty while idle and fullscreen-aware.
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setExclusiveZone(kind=="launcher" ? -1 : 0);
        layer->setKeyboardInteractivity(kind=="windows" ? LayerShellQt::Window::KeyboardInteractivityNone : LayerShellQt::Window::KeyboardInteractivityOnDemand);
        layer->setCloseOnDismissed(false);
        if(kind=="controlcenter") {
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorRight)|LayerShellQt::Window::AnchorTop);
            // Keep the control center above fullscreen clients without reserving
            // a permanent top-panel zone on the desktop.
            // Reach the physical right edge so clicks made on the edge reveal
            // strip can land on the status bar's transparent hit extension.
            layer->setMargins(QMargins(0,4,0,0));
        } else if(kind=="settings") {
            layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorRight)|LayerShellQt::Window::AnchorTop);
            layer->setMargins(QMargins(0,36,18,0));
        } else if(kind=="windows") {
            layer->setAnchors(LayerShellQt::Window::AnchorBottom);
            layer->setMargins(QMargins(0,0,0,qRound((m_settings?m_settings->iconSize:56)*1.65+30)));
        } else if(kind=="menu") {
            layer->setAnchors(LayerShellQt::Window::AnchorBottom);
            layer->setMargins(QMargins(0,0,0,150));
        } else layer->setAnchors(LayerShellQt::Window::AnchorNone);
    }
    connect(window,&QObject::destroyed,this,[this,window]{m_panels.remove(window);});
    connect(window,&QQuickWindow::frameSwapped,this,[this]{if(!m_publishTimer.isActive())m_publishTimer.start();});
    connect(window,&QQuickWindow::sceneGraphInitialized,this,[this,window]{QTimer::singleShot(0,this,[this,window]{applyPanel(window);});});
    connect(window,&QWindow::visibleChanged,this,[this,window]{applyPanel(window);});
    QTimer::singleShot(350,this,[this,window]{applyPanel(window);});
}
QPointF DockPlatform::panelAnchor(QQuickWindow *panel,const QPointF &dockPoint) const {
    if(!m_window || !panel)return {};
    if(!m_preview && wayland() && m_window->screen()) {
        const QRect screen=m_window->screen()->geometry();
        // Layer-shell placement is compositor-owned; QWindow::position is not
        // portable on Wayland. Both calculations use logical output pixels.
        const QPointF dockOrigin(screen.x()+(screen.width()-m_window->width())/2.,
                                 screen.y()+screen.height()-4-m_window->height());
        const QPointF panelOrigin(screen.x()+(screen.width()-panel->width())/2.,
                                  screen.y()+(screen.height()-panel->height())/2.);
        return dockOrigin+dockPoint-panelOrigin;
    }
    return m_window->mapToGlobal(dockPoint)-QPointF(panel->position());
}
void DockPlatform::handlePanelPrepared(const QString &name,uint surfaceId) {
#ifdef HAVE_KWAYLAND
    for(auto *window:m_panels.keys()) {
        if(window->objectName()!=name || !window->isVisible())continue;
        if(auto *surface=KWayland::Client::Surface::fromWindow(window))
            if(wl_proxy_get_id(reinterpret_cast<wl_proxy *>(static_cast<wl_surface *>(*surface)))==surfaceId)emit panelMotionPrepared(name);
    }
#else
    Q_UNUSED(name);Q_UNUSED(surfaceId);
#endif
}
void DockPlatform::updatePanelRegion(QQuickWindow *window,const QRectF &rect,qreal radius) {
    if(!m_panels.contains(window)) return;
    m_panels[window]={rect,radius}; applyPanel(window); scheduleOptics();
}
QRectF DockPlatform::geniePanelBounds(const QRectF &frame,const QRectF &icon,qreal openness,qreal radius) const {
    return genieShape(frame,icon,openness,radius).boundingRect();
}
void DockPlatform::updateGeniePanelRegion(QQuickWindow *window,const QRectF &frame,const QRectF &icon,qreal openness,qreal radius) {
    if(!m_panels.contains(window))return;
    const auto shape=genieShape(frame,icon,openness,radius);
    m_panels[window]={shape.boundingRect(),0,QRegion(shape.toFillPolygon().toPolygon())};
    applyPanel(window);scheduleOptics();
}
void DockPlatform::applyPanel(QQuickWindow *window) {
    if(!m_panels.contains(window)) return;
    const auto state=m_panels.value(window);
    auto region=state.shape.isEmpty()?rounded(state.rect,state.radius):state.shape;
    if(window->property("kind").toString()=="controlcenter" && window->property("statusShown").toBool())
        region|=rounded(window->property("statusInputRect").toRectF(),14);
    mask(window,region);
    const bool glass=(!window->property("glassMaterial").isValid() || window->property("glassMaterial").toBool()) && !window->property("genieActive").toBool();
    KWindowEffects::enableBlurBehind(window,glass && !refractionAvailable() && KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind)
        && (!m_settings || m_settings->blurEnabled),region);
}

void DockPlatform::probeRefraction() {
#ifndef HAVE_KWAYLAND
    return;
#endif
    if(m_probePending || !m_busOwner || !wayland() || m_preview) return;
    m_probePending=true;
    auto call=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","status");
    auto *watcher=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(call,1000),this);
    connect(watcher,&QDBusPendingCallWatcher::finished,this,[this,watcher]{
        QDBusPendingReply<QString> reply=*watcher;
        const bool ready=!reply.isError() && reply.value()=="ready:v1";
        m_probePending=false; watcher->deleteLater();
        if(ready) {
            auto probe=QDBusMessage::createMethodCall("org.kde.KWin","/MacGlassRefraction","org.macglass.Refraction","panelGenieSupported");
            auto *capability=new QDBusPendingCallWatcher(QDBusConnection::sessionBus().asyncCall(probe,1000),this);
            connect(capability,&QDBusPendingCallWatcher::finished,this,[this,capability]{
                QDBusPendingReply<bool> support=*capability;
                const bool available=!support.isError() && support.value();
                capability->deleteLater();
                if(available!=m_nativePanelGenie) {m_nativePanelGenie=available;emit effectsChanged();scheduleOptics();}
            });
        } else m_nativePanelGenie=false;
        if(!ready) {handleEdgeHover(false);handleControlCenterEdgeHover(false);handleObstructed(false);}
        if(ready==m_refractionAvailable) return;
        m_refractionAvailable=ready; emit effectsChanged(); applyRegion();
        for(auto *w:m_panels.keys()) applyPanel(w);
        m_opticalState.clear(); m_publishTimer.start();
    });
}

void DockPlatform::publishOptics() {
    if(!m_busOwner || !m_settings || m_preview) return;
    if(m_windows && m_window) {
        QVariantList local;
        for(const auto &value:m_targets) {
            auto target=value.toMap();
            target["rect"]=target["rect"].toRectF().translated(m_window->width()/2.,m_window->height()-(m_hidden?4:0));
            local.append(target);
        }
        m_windows->setIconGeometries(m_window,local);
    }
    QJsonObject state{{"protocol",1},{"pid",int(QCoreApplication::applicationPid())}};
    if(m_windows)state["tracking"]=QJsonObject{{"available",m_windows->available()},{"thumbnails",m_windows->thumbnailAvailable()}};
#ifdef DOCK_TESTING
    state["syntheticInput"]=QCoreApplication::arguments().contains("--stage4-test") || QCoreApplication::arguments().contains("--interaction-test");
#endif
    QJsonObject options;
    for(const char *key:{"blurRadius","refractionStrength","refractionWidth","refractionCurve","chromaticAberration","centerZoom","iconBlurRadius","iconRefractionStrength","iconRefractionWidth"})
        options[key]=m_settings->property(key).toDouble();
    options["blurEnabled"]=m_settings->blurEnabled;
    options["iconBlurEnabled"]=m_settings->iconBlurEnabled;
    state["options"]=options;
    if(m_window && m_window->screen()) {
        uint32_t id=0;
#ifdef HAVE_KWAYLAND
        if(auto *s=KWayland::Client::Surface::fromWindow(m_window)) id=wl_proxy_get_id(reinterpret_cast<wl_proxy *>(static_cast<wl_surface *>(*s)));
#endif
        QJsonArray targets;
        for(const auto &value:m_targets.mid(0,128)) {
            const auto map=value.toMap();const auto r=map["rect"].toRectF();
            const QString desktopId=map["id"].toString();
            if(desktopId.isEmpty() || desktopId.contains('/'))continue;
            if(!m_targetAliases.contains(desktopId)) {
                QStringList aliases{desktopId};
                if(auto service=KService::serviceByStorageId(desktopId)) aliases<<service->desktopEntryName()<<service->property<QString>("StartupWMClass");
                m_targetAliases.insert(desktopId,aliases);
            }
            const auto aliases=m_targetAliases.value(desktopId);
            QJsonArray names;for(auto alias:aliases) {if(alias.endsWith(".desktop"))alias.chop(8);if(!alias.isEmpty())names.append(alias.toCaseFolded());}
            targets.append(QJsonObject{{"id",desktopId},{"aliases",names},{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()}});
        }
        const QRect screen=m_window->screen()->geometry();
        state["dock"]=QJsonObject{{"surface",int(id)},{"hidden",m_hidden},{"autoHide",m_settings->autoHide},
            {"hitRect",QJsonObject{{"x",m_targetSurface.x()},{"y",m_targetSurface.y()},{"w",m_targetSurface.width()},{"h",m_targetSurface.height()}}},
            {"statusRegion",QJsonObject{{"x",m_statusRegion.x()},{"y",m_statusRegion.y()},{"w",m_statusRegion.width()},{"h",m_statusRegion.height()}}},
            {"screen",QJsonObject{{"x",screen.x()},{"y",screen.y()},{"w",screen.width()},{"h",screen.height()}}},{"targets",targets}};
    }
    state["backend"]=refractionAvailable();
    options["windowAnimations"]=m_settings->windowIntegration && m_settings->windowAnimations && !m_settings->reducedMotion;
    options["animationSpeed"]=m_settings->animationSpeed;
    state["options"]=options;
    QJsonObject windows;
    QList<QQuickWindow *> all=m_panels.keys();
    if(m_window) all.prepend(m_window);
    for(auto *window:all) {
        if(!window->isVisible() || (window==m_window && m_hidden)) continue;
        if(window->property("glassMaterial").isValid() && !window->property("glassMaterial").toBool()) continue;
        QJsonArray lenses;
        const auto visit=[&](auto &&self,QQuickItem *item,qreal opacity)->void {
            opacity*=item->opacity();
            if(!item->isVisible() || opacity<.01) return;
            if(item->property("glassSurface").toBool()) {
                // Clip delegates to every clipping ancestor (e.g. GridView).
                // Otherwise off-screen tiles become phantom compositor lenses.
                const QRectF r=item->mapRectToScene(item->boundingRect());
                QRectF visible=r.intersected(QRectF(0,0,window->width(),window->height()));
                for(auto *ancestor=item->parentItem();ancestor;ancestor=ancestor->parentItem())
                    if(ancestor->clip()) visible=visible.intersected(ancestor->mapRectToScene(ancestor->boundingRect()));
                if(visible.width()>1 && visible.height()>1 && lenses.size()<128) {
                    QJsonObject lens{{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()},
                        {"r",item->property("cornerRadius").toDouble()*r.width()/qMax(1.,item->width())},
                        {"icon",item->property("iconSurface").toBool()},{"opacity",opacity},
                        {"clip",QJsonObject{{"x",visible.x()},{"y",visible.y()},{"w",visible.width()},{"h",visible.height()}}}};
                    for(const auto &[property,key]:{std::pair{"backdropBlurRadius","blurRadius"},
                            std::pair{"backdropRefractionStrength","refractionStrength"},std::pair{"backdropRefractionWidth","refractionWidth"}}) {
                        const QVariant override=item->property(property);
                        if(override.isValid() && override.toDouble()>=0)lens[key]=override.toDouble();
                    }
                    lenses.append(lens);
                }
            }
            for(auto *child:item->childItems()) self(self,child,opacity);
        };
        const bool moving=window->property("genieActive").toBool();
        const bool nativeMoving=window->property("nativeMotionActive").toBool() && nativePanelGenie();
        if(!moving || nativeMoving)visit(visit,window->contentItem(),1.);
        uint32_t surfaceId=0;
#ifdef HAVE_KWAYLAND
        if(auto *surface=KWayland::Client::Surface::fromWindow(window))
            surfaceId=wl_proxy_get_id(reinterpret_cast<wl_proxy *>(static_cast<wl_surface *>(*surface)));
#endif
        const auto input=m_panels.value(window);
        QJsonObject description{{"width",window->width()},{"height",window->height()},{"surface",int(surfaceId)},{"lenses",lenses},
            {"presented",window->property("presented").toBool()},{"terminalHidden",window->property("terminalHidden").toBool()},
            {"stableInput",window->property("stablePanelInput").toBool()},
            {"dismissOutside",window->property("kind").toString()=="launcher" || window->property("kind").toString()=="windows" || window->property("kind").toString()=="controlcenter"},
            {"input",QJsonObject{{"x",input.rect.x()},{"y",input.rect.y()},{"w",input.rect.width()},{"h",input.rect.height()},{"r",input.radius}}}};
        if(nativeMoving) {
            const auto rectJson=[](const QRectF &r){return QJsonObject{{"x",r.x()},{"y",r.y()},{"w",r.width()},{"h",r.height()}};};
            description["genie"]=QJsonObject{{"frame",rectJson(window->property("restingFrame").toRectF())},
                {"icon",rectJson(window->property("destinationIcon").toRectF())},{"openness",window->property("revealProgress").toDouble()},
                {"radius",window->property("panelRadius").toDouble()},{"waiting",window->property("waitingForNative").toBool()},
                {"clipFrame",window->property("genieClipFrame").toBool()}};
        }
        windows[window->objectName()]=description;
    }
    state["windows"]=windows;
    const QString json=QString::fromUtf8(QJsonDocument(state).toJson(QJsonDocument::Compact));
    if(json==m_opticalState) return;
    m_opticalState=json; emit opticalStateChanged(json);
}

void DockPlatform::scheduleOptics() {
    if(!m_publishTimer.isActive()) m_publishTimer.start();
}
