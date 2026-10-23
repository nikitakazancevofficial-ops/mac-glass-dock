#pragma once
#include <QObject>
#include <QVariantList>
#include <QRectF>
#include <QtQml/qqmlregistration.h>
#include <memory>
class QWindow;

class WindowTracker : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(bool activeWindowExpanded READ activeWindowExpanded NOTIFY changed)
    Q_PROPERTY(bool thumbnailAvailable READ thumbnailAvailable NOTIFY changed)
public:
    explicit WindowTracker(QObject *parent = nullptr);
    ~WindowTracker() override;
    bool enabled() const;
    bool available() const;
    QString status() const;
    bool activeWindowExpanded() const;
    bool thumbnailAvailable() const;
    Q_INVOKABLE bool expandedOn(const QRectF &screen) const;
    void setEnabled(bool enabled);
    QVariantList applications() const;
    Q_INVOKABLE QVariantList windowsFor(const QString &desktopId) const;
    Q_INVOKABLE bool activateApp(const QString &desktopId);
    Q_INVOKABLE bool perform(int token, const QString &action);
    void setIconGeometries(QWindow *panel,const QVariantList &targets);
signals:
    void changed();
private:
    struct Private;
    std::unique_ptr<Private> d;
};
