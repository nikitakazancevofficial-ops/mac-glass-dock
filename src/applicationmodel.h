#pragma once
#include <QAbstractListModel>
#include <QQuickImageProvider>
#include <KService>
#include <QtQml/qqmlregistration.h>
#include <QSet>
class DockSettings;
class WindowTracker;

class ApplicationModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY runtimeError)
public:
    enum Role { NameRole = Qt::UserRole + 1, IconRole, AvailableRole, LaunchingRole, ErrorRole,
                DesktopIdRole, RunningRole, ActiveRole, PinnedRole };
    explicit ApplicationModel(const QStringList &ids, QObject *parent = nullptr);
    void attach(DockSettings *settings, WindowTracker *windows);
    int rowCount(const QModelIndex &parent = {}) const override;
    int count() const { return m_apps.size(); }
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    QString lastError() const { return m_lastError; }
    Q_INVOKABLE void launch(int row);
    Q_INVOKABLE void activate(int row);
    Q_INVOKABLE void launchDesktop(const QString &id);
    Q_INVOKABLE void togglePin(const QString &id);
    Q_INVOKABLE QString desktopId(int row) const;
signals:
    void launchFinished(int row, bool success, const QString &error);
    void countChanged();
    void runtimeError();
private:
    void rebuild();
    int rowFor(const QString &id) const;
    struct App { QString id; KService::Ptr service; QString title; int running = 0; bool active = false; bool pinned = true; };
    QList<App> m_apps;
    QStringList m_requested;
    DockSettings *m_settings = nullptr;
    WindowTracker *m_windows = nullptr;
    QSet<QString> m_jobs;
    QHash<QString, QString> m_errors;
    QString m_lastError;
};

class ThemeIconProvider : public QQuickImageProvider
{
public:
    ThemeIconProvider() : QQuickImageProvider(QQuickImageProvider::Image) {}
    QImage requestImage(const QString &id, QSize *size, const QSize &requestedSize) override;
};
