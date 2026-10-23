#include <QtTest>
#include "geniegeometry.h"

class GenieTests:public QObject {
    Q_OBJECT
private slots:
    void endpointsAndFunnel() {
        const QRectF frame(100,70,900,650),icon(1150,950,56,56);
        for(double y:{0.,.25,.5,.75,1.})for(double x:{0.,.25,.5,.75,1.}) {
            const QPointF p(x*frame.width(),y*frame.height());
            QCOMPARE(MacGlassGenie::map(p,frame,icon,0),frame.topLeft()+p);
            QCOMPARE(MacGlassGenie::map(p,frame,icon,1),icon.topLeft()+QPointF(x*icon.width(),y*icon.height()));
        }
        const auto topLeft=MacGlassGenie::map({},frame,icon,.46);
        const auto topRight=MacGlassGenie::map({frame.width(),0},frame,icon,.46);
        const auto bottomLeft=MacGlassGenie::map({0,frame.height()},frame,icon,.46);
        const auto bottomRight=MacGlassGenie::map({frame.width(),frame.height()},frame,icon,.46);
        QVERIFY(topRight.x()-topLeft.x()>frame.width()*.8);
        QVERIFY(bottomRight.x()-bottomLeft.x()<icon.width()*1.1);
        QVERIFY(bottomLeft.y()>frame.bottom());
        QCOMPARE(MacGlassGenie::opacity(.2),1.);
        QCOMPARE(MacGlassGenie::opacity(0),0.);
    }
    void continuousUnfoldedMesh() {
        const QRectF frame(100,50,900,650);
        for(const auto &icon:{QRectF(40,900,56,56),QRectF(540,900,56,56),QRectF(1250,900,56,56),QRectF(400,620,56,56)}) {
            for(int step=0;step<=200;++step) {
                const double t=step/200.;
                for(int y=0;y<32;++y)for(int x=0;x<16;++x) {
                    const QPointF p(x*frame.width()/16,y*frame.height()/32);
                    const auto a=MacGlassGenie::map(p,frame,icon,t);
                    const auto b=MacGlassGenie::map(p+QPointF(frame.width()/16,0),frame,icon,t);
                    const auto c=MacGlassGenie::map(p+QPointF(0,frame.height()/32),frame,icon,t);
                    QVERIFY(std::isfinite(a.x()) && std::isfinite(a.y()));
                    const QPointF horizontal=b-a,vertical=c-a;
                    QVERIFY2(horizontal.x()*vertical.y()-horizontal.y()*vertical.x()>=-1e-7,"Genie mesh folded over itself");
                }
            }
        }
    }
};
QTEST_APPLESS_MAIN(GenieTests)
#include "genietests.moc"
