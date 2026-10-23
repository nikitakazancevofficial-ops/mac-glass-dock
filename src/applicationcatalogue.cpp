#include "applicationcatalogue.h"
#include "applicationmodel.h"
#include "docksettings.h"
#include <KSycoca>
#include <QCollator>
#include <algorithm>

ApplicationCatalogue::ApplicationCatalogue(DockSettings *settings, ApplicationModel *dock, QObject *parent)
    : QAbstractListModel(parent), m_settings(settings), m_dock(dock)
{
    connect(KSycoca::self(), &KSycoca::databaseChanged, this, &ApplicationCatalogue::refresh);
    connect(settings, &DockSettings::pinsChanged, this, [this] {
        if (m_category == "Избранное") filter();
        else if (!m_filtered.isEmpty()) emit dataChanged(index(0), index(m_filtered.size()-1), {Pinned});
    });
    refresh();
}
QStringList ApplicationCatalogue::categories() const { return {"Все", "Избранное", "Интернет", "Работа", "Медиа", "Разработка", "Игры", "Система", "Другое"}; }
QString ApplicationCatalogue::categoryOf(const KService::Ptr &s) const
{
    const auto c = s->categories();
    if (c.contains("Network")) return "Интернет";
    if (c.contains("Development")) return "Разработка";
    if (c.contains("AudioVideo") || c.contains("Graphics") || c.contains("Audio")) return "Медиа";
    if (c.contains("Office")) return "Работа";
    if (c.contains("Game")) return "Игры";
    if (c.contains("System") || c.contains("Settings") || c.contains("Utility")) return "Система";
    return "Другое";
}
void ApplicationCatalogue::refresh()
{
    m_all.clear();
    QSet<QString> seen;
    for (const auto &s : KService::allServices()) {
        if (!s->isValid() || !s->isApplication() || s->noDisplay() || !s->showInCurrentDesktop() || seen.contains(s->storageId())) continue;
        seen.insert(s->storageId()); m_all.append(s);
    }
    QCollator collator;
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(m_all.begin(), m_all.end(), [&collator](const auto &a, const auto &b) { return collator.compare(a->name(), b->name()) < 0; });
    filter();
}
int ApplicationCatalogue::rowCount(const QModelIndex &p) const { return p.isValid() ? 0 : m_filtered.size(); }
QVariant ApplicationCatalogue::data(const QModelIndex &i, int role) const
{
    if (!i.isValid() || i.row() < 0 || i.row() >= m_filtered.size()) return {};
    const auto &s = m_filtered[i.row()];
    switch(role) {
    case Name: return s->name();
    case Icon: return s->icon();
    case DesktopId: return s->storageId();
    case Pinned: return m_settings->pinnedApps.contains(s->storageId());
    case GenericName: return s->genericName();
    default: return {};
    }
}
QHash<int,QByteArray> ApplicationCatalogue::roleNames() const { return {{Name,"appName"},{Icon,"iconName"},{DesktopId,"desktopId"},{Pinned,"pinned"},{GenericName,"genericName"}}; }
void ApplicationCatalogue::setQuery(const QString &query) { if (query != m_query) { m_query = query; filter(); } }
void ApplicationCatalogue::setCategory(const QString &category) { if (categories().contains(category) && category != m_category) { m_category = category; filter(); } }
void ApplicationCatalogue::filter()
{
    beginResetModel();
    m_filtered.clear();
    const QString q = m_query.trimmed();
    for (const auto &s : m_all) {
        if (m_category == "Избранное" && !m_settings->pinnedApps.contains(s->storageId())) continue;
        if (m_category != "Все" && m_category != "Избранное" && categoryOf(s) != m_category) continue;
        const QString search = s->name() + " " + s->genericName() + " " + s->keywords().join(' ') + " " + s->storageId();
        if (!q.isEmpty() && !search.contains(q, Qt::CaseInsensitive)) continue;
        m_filtered.append(s);
    }
    endResetModel();
    emit filterChanged();
}
void ApplicationCatalogue::launch(const QString &id) { m_dock->launchDesktop(id); emit launched(); }
void ApplicationCatalogue::togglePin(const QString &id) { m_dock->togglePin(id); }
