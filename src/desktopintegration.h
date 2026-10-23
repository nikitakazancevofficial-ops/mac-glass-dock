#pragma once
#include <QObject>
#include <QProcess>
#include <QtQml/qqmlregistration.h>

class DesktopIntegration : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Owned by DockPlatform")
    Q_PROPERTY(bool active READ active NOTIFY changed)
    Q_PROPERTY(bool decorated READ decorated NOTIFY changed)
    Q_PROPERTY(bool autostart READ autostart NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY changed)
public:
    explicit DesktopIntegration(QObject *parent=nullptr);
    bool active() const {return m_active;}
    bool decorated() const {return m_decorated;}
    bool autostart() const {return m_autostart;}
    bool busy() const {return m_process.state()!=QProcess::NotRunning;}
    QString error() const {return m_error;}
    Q_INVOKABLE void apply(const QString &action);
signals:
    void changed();
    void activated();
    void restored();
private:
    QProcess m_process;
    bool m_active=false,m_decorated=false,m_autostart=false;
    QString m_error,m_action;
};
