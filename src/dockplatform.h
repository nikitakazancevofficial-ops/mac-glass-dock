#pragma once
#include <QObject>
#include <QCoreApplication>
#include <QRectF>
#include <QVariantList>
#include <QPointer>
#include <QHash>
#include <QQuickWindow>
#include <QTimer>
#include "desktopintegration.h"
class WindowTracker;
#include <QtQml/qqmlregistration.h>
class QQuickWindow;
class DockSettings;
class DockPlatform : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.macglass.Dock")
    QML_ELEMENT
    QML_UNCREATABLE("Created by the application")
    Q_PROPERTY(bool blurAvailable READ blurAvailable NOTIFY effectsChanged)
    Q_PROPERTY(bool refractionAvailable READ refractionAvailable NOTIFY effectsChanged)
    Q_PROPERTY(bool nativePanelGenie READ nativePanelGenie NOTIFY effectsChanged)
    Q_PROPERTY(QStringList screenNames READ screenNames NOTIFY screensChanged)
    Q_PROPERTY(bool edgeHovered READ edgeHovered NOTIFY edgeHoverChanged)
    Q_PROPERTY(bool controlCenterEdgeHovered READ controlCenterEdgeHovered NOTIFY controlCenterEdgeHoverChanged)
    Q_PROPERTY(DesktopIntegration *desktop READ desktop CONSTANT)
    Q_PROPERTY(bool preview READ preview NOTIFY effectsChanged)
    Q_PROPERTY(bool obstructed READ obstructed NOTIFY obstructionChanged)
    Q_PROPERTY(bool statusObstructed READ statusObstructed NOTIFY statusObstructionChanged)
public:
    explicit DockPlatform(QObject *parent = nullptr);
    void setSettings(DockSettings *settings);
    void setWindowTracker(WindowTracker *windows);
    void configure(QQuickWindow *window, bool preview);
    bool blurAvailable() const { return m_blurAvailable; }
    bool refractionAvailable() const { return m_refractionAvailable && !m_preview; }
    bool nativePanelGenie() const { return m_nativePanelGenie && refractionAvailable(); }
    bool ownsBus() const { return m_busOwner; }
    bool edgeHovered() const {return m_edgeHovered;}
    bool controlCenterEdgeHovered() const {return m_controlCenterEdgeHovered;}
    DesktopIntegration *desktop() {return &m_desktop;}
    bool preview() const {return m_preview;}
    bool obstructed() const {return m_obstructed;}
    bool statusObstructed() const {return m_statusObstructed;}
    QStringList screenNames() const;
    Q_INVOKABLE void updateRegion(const QRectF &surface, qreal radius, const QVariantList &icons);
    Q_INVOKABLE void preparePanel(QQuickWindow *window, const QString &kind);
    Q_INVOKABLE void hidePanelAfterFrame(QQuickWindow *window,int generation);
    Q_INVOKABLE QPointF panelAnchor(QQuickWindow *panel, const QPointF &dockPoint) const;
    Q_INVOKABLE void updatePanelRegion(QQuickWindow *window, const QRectF &surface, qreal radius);
    Q_INVOKABLE void updateStatusRegion(const QRectF &surface);
    Q_INVOKABLE void setPanelKeyboardFocus(QQuickWindow *window, bool focused);
    Q_INVOKABLE QRectF geniePanelBounds(const QRectF &frame, const QRectF &icon, qreal openness, qreal radius) const;
    Q_INVOKABLE void updateGeniePanelRegion(QQuickWindow *window, const QRectF &frame, const QRectF &icon, qreal openness, qreal radius);
    Q_INVOKABLE void setHidden(bool hidden);
    Q_INVOKABLE void updateTargets(const QVariantList &targets, const QRectF &surface);
public slots:
    Q_SCRIPTABLE QString opticalState() const { return m_opticalState; }
    Q_SCRIPTABLE void openPanel(const QString &kind) { emit panelRequested(kind); }
    Q_SCRIPTABLE void quitDock() {QCoreApplication::quit();}
    void handleReveal() { emit revealRequested(); }
    void handlePointerOutside() { emit pointerOutsideDock(); }
    void handleEdgeHover(bool hovered);
    void handleControlCenterEdgeHover(bool hovered);
    void handleObstructed(bool obstructed);
    void handleStatusObstructed(bool obstructed);
    void handlePanelOutside(const QString &name) { emit panelOutsidePressed(name); }
    void handleControlCenterStatusPressed() { emit controlCenterStatusPressed(); }
    void handlePanelPrepared(const QString &name,uint surface);
signals:
    void panelRequested(const QString &kind);
    void panelOutsidePressed(const QString &name);
    void controlCenterStatusPressed();
    void panelMotionPrepared(const QString &name);
    Q_SCRIPTABLE void opticalStateChanged(const QString &state);
    void effectsChanged();
    void screensChanged();
    void revealRequested();
    void pointerOutsideDock();
    void edgeHoverChanged();
    void controlCenterEdgeHoverChanged(bool hovered);
    void obstructionChanged();
    void statusObstructionChanged();
private:
    DesktopIntegration m_desktop;
    void applyRegion();
    void applyPanel(QQuickWindow *window);
    void selectScreen();
    void publishOptics();
    void probeRefraction();
    void scheduleOptics();
    QTimer m_publishTimer, m_probeTimer;
    QString m_opticalState;
    bool m_refractionAvailable = false, m_probePending = false, m_busOwner = false;
    bool m_nativePanelGenie=false;
    QPointer<QQuickWindow> m_window;
    DockSettings *m_settings = nullptr;
    WindowTracker *m_windows = nullptr;
    QRectF m_surface;
    QVariantList m_icons;
    QVariantList m_targets;
    QHash<QString,QStringList> m_targetAliases;
    bool m_edgeHovered=false;
    bool m_controlCenterEdgeHovered=false;
    bool m_obstructed=false;
    bool m_statusObstructed=false;
    QRectF m_targetSurface;
    QRectF m_statusRegion;
    qreal m_radius = 30;
    bool m_blurAvailable = false, m_preview = false, m_hidden = false;
    struct Panel { QRectF rect; qreal radius = 30; QRegion shape; };
    QHash<QQuickWindow *, Panel> m_panels;
};
