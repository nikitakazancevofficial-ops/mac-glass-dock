// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#include "refraction.h"
#include "geniegeometry.h"
#include "statushitgeometry.h"
#include "dockedgegeometry.h"
#include <effect/effecthandler.h>
#include <wayland/surface.h>
#include <wayland/layershell_v1.h>
#include <input_event.h>
#include <QJsonArray>
#include <QPainterPath>
#include <cmath>
#ifdef MACGLASS_NESTED_TEST_CAPTURE
#include <window.h>
#endif

namespace KWin {
namespace {
QRectF rectangle(const QJsonObject &o) {return {o["x"].toDouble(),o["y"].toDouble(),o["w"].toDouble(),o["h"].toDouble()};}
bool contains(const QJsonObject &o,const QPointF &p) {
    const QRectF r=rectangle(o);
    if(!r.contains(p) || (o.contains("clip") && !rectangle(o["clip"].toObject()).contains(p)))return false;
    const double radius=qBound(0.,o["r"].toDouble(),qMin(r.width(),r.height())*.5);
    const QPointF d=p-r.center();
    const double x=std::abs(d.x())-r.width()/2+radius,y=std::abs(d.y())-r.height()/2+radius;
    return std::hypot(qMax(x,0.),qMax(y,0.))+qMin(qMax(x,y),0.)<=radius;
}
}
void MacGlassRefraction::pointerButton(PointerButtonEvent *event) {
    if(event->state!=PointerButtonState::Pressed)return;
    if(event->button==Qt::LeftButton && MacGlassStatusHit::isVisibleStatusRowClick(
            rectangle(m_dock.value("statusRegion").toObject()),event->position))
        return; // Let the client's TapHandler toggle; don't classify this as outside.
    if(event->button==Qt::LeftButton && controlCenterStatusHit(event->position)) {
        // The status pill is a layer-shell overlay. Route its activation from
        // the compositor's global pointer coordinates so edge clicks do not
        // depend on Qt's local window origin or client-side hit testing.
        emit controlCenterStatusPressed();
        return;
    }
    dismissOutside(event->position);
}
void MacGlassRefraction::touchDown(TouchDownEvent *event) {dismissOutside(event->pos);}
bool MacGlassRefraction::controlCenterStatusHit(const QPointF &position) const {
    const QRectF status=rectangle(m_dock.value("statusRegion").toObject());
    const QRectF screen=rectangle(m_dock.value("screen").toObject());
    return MacGlassStatusHit::compositorOwnsClick(status,screen,position);
}
void MacGlassRefraction::dismissOutside(const QPointF &position) {
    if(!m_pid || effects->isScreenLocked())return;
    // Observe only. InputEventSpy has no way to swallow or redirect the event.
    // Publish no coordinates or information about the application underneath.
    for(auto *w:effects->stackingOrder()) {
        const QString name=key(w);const auto state=m_windows.value(name).toObject();
        if(name.isEmpty() || !state["dismissOutside"].toBool() || w->isDeleted())continue;
        const QPointF local=position-w->frameGeometry().topLeft();
        bool inside=contains(state["input"].toObject(),local);
        if(inside && state.contains("genie") && !state["stableInput"].toBool()) {
            const auto motion=state["genie"].toObject();
            const QRectF frame=rectangle(motion["frame"].toObject()),icon=rectangle(motion["icon"].toObject());
            const double radius=qBound(0.,motion["radius"].toDouble(),qMin(frame.width(),frame.height())/2);
            QPainterPath shape;shape.addRoundedRect(frame,radius,radius);QPolygonF warped;
            for(int i=0;i<=160;++i)warped<<MacGlassGenie::map(shape.pointAtPercent(i/160.)-frame.topLeft(),frame,icon,1-motion["openness"].toDouble(1));
            inside=warped.containsPoint(local,Qt::OddEvenFill);
        }
        if(!inside)emit panelOutsidePressed(name);
    }
}
EffectWindow *MacGlassRefraction::dockWindow() const {
    const int surfaceId=m_dock["surface"].toInt();if(!m_pid || !surfaceId)return nullptr;
    for(auto *w:effects->stackingOrder()) {
        auto *surface=w->surface();
        if(!w->isDeleted() && w->pid()==m_pid && surface && surface->role()==LayerSurfaceV1Interface::role() && int(surface->id())==surfaceId)return w;
    }
    return nullptr;
}
void MacGlassRefraction::refreshAnchors() {
    const auto *dock=dockWindow();if(!dock)return;
    const QRectF frame=dock->frameGeometry();
    const QPointF reference(frame.center().x(),frame.bottom()-(m_dock["hidden"].toBool()?4:0));
    const qint64 now=m_anchorClock.elapsed();
    for(const auto &value:m_dock["targets"].toArray()) {
        const auto target=value.toObject();const QRectF r=rectangle(target).translated(reference);
        if(r.isEmpty())continue;
        for(const auto &alias:target["aliases"].toArray()) {
            const auto name=alias.toString().toCaseFolded();
            if(!name.isEmpty())m_anchors[name]={r,now};
        }
    }
    for(auto it=m_anchors.begin();it!=m_anchors.end();) {
        if(now-it->updated>5000)it=m_anchors.erase(it);else ++it;
    }
}
QRectF MacGlassRefraction::originFor(EffectWindow *w) {
    refreshAnchors();
    for(auto name:w->windowClass().toCaseFolded().split(' ',Qt::SkipEmptyParts)) {
        if(name.endsWith(".desktop"))name.chop(8);
        if(auto it=m_anchors.constFind(name);it!=m_anchors.cend() && m_anchorClock.elapsed()-it->updated<=5000)return it->rect;
    }
    return {};
}
void MacGlassRefraction::syncOcclusion() {
    auto *dock=dockWindow();bool obstructed=false;
    bool statusObstructed=false;
    if(dock && !effects->isScreenLocked() && !m_showingDesktop) {
        const QRectF frame=dock->frameGeometry();
        const QPointF reference(frame.center().x(),frame.bottom()-(m_dock["hidden"].toBool()?4:0));
        const QRectF surface=rectangle(m_dock["hitRect"].toObject()).translated(reference);
        if(!surface.isEmpty())for(auto *w:effects->stackingOrder()) {
            if(w->pid()==m_pid || w->isDeleted() || w->isMinimized() || !w->isVisible()
                || (!w->isNormalWindow() && !w->isDialog()) || !w->isOnCurrentDesktop() || !w->isOnCurrentActivity())continue;
            if(surface.intersects(w->frameGeometry())) {obstructed=true;break;}
        }
        const QRectF statusSurface=rectangle(m_dock["statusRegion"].toObject());
        for(auto *w:effects->stackingOrder()) {
            if(w->pid()==m_pid || w->isDeleted() || w->isMinimized() || !w->isVisible()
                || (!w->isNormalWindow() && !w->isDialog()) || !w->isOnCurrentDesktop() || !w->isOnCurrentActivity())continue;
            if(statusSurface.intersects(w->frameGeometry())) {statusObstructed=true;break;}
        }
    }
    if(obstructed!=m_obstructed) {m_obstructed=obstructed;emit obstructedChanged(obstructed);}
    if(statusObstructed!=m_statusObstructed) {m_statusObstructed=statusObstructed;emit statusObstructedChanged(statusObstructed);}
}
void MacGlassRefraction::handlePointer(const QPointF &p,bool geometryChanged) {
    if(m_syntheticInput)return; // QtTest pointer injection doesn't move KWin's hardware cursor.
    auto *dock=dockWindow();bool inside=false,edge=false;
    bool controlCenterEdge=false;
    if(!effects->isScreenLocked()) {
        const QRectF screen=rectangle(m_dock["screen"].toObject());
        // Keep the control-center edge watcher independent of the Dock surface:
        // fullscreen clients can cover the Dock, but must not disable this edge.
        const QRectF statusRegion=rectangle(m_dock["statusRegion"].toObject());
        // Reveal only from the narrow physical right edge, from the top of the
        // output down to the status pill. This remains observable over fullscreen.
        const QRectF statusHoverRegion(screen.right()-4,screen.top(),4,
                                      qMax(0.,statusRegion.bottom()-screen.top()+1));
        controlCenterEdge=screen.contains(p) && !statusRegion.isEmpty() && statusHoverRegion.contains(p);
        if(dock) {
            const QRectF frame=dock->frameGeometry();
            const bool hidden=m_dock["hidden"].toBool();
            if(!hidden)for(const auto &lens:m_windows.value("dockWindow").toObject()["lenses"].toArray()) {
                if(contains(lens.toObject(),p-frame.topLeft())) {inside=true;break;}
            }
            // Reveal only below the padded Dock width at the physical bottom
            // edge. Its vertical animation must not move the trigger upwards.
            edge=m_dock["autoHide"].toBool()
                && (MacGlassDockEdge::contains(screen,frame,p) || (!hidden && inside));
            if(edge && hidden) {++m_reveals;emit revealRequested();}
        }
    }
    if(edge!=m_edgeHover) {m_edgeHover=edge;emit edgeHoverChanged(edge);}
    if(controlCenterEdge!=m_controlCenterEdgeHover) {
        m_controlCenterEdgeHover=controlCenterEdge;
        emit controlCenterEdgeHoverChanged(controlCenterEdge);
    }
    if(!inside && (m_pointerWasInside || geometryChanged))emit pointerOutsideDock();
    m_pointerWasInside=inside;
}
#ifdef MACGLASS_NESTED_TEST_CAPTURE
void MacGlassRefraction::testPanelPress(const QString &name,bool inside) {
    if(!qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE"))return;
    for(auto *w:effects->stackingOrder())if(key(w)==name) {
        const auto box=rectangle(m_windows.value(name).toObject()["input"].toObject());
        PointerButtonEvent event{};
        event.state=PointerButtonState::Pressed;
        event.position=w->frameGeometry().topLeft()+(inside?box.center():QPointF(1,1));
        pointerButton(&event);return;
    }
}
void MacGlassRefraction::placeProbe(bool overlap) {
    if(!qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE"))return;
    auto *dock=dockWindow();if(!dock)return;
    const QRectF frame=dock->frameGeometry();
    const QPointF reference(frame.center().x(),frame.bottom()-(m_dock["hidden"].toBool()?4:0));
    const QRectF surface=rectangle(m_dock["hitRect"].toObject()).translated(reference);
    const QRectF screen=rectangle(m_dock["screen"].toObject());
    for(auto *w:effects->stackingOrder())if(!w->isDeleted() && w->windowClass().contains("mac-glass-dock-probe") && w->window()) {
        QRectF geometry=w->frameGeometry();
        geometry.moveTopLeft(overlap?QPointF(surface.center().x()-geometry.width()/2,surface.top()-80):screen.topLeft()+QPointF(40,70));
        w->window()->moveResize(RectF(geometry));
    }
}
void MacGlassRefraction::testPointer(double x,double y) {
    if(!qEnvironmentVariableIsSet("MACGLASS_NESTED_CAPTURE"))return;
    const bool synthetic=m_syntheticInput;m_syntheticInput=false;
    handlePointer(QPointF(x,y),true);m_syntheticInput=synthetic;
}
#endif
}
