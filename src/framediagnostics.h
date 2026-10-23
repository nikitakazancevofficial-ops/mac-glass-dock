#pragma once
#include <QObject>
#include <QElapsedTimer>
class QQuickWindow;
class FrameDiagnostics : public QObject
{
    Q_OBJECT
public:
    explicit FrameDiagnostics(QQuickWindow *window, QObject *parent = nullptr);
private:
    QElapsedTimer clock;
    qint64 previous = 0, frames = 0, intervals = 0, total = 0, maximum = 0;
};
