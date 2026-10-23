// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#include "refraction.h"
#include "geniegeometry.h"
#include <effect/effecthandler.h>
#include <opengl/glshader.h>
#include <QEasingCurve>
#include <QTimer>
#include <cmath>

namespace KWin {
void MacGlassRefraction::observeWindow(EffectWindow *w) {
    connect(w,&EffectWindow::minimizedChanged,this,&MacGlassRefraction::minimizedChanged,Qt::UniqueConnection);
    connect(w,&EffectWindow::windowFrameGeometryChanged,this,&MacGlassRefraction::syncOcclusion,Qt::UniqueConnection);
    connect(w,&EffectWindow::windowHiddenChanged,this,&MacGlassRefraction::syncOcclusion,Qt::UniqueConnection);
}
void MacGlassRefraction::minimizedChanged(EffectWindow *w) {
    if(!w->isMinimized())m_settledHidden.remove(w);
    beginMotion(w,w->isMinimized()?Transition::Minimize:Transition::Restore);
    syncOcclusion();
}
void MacGlassRefraction::beginMotion(EffectWindow *w,Transition transition) {
    const bool closing=transition==Transition::Close || transition==Transition::Minimize;
    const bool inFlight=m_motions.contains(w);
    if(!m_pid || !m_motionShader || !m_options["windowAnimations"].toBool()
        || effects->isScreenLocked() || effects->activeFullScreenEffect()
        || !w->isNormalWindow() || (!w->isVisible() && transition!=Transition::Minimize && !inFlight) || w->isFullScreen()
        || !w->isManaged() || w->isLockScreen() || w->isPopupWindow() || w->isOutline() || w->isSpecialWindow()
        || !w->isOnCurrentDesktop() || !w->isOnCurrentActivity()
        || w->pid()==m_pid || (transition==Transition::Close && w->skipsCloseAnimation())) return;
    // Panel, popup, lock-screen and desktop windows are never candidates.
    // An icon-owned transition intentionally replaces the generic desktop
    // animation for this window only; unrelated effects keep their grabs.
    const int role=transition==Transition::Minimize?WindowMinimizedGrabRole:transition==Transition::Restore?WindowUnminimizedGrabRole:closing?WindowClosedGrabRole:WindowAddedGrabRole;
    const auto found=m_motions.find(w);
    const qreal openness=found==m_motions.end() ? (closing?1.:0.) : found->second->openness();
    const QRectF origin=inFlight?found->second->origin:originFor(w);
    if(const QVariant grab=w->data(role);grab.isValid() && grab.value<void *>()!=this && origin.isEmpty()) return;
    // Keep a deleted reference across cancellation of an in-flight opening.
    const EffectWindowDeletedRef keepAlive=transition==Transition::Close ? EffectWindowDeletedRef(w) : EffectWindowDeletedRef();
    finishMotion(w);
    auto motion=std::make_unique<Motion>();
    motion->closing=closing; motion->from=openness;motion->role=role;
    motion->origin=origin;
    motion->repaintRect=QRectF(w->expandedGeometry()).united(origin).adjusted(-2,-2,2,2);
    m_lastOrigin=origin;
    motion->deleted=keepAlive;
    motion->visible.emplace(w,EffectWindow::PAINT_DISABLED|EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
    const double speed=qBound(.25,m_options["animationSpeed"].toDouble(1),3.);
    const double remaining=closing?openness:1.-openness;
    const double duration=(closing?650.:620.)/std::pow(speed,.35)*remaining;
    motion->time.setDuration(std::chrono::milliseconds(qBound(60,int(duration),1000)));
    motion->time.setEasingCurve(QEasingCurve::InOutSine);
    m_motions[w]=std::move(motion);
    w->setData(role,QVariant::fromValue(static_cast<void *>(this)));
    redirect(w);setShader(w,m_motionShader.get());
#ifdef MACGLASS_NESTED_TEST_CAPTURE
    // Reproduce a second effect retaining the window beyond our final frame.
    // This deliberately exercises the end-of-animation flash, in QA only.
    if(closing && w->windowClass().contains("mac-glass-dock-probe")
        && qEnvironmentVariableIsSet("MACGLASS_TEST_TAIL_HOLD")) {
        struct Hold {
            EffectWindowDeletedRef deleted;
            EffectWindowVisibleRef visible;
            explicit Hold(EffectWindow *window):deleted(window),visible(window,EffectWindow::PAINT_DISABLED|EffectWindow::PAINT_DISABLED_BY_MINIMIZE){}
        };
        const auto hold=std::make_shared<Hold>(w);
        QTimer::singleShot(1600,this,[hold]{});
    }
#endif
    if(transition==Transition::Minimize)++m_minimizes;
    else if(transition==Transition::Restore)++m_restores;
    else if(closing)++m_closings;else ++m_openings;
    if(!origin.isEmpty())++m_anchoredMotions;
    effects->addRepaint(RectF(m_motions[w]->repaintRect));
}
void MacGlassRefraction::finishMotion(EffectWindow *w,bool completed) {
    auto it=m_motions.find(w);if(it==m_motions.end())return;
    auto motion=std::move(it->second);m_motions.erase(it);
    if(completed && motion->closing && (w->isMinimized() || w->isDeleted()))m_settledHidden.insert(w);
    // Release forced visibility while the redirected final frame still exists.
    // Another effect may retain its own visible/deleted ref: the settled guard
    // prevents that reference from repainting the undeformed original window.
    motion->visible.reset();
    unredirect(w);
    if(w->data(motion->role).value<void *>()==this) w->setData(motion->role,QVariant());
    effects->addRepaint(RectF(motion->repaintRect));
    // motion releases its visible/deleted refs after erasing the map entry.
}
void MacGlassRefraction::clearMotions() {
    while(!m_motions.empty()) finishMotion(m_motions.begin()->first);
    m_settledHidden.clear();
}
void MacGlassRefraction::apply(EffectWindow *w,int,WindowPaintData &data,WindowQuadList &quads) {
    if(applyPanelMotion(w,data,quads))return;
    const auto it=m_motions.find(w);if(it==m_motions.end())return;
    const double openness=it->second->openness();
    const QRectF origin=it->second->origin,frame=w->frameGeometry();
    const bool minimizing=it->second->role==WindowMinimizedGrabRole;
    const bool restoring=it->second->role==WindowUnminimizedGrabRole;
    if(minimizing && openness>.9 && data.opacity()<.8) ++m_minimizeConflicts;
    if(minimizing || restoring) {
        // Squash doesn't honor the minimized grab role. Its paintWindow pass
        // can fade/translate/resize this same window before our drawWindow.
        // Own the animation transform here, without unloading a global effect.
        // WindowItem opacity is already baked into the offscreen texture;
        // neutral paint opacity is 1, not window->opacity() a second time.
        data.setOpacity(1);
        data.setXScale(1);data.setYScale(1);data.setZScale(1);
        data.setXTranslation(0);data.setYTranslation(0);data.setZTranslation(0);
        data.setRotationAngle(0);
    }
    const auto checkStart=[&] {
        if(minimizing && openness>.9) {
            ++m_minimizeStartFrames;
            if(data.opacity()<.99)++m_minimizeStartFades;
        }
    };
    if(!origin.isEmpty()) {
        // High vertical resolution preserves the smooth funnel silhouette.
        quads=quads.makeRegularGrid(qBound(12,int(frame.width()/40),32),qBound(32,int(frame.height()/18),64));
        for(auto &quad:quads)for(int i=0;i<4;++i) {
            auto &vertex=quad[i];
            const QPointF p=MacGlassGenie::map({vertex.x(),vertex.y()},frame,origin,1.-openness)-frame.topLeft();
            vertex.move(p.x(),p.y());
        }
        data.multiplyOpacity(MacGlassGenie::opacity(openness));
        checkStart();
        return;
    }
    const double sx=origin.isEmpty()?.88:qBound(.015,origin.width()/qMax(1.,frame.width()),.6);
    const double sy=origin.isEmpty()?.88:qBound(.015,origin.height()/qMax(1.,frame.height()),.6);
    const double scaleX=sx+(1-sx)*openness,scaleY=sy+(1-sy)*openness;
    const QPointF start=origin.isEmpty()?frame.center()+QPointF(0,18):origin.center();
    const QPointF center=start*(1-openness)+frame.center()*openness-QPointF(0,48*std::sin(M_PI*openness)*(1-openness));
    data.setXTranslation(data.xTranslation()+center.x()-frame.x()-frame.width()*scaleX*.5);
    data.setYTranslation(data.yTranslation()+center.y()-frame.y()-frame.height()*scaleY*.5);
    data.setXScale(data.xScale()*scaleX);data.setYScale(data.yScale()*scaleY);
    data.multiplyOpacity(qBound(0.,openness*3.,1.));
    checkStart();
}
void MacGlassRefraction::postPaintScreen() {
    for(auto *w:m_panelMotions)repaintPanelMotion(w);
    QList<EffectWindow *> done;
    for(auto &[w,motion]:m_motions) {
        if(motion->time.done()) done.append(w);
        else effects->addRepaint(RectF(motion->repaintRect));
    }
    for(auto *w:done) finishMotion(w,true);
    effects->postPaintScreen();
}
}
