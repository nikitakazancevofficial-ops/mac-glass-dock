#include "framediagnostics.h"
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QTimer>
#include <QTextStream>
#include <QPointer>
FrameDiagnostics::FrameDiagnostics(QQuickWindow *window, QObject *parent) : QObject(parent)
{
    clock.start();
    connect(window,&QQuickWindow::frameSwapped,this,[this]{
        const qint64 now=clock.nsecsElapsed();
        const qint64 delta=now-previous;
        if(previous && delta<250000000) {++intervals; total+=delta; maximum=qMax(maximum,delta);}
        previous=now; ++frames;
    });
    auto *timer=new QTimer(this);
    timer->setInterval(5000);
    connect(timer,&QTimer::timeout,this,[this,guard=QPointer<QQuickWindow>(window)]{
        if(!guard) return;
        QTextStream out(stdout);
        out<<"Window "<<guard->objectName()<<" frames/5s="<<frames<<" active-interval avg(ms)="
           <<(intervals?double(total)/intervals/1000000:0)
           <<" max(ms)="<<double(maximum)/1000000
           <<" graphicsApi="<<int(guard->rendererInterface()->graphicsApi())
           <<" dpr="<<guard->devicePixelRatio()<<Qt::endl;
        frames=0; intervals=0; total=0; maximum=0;
    });
    timer->start();
}
