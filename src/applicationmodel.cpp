#include "applicationmodel.h"
#include "docksettings.h"
#include "windowtracker.h"
#include <KIO/ApplicationLauncherJob>
#include <KSycoca>
#include <QIcon>
#include <QUrl>
#include <QFileInfo>
#include <QImage>
#include <QDebug>
#include <QTimer>
#include <QPainter>
namespace {
KService::Ptr serviceFor(const QString &id) {
    if (id.contains('/') || !id.endsWith(".desktop")) return {};
    auto s = KService::serviceByStorageId(id);
    return s && s->isValid() && s->isApplication() ? s : KService::Ptr();
}
}
ApplicationModel::ApplicationModel(const QStringList &ids, QObject *parent)
    : QAbstractListModel(parent), m_requested(ids)
{
    rebuild();
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, &ApplicationModel::rebuild);
}
void ApplicationModel::attach(DockSettings *settings, WindowTracker *windows)
{
    m_settings = settings; m_windows = windows;
    connect(settings, &DockSettings::pinsChanged, this, &ApplicationModel::rebuild);
    // Coalesce bursts of per-window state changes into a single model refresh.
    auto *timer = new QTimer(this);
    timer->setSingleShot(true); timer->setInterval(0);
    connect(windows, &WindowTracker::changed, timer, [timer]{if(!timer->isActive())timer->start();});
    connect(timer, &QTimer::timeout, this, &ApplicationModel::rebuild);
    rebuild();
}
void ApplicationModel::rebuild()
{
    QList<App> next;
    const auto rowIn = [&next](const QString &id) {
        for(int i=0;i<next.size();++i) if(next[i].id==id) return i;
        return -1;
    };
    const QStringList ids = m_settings ? m_settings->pinnedApps : m_requested;
    for (const auto &id : ids) {
        if (rowIn(id) >= 0) continue;
        auto s = serviceFor(id);
        next.append({id, s, s ? s->name() : id, 0, false, true});
    }
    if (m_windows) for (const auto &value : m_windows->applications()) {
        const auto app = value.toMap();
        const QString id = app.value("id").toString();
        int row = rowIn(id);
        if (row < 0) {
            auto s = serviceFor(id);
            next.append({id, s, s ? s->name() : app.value("title").toString(), 0, false, false});
            row = next.size()-1;
        }
        next[row].running = app.value("count").toInt();
        next[row].active = app.value("active").toBool();
    }
    // Preserve existing delegates and their spring state while a new app maps.
    // A full model reset made every icon jump and recreated its glass surface.
    const int previousCount=m_apps.size();
    for(int i=m_apps.size()-1;i>=0;--i) {
        if(rowIn(m_apps[i].id)>=0) continue;
        beginRemoveRows({},i,i); m_apps.removeAt(i); endRemoveRows();
    }
    for(int i=0;i<next.size();++i) {
        const int current=rowFor(next[i].id);
        if(current<0) {beginInsertRows({},i,i);m_apps.insert(i,next[i]);endInsertRows();}
        else if(current!=i) {
            beginMoveRows({},current,current,{},current<i?i+1:i);
            m_apps.move(current,i);endMoveRows();
        }
        m_apps[i]=next[i];
        emit dataChanged(index(i),index(i));
    }
    if(previousCount!=m_apps.size()) emit countChanged();
}
int ApplicationModel::rowFor(const QString &id) const { for (int i=0;i<m_apps.size();++i) if (m_apps[i].id==id) return i; return -1; }
QString ApplicationModel::desktopId(int row) const { return row>=0 && row<m_apps.size() ? m_apps[row].id : QString(); }
int ApplicationModel::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : m_apps.size(); }
QVariant ApplicationModel::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row()<0 || i.row()>=m_apps.size()) return {};
    const auto &a=m_apps[i.row()];
    switch(role) {
    case NameRole: return a.title;
    case IconRole: return a.service ? a.service->icon() : QStringLiteral("application-x-executable");
    case AvailableRole: return bool(a.service) || a.running>0;
    case LaunchingRole: return m_jobs.contains(a.id);
    case ErrorRole: return m_errors.value(a.id, a.service || a.running ? QString() : tr("Приложение не установлено"));
    case DesktopIdRole: return a.id;
    case RunningRole: return a.running;
    case ActiveRole: return a.active;
    case PinnedRole: return a.pinned;
    default: return {};
    }
}
QHash<int,QByteArray> ApplicationModel::roleNames() const {
    return {{NameRole,"appName"},{IconRole,"iconName"},{AvailableRole,"available"},{LaunchingRole,"launching"},
        {ErrorRole,"launchError"},{DesktopIdRole,"desktopId"},{RunningRole,"runningCount"},{ActiveRole,"activeApp"},{PinnedRole,"pinned"}};
}
void ApplicationModel::launch(int row) { const auto id=desktopId(row); if(!id.isEmpty()) launchDesktop(id); }
void ApplicationModel::activate(int row)
{
    const auto id=desktopId(row);
    if (id.isEmpty()) return;
    if (m_windows && m_windows->activateApp(id)) return;
    launchDesktop(id);
}
void ApplicationModel::launchDesktop(const QString &id)
{
    const auto service=serviceFor(id);
    if (!service) {
        m_lastError=tr("Приложение недоступно: %1").arg(id);
        m_errors[id]=m_lastError;
        const int row=rowFor(id);
        if(row>=0) emit dataChanged(index(row),index(row),{ErrorRole});
        emit runtimeError(); emit launchFinished(row,false,m_lastError); return;
    }
    if(m_jobs.contains(id)) return;
    m_jobs.insert(id); m_errors.remove(id);
    const int row=rowFor(id);
    if(row>=0) emit dataChanged(index(row),index(row),{LaunchingRole,ErrorRole});
    auto *job=new KIO::ApplicationLauncherJob(service,this);
    job->setUiDelegate(nullptr);
    connect(job,&KJob::result,this,[this,id,job] {
        m_jobs.remove(id); m_errors[id]=job->error() ? job->errorString() : QString();
        const int current=rowFor(id);
        if(current>=0) emit dataChanged(index(current),index(current),{LaunchingRole,ErrorRole});
        if(job->error()) { m_lastError=job->errorString(); emit runtimeError(); }
        emit launchFinished(current,job->error()==0,m_errors[id]);
    });
    job->start();
}
void ApplicationModel::togglePin(const QString &id)
{
    if(!m_settings) return;
    QStringList pins=m_settings->pinnedApps;
    if(pins.contains(id)) pins.removeAll(id);
    else if(serviceFor(id) && pins.size()<24) pins.append(id);
    else return;
    m_settings->setPinnedApps(pins);
}
QImage ThemeIconProvider::requestImage(const QString &id,QSize *size,const QSize &requestedSize)
{
    QString name=QUrl::fromPercentEncoding(id.toUtf8());
    const bool dark=name.startsWith("dark/");
    const bool original=name.startsWith("original/");
    if(dark || original || name.startsWith("light/")) name=name.mid(name.indexOf('/')+1);
    QIcon icon=QFileInfo(name).isAbsolute() ? QIcon(name) : QIcon::fromTheme(name);
    if(icon.isNull()) icon=QIcon::fromTheme(QStringLiteral("application-x-executable"));
    const QSize target=requestedSize.isValid() ? requestedSize.boundedTo(QSize(512,512)) : QSize(128,128);
    QImage result=icon.pixmap(target).toImage();
    if(!original && name.endsWith("-symbolic") && !result.isNull()) {
        QPainter painter(&result);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(result.rect(),dark ? QColor("#edf6ff") : QColor("#172b42"));
    }
    if(size) *size=result.size();
    return result;
}
