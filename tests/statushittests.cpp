#include "statushitgeometry.h"
#include "dockedgegeometry.h"
#include <QtTest>

class StatusHitTests : public QObject {
    Q_OBJECT
private slots:
    void dockOnlyRevealsBelowItsOwnWidth() {
        for(const QRectF &screen:{QRectF(0,0,1920,1080),QRectF(-1920,-1080,1920,1080)}) {
            const QRectF dock(screen.center().x()-380,screen.bottom()-2,760,2);
            for(double x:{dock.left(),dock.center().x(),dock.right()-1}) {
                QVERIFY(MacGlassDockEdge::contains(screen,dock,{x,screen.bottom()-1}));
                QVERIFY(MacGlassDockEdge::contains(screen,dock,{x,screen.bottom()-2}));
                QVERIFY(!MacGlassDockEdge::contains(screen,dock,{x,screen.bottom()-3}));
                QVERIFY(!MacGlassDockEdge::contains(screen,dock,{x,screen.bottom()-36}));
            }
            for(double x:{screen.left(),dock.left()-1,dock.right(),screen.right()-1})
                QVERIFY(!MacGlassDockEdge::contains(screen,dock,{x,screen.bottom()-1}));
            QVERIFY(!MacGlassDockEdge::contains(screen,{},screen.center()));
            // A wider/moved Dock must update the horizontal trigger, while its
            // vertical position stays tied to the physical screen bottom.
            const QRectF moved=dock.adjusted(-40,-180,40,0);
            QVERIFY(MacGlassDockEdge::contains(screen,moved,{moved.left()+1,screen.bottom()-1}));
            QVERIFY(!MacGlassDockEdge::contains(screen,moved,{moved.left()+1,moved.top()}));
        }
    }

    void clickOnVisibleRowIsLeftToTheClient() {
        const QRectF screen(0,0,1920,1080);
        const QRectF row(1621,9,294,28);
        QVERIFY(MacGlassStatusHit::contains(row,screen,{1800,20}));
        QVERIFY(MacGlassStatusHit::isVisibleStatusRowClick(row,{1800,20}));
        QVERIFY(!MacGlassStatusHit::compositorOwnsClick(row,screen,{1800,20}));
    }
    void clickOnTransparentRightEdgeExtensionIsRouted() {
        QVERIFY(MacGlassStatusHit::compositorOwnsClick(QRectF(1621,9,294,28),
                                                        QRectF(0,0,1920,1080),{1918,20}));
    }
    void unrelatedPointsAreRejected() {
        const QRectF screen(0,0,1920,1080);
        const QRectF row(1621,9,294,28);
        QVERIFY(!MacGlassStatusHit::compositorOwnsClick(row,screen,{1600,20}));
        QVERIFY(!MacGlassStatusHit::compositorOwnsClick(row,screen,{1800,40}));
        QVERIFY(!MacGlassStatusHit::compositorOwnsClick({},screen,{1800,20}));
    }
};

QTEST_MAIN(StatusHitTests)
#include "statushittests.moc"
