#include "windowtracker.h"
#include <QCoreApplication>
#include <QGuiApplication>
#include <QHash>
#include <QPointer>
#include <KService>
#include <algorithm>
#include <QRectF>
#ifdef HAVE_KWAYLAND
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/plasmawindowmanagement.h>
#include <KWayland/Client/surface.h>
using namespace KWayland::Client;
#endif

struct WindowTracker::Private {
    bool enabled = false, available = false, thumbnails=false;
    QString status = QStringLiteral("Интеграция окон выключена");
    QHash<QString, QString> aliases;
#ifdef HAVE_KWAYLAND
    QPointer<Registry> registry;
    QHash<int, QPointer<PlasmaWindow>> windows;
    QHash<int,QRect> iconGeometries;
    int nextToken = 1;
    QString desktopId(const QString &appId) const {
        QString key = appId.toCaseFolded();
        if (key.endsWith(".desktop")) key.chop(8);
        return aliases.value(key, appId.endsWith(".desktop") ? appId : appId + ".desktop");
    }
#endif
};

WindowTracker::WindowTracker(QObject *parent) : QObject(parent), d(std::make_unique<Private>())
{
    for (const auto &s : KService::allServices()) {
        if (!s->isApplication()) continue;
        const QString id = s->storageId();
        QString key = id.toCaseFolded();
        if (key.endsWith(".desktop")) key.chop(8);
        d->aliases.insert(key, id);
        const QString wmClass = s->property<QString>(QStringLiteral("StartupWMClass")).toCaseFolded();
        if (!wmClass.isEmpty()) d->aliases.insert(wmClass, id);
    }
}
WindowTracker::~WindowTracker() = default;
bool WindowTracker::enabled() const { return d->enabled; }
bool WindowTracker::available() const { return d->available; }
bool WindowTracker::thumbnailAvailable() const {return d->available && d->thumbnails;}
QString WindowTracker::status() const { return d->status; }
bool WindowTracker::activeWindowExpanded() const {return expandedOn({});}
bool WindowTracker::expandedOn(const QRectF &screen) const {
#ifdef HAVE_KWAYLAND
    for(const auto &w:d->windows) {
        if(!w || w->skipTaskbar() || !w->isActive() || w->isMinimized()) continue;
        if((w->isMaximized() || w->isFullscreen()) && (screen.isEmpty() || w->geometry().isEmpty() || screen.intersects(w->geometry()))) return true;
    }
#else
    Q_UNUSED(screen)
#endif
    return false;
}

void WindowTracker::setEnabled(bool enabled)
{
    if (enabled == d->enabled) return;
    d->enabled = enabled;
    d->available = false;
    d->thumbnails=false;
#ifdef HAVE_KWAYLAND
    if (d->registry) delete d->registry;
    d->windows.clear();
#endif
    d->status = tr("Интеграция окон выключена");
    if (enabled) {
#ifdef HAVE_KWAYLAND
        if (QGuiApplication::platformName().startsWith("wayland")) {
            auto *registry = new Registry(this);
            d->registry = registry;
            auto *connection = ConnectionThread::fromApplication(registry);
            if (!connection) { delete registry; d->status = tr("Нет подключения Wayland"); emit changed(); return; }
            d->status = tr("Ожидание Plasma window management");
            connect(registry,&Registry::interfaceAnnounced,this,[this](const QByteArray &name,quint32,quint32){
                if(name=="zkde_screencast_unstable_v1") {d->thumbnails=true;emit changed();}
            });
            connect(registry, &Registry::plasmaWindowManagementAnnounced, this, [this, registry](quint32 name, quint32 version) {
                auto *manager = registry->createPlasmaWindowManagement(name, version, registry);
                d->available = manager && manager->isValid();
                d->status = d->available ? tr("KWin: отслеживание окон подключено") : tr("Протокол управления окнами недоступен");
                if (!manager) { emit changed(); return; }
                connect(manager, &PlasmaWindowManagement::windowCreated, this, [this](PlasmaWindow *window) {
                    // Keep initially hidden/skip-taskbar windows subscribed: their
                    // first protocol snapshot can change before the app maps.
                    if (window->pid() == QCoreApplication::applicationPid()) return;
                    const int token = d->nextToken++;
                    d->windows.insert(token, window);
                    const auto update = [this] { emit changed(); };
                    connect(window, &PlasmaWindow::appIdChanged, this, update);
                    connect(window, &PlasmaWindow::titleChanged, this, update);
                    connect(window, &PlasmaWindow::activeChanged, this, update);
                    connect(window, &PlasmaWindow::minimizedChanged, this, update);
                    connect(window, &PlasmaWindow::maximizedChanged, this, update);
                    connect(window, &PlasmaWindow::fullscreenChanged, this, update);
                    connect(window, &PlasmaWindow::geometryChanged, this, update);
                    connect(window, &PlasmaWindow::skipTaskbarChanged, this, update);
                    connect(window, &PlasmaWindow::closeableChanged, this, update);
                    connect(window, &PlasmaWindow::minimizeableChanged, this, update);
                    connect(window, &PlasmaWindow::unmapped, this, [this, token] {
                        d->iconGeometries.remove(token);
                        d->windows.remove(token); emit changed();
                    });
                    emit changed();
                });
                connect(manager, &PlasmaWindowManagement::removed, this, [this] {
                    d->available = false; d->windows.clear();
                    d->status = tr("KWin отключил протокол окон"); emit changed();
                });
                emit changed();
            });
            connect(registry, &Registry::interfacesAnnounced, this, [this] {
                if (!d->available) { d->status = tr("Композитор не объявляет управление окнами"); emit changed(); }
            });
            registry->create(connection);
            registry->setup();
        } else d->status = tr("Управление окнами доступно только в Wayland");
#else
        d->status = tr("Сборка без необязательной библиотеки KWayland");
#endif
    }
    emit changed();
}

QVariantList WindowTracker::windowsFor(const QString &desktopId) const
{
    QVariantList result;
#ifdef HAVE_KWAYLAND
    for (auto it = d->windows.cbegin(); it != d->windows.cend(); ++it) {
        const auto w = it.value();
        if (!w || w->skipTaskbar() || d->desktopId(w->appId()) != desktopId) continue;
        result.append(QVariantMap{{"token", it.key()}, {"uuid",QString::fromUtf8(w->uuid())}, {"title", w->title()}, {"active", w->isActive()},
            {"minimized", w->isMinimized()}, {"canMinimize", w->isMinimizeable()}, {"canClose", w->isCloseable()}});
    }
#else
    Q_UNUSED(desktopId)
#endif
    std::sort(result.begin(),result.end(),[](const QVariant &a,const QVariant &b){return a.toMap()["token"].toInt()<b.toMap()["token"].toInt();});
    return result;
}

QVariantList WindowTracker::applications() const
{
    QVariantList result;
#ifdef HAVE_KWAYLAND
    QHash<QString, QVariantMap> groups;
    for (const auto &w : d->windows) {
        if (!w || w->skipTaskbar() || w->appId().isEmpty()) continue;
        const QString id = d->desktopId(w->appId());
        auto &group = groups[id];
        group["id"] = id; group["title"] = w->title();
        group["count"] = group.value("count").toInt() + 1;
        group["active"] = group.value("active").toBool() || w->isActive();
    }
    auto ids=groups.keys();
    std::sort(ids.begin(),ids.end());
    for (const auto &id : ids) result.append(groups.value(id));
#endif
    return result;
}

bool WindowTracker::activateApp(const QString &id)
{
    const auto windows = windowsFor(id);
    if (windows.isEmpty()) return false;
    int choice = 0;
    for (int i = 0; i < windows.size(); ++i) {
        const auto w=windows[i].toMap();
        if(w.value("active").toBool() && !w.value("minimized").toBool()) {
            if(w.value("canMinimize").toBool()) perform(w.value("token").toInt(),"minimize");
            return true; // Never launch another instance when an active window cannot minimize.
        }
        if(!w.value("minimized").toBool()) choice=i;
    }
    return perform(windows.at(choice).toMap().value("token").toInt(), QStringLiteral("activate"));
}

void WindowTracker::setIconGeometries(QWindow *panel,const QVariantList &targets) {
#ifdef HAVE_KWAYLAND
    auto *surface=Surface::fromWindow(panel);
    if(!d->available || !surface)return;
    QHash<QString,QRect> geometry;
    for(const auto &value:targets) {
        const auto target=value.toMap();geometry.insert(target["id"].toString(),target["rect"].toRectF().toAlignedRect());
    }
    for(auto it=d->windows.cbegin();it!=d->windows.cend();++it) {
        if(!it.value())continue;
        const QRect rect=geometry.value(d->desktopId(it.value()->appId()));
        if(rect.isEmpty()) {
            if(d->iconGeometries.remove(it.key()))it.value()->unsetMinimizedGeometry(surface);
        } else if(d->iconGeometries.value(it.key())!=rect) {
            it.value()->setMinimizedGeometry(surface,rect);
            d->iconGeometries[it.key()]=rect;
        }
    }
#else
    Q_UNUSED(panel) Q_UNUSED(targets)
#endif
}

bool WindowTracker::perform(int token, const QString &action)
{
#ifdef HAVE_KWAYLAND
    const auto w = d->windows.value(token);
    if (!d->enabled || !w || w->skipTaskbar()) return false;
    if (action == "activate") {
        if (w->isMinimized() && w->isMinimizeable()) w->requestToggleMinimized();
        w->requestActivate();
    } else if (action == "minimize" && w->isMinimizeable() && !w->isMinimized()) w->requestToggleMinimized();
    else if (action == "maximize" && w->isMaximizeable()) w->requestToggleMaximized();
    else if (action == "close" && w->isCloseable()) w->requestClose();
    else return false;
    return true;
#else
    Q_UNUSED(token) Q_UNUSED(action)
    return false;
#endif
}
