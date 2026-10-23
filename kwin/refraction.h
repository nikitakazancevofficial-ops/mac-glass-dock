// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#pragma once
#include <effect/offscreeneffect.h>
#include <effect/effectwindow.h>
#include <effect/timeline.h>
#include <input_event_spy.h>
#include <QJsonObject>
#include <QHash>
#include <QSet>
#include <QElapsedTimer>
#include <QDBusServiceWatcher>
#include <memory>
#include <unordered_map>
#include <optional>

namespace KWin {
class GLTexture; class GLFramebuffer; class GLShader; class BackgroundEffectItem;
class MacGlassRefraction : public OffscreenEffect, public InputEventSpy {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface","org.macglass.Refraction")
public:
    MacGlassRefraction();
    ~MacGlassRefraction() override;
    static bool supported();
    bool isActive() const override;
    int requestedEffectChainPosition() const override { return 19; }
    bool blocksDirectScanout() const override { return isActive(); }
    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view,EffectWindow *w,WindowPrePaintData &data) override;
    void postPaintScreen() override;
    bool drawWindow(const RenderTarget &,const RenderViewport &,EffectWindow *,int,const Region &,WindowPaintData &) override;
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    bool paintScreen(const RenderTarget &,const RenderViewport &,int,const Region &,LogicalOutput *) override;
#endif
    void pointerButton(PointerButtonEvent *event) override;
    void touchDown(TouchDownEvent *event) override;
public slots:
    Q_SCRIPTABLE QString status() const;
    Q_SCRIPTABLE QString diagnostics() const;
    Q_SCRIPTABLE bool panelGenieSupported() const { return m_ready && bool(m_motionShader); }
    void updateState(const QString &json);
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    Q_SCRIPTABLE void testCaptureOwnedScene();
    Q_SCRIPTABLE void testPointer(double x,double y);
    Q_SCRIPTABLE void placeProbe(bool overlap);
    Q_SCRIPTABLE void testPanelPress(const QString &name,bool inside);
#endif
signals:
    Q_SCRIPTABLE void revealRequested();
    Q_SCRIPTABLE void pointerOutsideDock();
    Q_SCRIPTABLE void edgeHoverChanged(bool hovered);
    Q_SCRIPTABLE void obstructedChanged(bool obstructed);
    Q_SCRIPTABLE void statusObstructedChanged(bool obstructed);
    Q_SCRIPTABLE void panelOutsidePressed(const QString &name);
    Q_SCRIPTABLE void panelMotionReady(const QString &name,uint surface);
    Q_SCRIPTABLE void controlCenterEdgeHoverChanged(bool hovered);
    Q_SCRIPTABLE void controlCenterStatusPressed();
private:
    enum class Transition {Open,Close,Minimize,Restore};
    struct Motion {
        TimeLine time;
        bool closing=false;
        qreal from=0;
        int role=0;
        QRectF origin;
        QRectF repaintRect;
        EffectWindowDeletedRef deleted;
        std::optional<EffectWindowVisibleRef> visible;
        qreal openness() const {return closing ? from*(1-time.value()) : from+(1-from)*time.value();}
    };
    void beginMotion(EffectWindow *w,Transition transition);
    void observeWindow(EffectWindow *w);
    void minimizedChanged(EffectWindow *w);
    EffectWindow *dockWindow() const;
    void handlePointer(const QPointF &position,bool geometryChanged=false);
    void syncOcclusion();
    void syncPanelMotions();
    void repaintPanelMotion(EffectWindow *);
    bool applyPanelMotion(EffectWindow *,WindowPaintData &,WindowQuadList &);
    QRectF panelMotionBounds(EffectWindow *) const;
    void dismissOutside(const QPointF &position);
    bool controlCenterStatusHit(const QPointF &position) const;
    void refreshAnchors();
    QRectF originFor(EffectWindow *w);
    void finishMotion(EffectWindow *w,bool completed=false);
    void clearMotions();
    void apply(EffectWindow *,int,WindowPaintData &,WindowQuadList &) override;
    struct Buffer {
        std::unique_ptr<GLTexture> texture;
        std::unique_ptr<GLFramebuffer> fbo;
        qreal scale=1;
#ifdef MACGLASS_NESTED_TEST_CAPTURE
        QByteArray lastHash;
        quint64 changes=0;
#endif
    };
    struct WindowResources {
        std::unique_ptr<BackgroundEffectItem> background;
        std::unordered_map<RenderView *, Buffer> views;
    };
    QString key(EffectWindow *w) const;
    void requestState();
    void syncBackgrounds();
    bool renderLenses(const RenderTarget &,const RenderViewport &,EffectWindow *,const Region &,WindowPaintData &);
    std::unique_ptr<GLShader> m_shader;
    std::unique_ptr<GLShader> m_motionShader;
    std::unordered_map<EffectWindow *,std::unique_ptr<Motion>> m_motions;
    // No references/textures: ownership ends when KWin deletes or restores it.
    QSet<EffectWindow *> m_settledHidden;
    QSet<EffectWindow *> m_panelMotions;
    QSet<EffectWindow *> m_hiddenPanels;
    quint64 m_panelTailSuppressed=0,m_panelBackgroundRepaints=0;
    quint64 m_panelMotionFrames=0,m_warpedLenses=0;
    quint64 m_controlCenterRepaints=0;
    std::unordered_map<EffectWindow *,WindowResources> m_buffers;
    RenderView *m_currentView=nullptr;
    QJsonObject m_options,m_windows,m_dock;
    struct Anchor {QRectF rect;qint64 updated=0;};
    QHash<QString,Anchor> m_anchors;
    QElapsedTimer m_anchorClock;
    bool m_pointerWasInside=false,m_edgeHover=false,m_controlCenterEdgeHover=false;
    bool m_syntheticInput=false;
    bool m_obstructed=false;
    bool m_statusObstructed=false;
    bool m_showingDesktop=false;
    QDBusServiceWatcher m_watcher;
    qint64 m_pid=0;
    bool m_ready=false;
    quint64 m_draws=0,m_iconDraws=0;
    quint64 m_openings=0,m_closings=0,m_motionFrames=0;
    quint64 m_minimizes=0,m_restores=0,m_anchoredMotions=0,m_reveals=0;
    quint64 m_tailSuppressed=0;
    quint64 m_minimizeConflicts=0;
    quint64 m_minimizeStartFrames=0,m_minimizeStartFades=0;
    QRectF m_lastOrigin;
    QHash<QString,quint64> m_windowDraws;
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    QSet<QString> m_captured;
    bool m_clientReady=false;
    int m_qaScaleApplied=-1;
    bool m_qaCapturePending=false;
    quint64 m_qaSceneCaptures=0;
    QByteArray m_qaSceneHash;
#endif
};
}
