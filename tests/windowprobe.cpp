#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickPaintedItem>
#include <QPainter>
#include <QTimer>
class ProbeContent:public QQuickPaintedItem {
    QTimer timer;
    bool phase=false;
public:
    explicit ProbeContent(QQuickItem *parent):QQuickPaintedItem(parent) {
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_WINDOW_THUMBNAILS")) {
            QObject::connect(&timer,&QTimer::timeout,this,[this]{phase=!phase;update();});timer.start(180);
        }
    }
    void paint(QPainter *p) override {
        p->fillRect(boundingRect(),QColor("#202832"));
        if(qEnvironmentVariableIsSet("MACGLASS_TEST_WINDOW_THUMBNAILS"))
            p->fillRect(0,0,width(),36,phase?QColor("#b3476d"):QColor("#287d74"));
        p->setPen(QColor("#e3ebf7"));p->setFont(QFont("Sans Serif",13));
        p->drawText(18,28,"DOCUMENTS                       TYPE       DATE");
        p->setFont(QFont("Sans Serif",11));
        for(int row=0;row<10;++row) {
            const int y=42+row*28;
            if(row%2==0)p->fillRect(0,y,width(),28,QColor("#293440"));
            p->fillRect(16,y+8,14,11,QColor("#69b8e1"));p->setPen(QColor("#d7e3ee"));
            p->drawText(42,y+19,QString("Project %1       Folder       Today").arg(row+1));
        }
    }
};
int main(int argc,char **argv) {
    QGuiApplication app(argc,argv);
    app.setApplicationName("Mac Glass Dock Window Probe");
    app.setDesktopFileName("mac-glass-dock-probe");
    QQuickWindow window;
    window.setTitle("Mac Glass Dock — isolated window test");
    window.setColor(QColor("#31506b"));
    window.resize(320,180);
    if(qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE")) {
        auto *content=new ProbeContent(window.contentItem());
        content->setWidth(window.width());content->setHeight(window.height());
        QObject::connect(&window,&QWindow::widthChanged,content,[&window,content]{content->setWidth(window.width());});
        QObject::connect(&window,&QWindow::heightChanged,content,[&window,content]{content->setHeight(window.height());});
    }
    if(app.arguments().contains("--fullscreen")) window.showFullScreen();
    else window.show();
    return app.exec();
}
