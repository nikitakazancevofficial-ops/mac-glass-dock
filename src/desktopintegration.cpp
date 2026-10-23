#include "desktopintegration.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

DesktopIntegration::DesktopIntegration(QObject *parent):QObject(parent) {
    connect(&m_process,&QProcess::errorOccurred,this,[this]{m_error=m_process.errorString();emit changed();});
    connect(&m_process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        const auto result=QJsonDocument::fromJson(m_process.readAllStandardOutput()).object();
        if(result.contains("active")) {m_active=result["active"].toBool();m_decorated=result["decorated"].toBool();}
        if(result.contains("autostart")) m_autostart=result["autostart"].toBool();
        if(code || status!=QProcess::NormalExit) m_error=QString::fromUtf8(m_process.readAllStandardError()).trimmed();
        else {
            if(m_action=="activate") emit activated();
            if(m_action=="restore") emit restored();
        }
        emit changed();
    });
    QTimer::singleShot(0,this,[this]{apply("status");});
}
void DesktopIntegration::apply(const QString &action) {
    if(busy() || !QStringList{"status","activate","restore","decorate","restore-decoration","enable-autostart","disable-autostart"}.contains(action))return;
    m_action=action;m_error.clear();
    m_process.start("python3",{QStringLiteral(DOCK_PROJECT_DIR "/scripts/desktop-session.py"),action});
    emit changed();
}
