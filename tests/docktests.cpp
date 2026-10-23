#include "docktests.h"
#include "applicationmodel.h"

#include <QFile>
#include <QFileInfo>
#include <QQuickItem>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QSignalSpy>
#include <QTest>

static QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    if (root->objectName() == name)
        return root;
    for (auto *child : root->childItems())
        if (auto *found = findItem(child, name))
            return found;
    return nullptr;
}

class DockTests : public QObject
{
    Q_OBJECT
public:
    DockTests(QQuickWindow *window, ApplicationModel *model) : window(window), model(model) {}
private slots:
    void contentAndResources()
    {
        QVERIFY(window);
        QCOMPARE(window->color().alpha(), 0);
        QVERIFY(model->count() > 0);
        for (int i = 0; i < model->count(); ++i) {
            auto *item = findItem(window->contentItem(), QStringLiteral("dockItem%1").arg(i));
            QVERIFY(item);
            auto *icon = findItem(item, QStringLiteral("appIcon"));
            QVERIFY(icon);
            QTRY_VERIFY2(icon->property("status").toInt() == 1,
                qPrintable(QStringLiteral("Icon %1: %2 (status %3)").arg(i)
                    .arg(icon->property("source").toUrl().toString()).arg(icon->property("status").toInt())));
        }
        for(const auto &name:{"glass.frag","adaptive-icon.frag","panel-genie.vert","panel-genie.frag"}) {
            QFile shader(QStringLiteral(":/effects/")+name+QStringLiteral(".qsb"));
            QVERIFY(shader.open(QIODevice::ReadOnly));
            QVERIFY(shader.size()>100);
        }
    }

    void continuousHoverAndLayout()
    {
        auto *dock = findItem(window->contentItem(), QStringLiteral("dock"));
        QVERIFY(dock);
        auto *center = findItem(dock, QStringLiteral("dockItem2"));
        auto *side = findItem(dock, QStringLiteral("dockItem0"));
        QVERIFY(center && side);
        const qreal restingWidth = window->width();
        const QPoint point = center->mapToScene(QPointF(center->width() / 2, center->height() / 2)).toPoint();
        QTest::mouseMove(window, point, 20);
        QTRY_VERIFY(dock->property("pointerInside").toBool());
        QTRY_VERIFY(center->property("zoom").toReal() > 1.35);
        QVERIFY(center->property("zoom").toReal() > side->property("zoom").toReal());
        QTRY_VERIFY(window->width() > restingWidth + 15);
        const auto arguments = QCoreApplication::arguments();
        const int capture = arguments.indexOf(QStringLiteral("--capture"));
        if (capture >= 0 && capture + 1 < arguments.size()) {
            QTest::qWait(160);
            const QFileInfo output(arguments.at(capture + 1));
            QVERIFY(window->grabWindow().save(output.path() + QLatin1Char('/') + output.completeBaseName() + QStringLiteral("-hover.png")));
        }
        for (int pass = 0; pass < 8; ++pass) {
            QTest::mouseMove(window, QPoint(25 + (pass * 43) % (window->width() - 45), point.y()), 25);
            for (int i = 1; i < model->count(); ++i) {
                auto *left = findItem(dock, QStringLiteral("dockItem%1").arg(i - 1));
                auto *right = findItem(dock, QStringLiteral("dockItem%1").arg(i));
                QVERIFY(left->x() + left->width() <= right->x() + 1);
            }
        }
        // Explicit leave event: portable even with offscreen's synthetic cursor.
        QEvent leave(QEvent::Leave);
        QCoreApplication::sendEvent(window, &leave);
        QTRY_VERIFY(!dock->property("pointerInside").toBool());
        QTRY_VERIFY(qAbs(center->property("zoom").toReal() - 1) < 0.01);
        for (const QString &name : QStringList{QStringLiteral("launcherButton"),
                QStringLiteral("dockItem0"), QStringLiteral("dockItem%1").arg(model->count()-1),
                QStringLiteral("settingsButton")}) {
            auto *edge = findItem(dock, name);
            QVERIFY(edge);
            const QPoint position = edge->mapToScene(QPointF(edge->width()/2,edge->height()/2)).toPoint();
            QTest::mouseMove(window, position, 25);
            qInfo() << "Edge hover:" << name;
            QTRY_VERIFY2(edge->property("zoom").toReal() > 1.3, qPrintable(name));
            QCoreApplication::sendEvent(window, &leave);
            QTRY_VERIFY(qAbs(edge->property("zoom").toReal()-1) < .01);
        }
        QTest::qWait(350);
        QSignalSpy idleFrames(window, &QQuickWindow::frameSwapped);
        QTest::qWait(250);
        qInfo() << "Idle frames in 250ms:" << idleFrames.count();
        QVERIFY2(idleFrames.count() < 8, "Dock is continuously rendering while idle");
    }

    void inputRegionAndRenderedFrame()
    {
        if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
            QVERIFY(!window->mask().isEmpty());
            QVERIFY(!window->mask().contains(QPoint(0, 0)));
            for (int i = 0; i < model->count(); ++i) {
                auto *item = findItem(window->contentItem(), QStringLiteral("dockItem%1").arg(i));
                QVERIFY(window->mask().contains(item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint()));
            }
        }
        const QImage frame = window->grabWindow();
        QVERIFY(!frame.isNull());
        QVERIFY(frame.pixelColor(0, 0).alpha() == 0);
        auto *shader = findItem(window->contentItem(), QStringLiteral("glassShader"));
        QVERIFY(shader);
        QVERIFY2(shader->property("status").toInt() != 2, qPrintable(shader->property("log").toString()));
        QCOMPARE(shader->property("fragmentShader").toUrl(), QUrl(QStringLiteral("qrc:/effects/glass.frag.qsb")));
        // Qt 6.12 can retain per-instance Uncompiled after a shared shader-info cache hit.
        // Require actual translucent output in addition to resource/error checks.
        auto *dock = findItem(window->contentItem(), QStringLiteral("dock"));
        QVERIFY(dock);
        const QRectF surface = dock->property("surfaceRect").toRectF();
        const QPointF sample = dock->mapToScene(QPointF(surface.center().x(), surface.bottom()-5));
        const qreal dpr = frame.width()/qreal(window->width());
        const int alpha = frame.pixelColor(qRound(sample.x()*dpr),qRound(sample.y()*dpr)).alpha();
        QVERIFY(alpha>0 && alpha<255);
        const auto args = QCoreApplication::arguments();
        const int index = args.indexOf(QStringLiteral("--capture"));
        if (index >= 0 && index + 1 < args.size())
            QVERIFY(frame.save(args.at(index + 1)));
    }

    void clickLaunchesDesktopEntry()
    {
        // The CTest configuration uses a harmless executable that only writes a build marker.
        QCOMPARE(model->data(model->index(0), ApplicationModel::NameRole).toString(), QStringLiteral("Dock Launch Probe"));
        QFile::remove(QStringLiteral(DOCK_TEST_MARKER));
        QSignalSpy result(model, &ApplicationModel::launchFinished);
        auto *first = findItem(window->contentItem(), QStringLiteral("dockItem0"));
        QVERIFY(first);
        const QPoint point = first->mapToScene(QPointF(first->width() / 2, first->height() / 2)).toPoint();
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, point);
        QTRY_VERIFY_WITH_TIMEOUT(!result.isEmpty(), 8000);
        QVERIFY2(result.at(0).at(1).toBool(), qPrintable(result.at(0).at(2).toString()));
        QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(QStringLiteral(DOCK_TEST_MARKER)), 8000);
        QVERIFY(!model->data(model->index(0), ApplicationModel::LaunchingRole).toBool());

        ApplicationModel missing({QStringLiteral("nonexistent-mac-glass-dock-test.desktop"), QStringLiteral("/tmp/invalid.desktop")});
        QSignalSpy failed(&missing, &ApplicationModel::launchFinished);
        missing.launch(0);
        missing.launch(1);
        QCOMPARE(failed.count(), 2);
        QVERIFY(!failed.at(0).at(1).toBool());
        QVERIFY(!failed.at(1).at(1).toBool());
    }

    void closesCleanly()
    {
        QVERIFY(window->close());
        QVERIFY(!window->isVisible());
    }
private:
    QQuickWindow *window;
    ApplicationModel *model;
};

int runDockTests(QQuickWindow *window, ApplicationModel *model)
{
    DockTests tests(window, model);
    return QTest::qExec(&tests, QStringList{QStringLiteral("dock-tests")});
}

#include "docktests.moc"
