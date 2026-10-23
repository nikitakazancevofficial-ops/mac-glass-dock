#pragma once
#include <QAbstractListModel>
#include <KService>
#include <QtQml/qqmlregistration.h>
class DockSettings;
class ApplicationModel;

class ApplicationCatalogue : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY filterChanged)
    Q_PROPERTY(QString category READ category WRITE setCategory NOTIFY filterChanged)
    Q_PROPERTY(int count READ count NOTIFY filterChanged)
    Q_PROPERTY(QStringList categories READ categories CONSTANT)
public:
    enum Role { Name = Qt::UserRole + 1, Icon, DesktopId, Pinned, GenericName };
    ApplicationCatalogue(DockSettings *settings, ApplicationModel *dock, QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    int count() const { return m_filtered.size(); }
    QString query() const { return m_query; }
    QString category() const { return m_category; }
    QStringList categories() const;
    void setQuery(const QString &query);
    void setCategory(const QString &category);
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void launch(const QString &id);
    Q_INVOKABLE void togglePin(const QString &id);
signals:
    void filterChanged();
    void launched();
private:
    void filter();
    QString categoryOf(const KService::Ptr &service) const;
    DockSettings *m_settings;
    ApplicationModel *m_dock;
    KService::List m_all, m_filtered;
    QString m_query, m_category = QStringLiteral("Все");
};
