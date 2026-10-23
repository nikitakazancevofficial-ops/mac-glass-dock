// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#include "refraction.h"
#include "geniegeometry.h"
#include <effect/effecthandler.h>
#include <QPainterPath>

namespace KWin {
namespace {
QRectF rectangle(const QJsonObject &o) {return {o["x"].toDouble(),o["y"].toDouble(),o["w"].toDouble(),o["h"].toDouble()};}
}
void MacGlassRefraction::syncPanelMotions() {
    effects->makeOpenGLContextCurrent();
    for(auto *w:m_panelMotions.values()) {
        if(m_windows.value(key(w)).toObject().contains("genie") && !w->isDeleted())continue;
        // A missing description after a mapped Genie is an unmap, not a
        // request to draw the full-sized source buffer for another effect.
        if(m_pid && m_windows.value(key(w)).toObject().isEmpty())m_hiddenPanels.insert(w);
        m_panelMotions.remove(w);unredirect(w);effects->addRepaintFull();
    }
    for(auto *w:effects->stackingOrder()) {
        const QString id=key(w);
        const auto state=m_windows.value(id).toObject();
        if((id=="launcherWindow" || state.contains("genie")) && !state["terminalHidden"].toBool())m_hiddenPanels.remove(w);
        if(!state.contains("genie"))continue;
        if(state["terminalHidden"].toBool())m_hiddenPanels.insert(w);
        if(!m_panelMotions.contains(w)) {
            redirect(w);setShader(w,m_motionShader.get());m_panelMotions.insert(w);w->addRepaintFull();
        }
        repaintPanelMotion(w);
        if(state["genie"].toObject()["waiting"].toBool())emit panelMotionReady(id,uint(state["surface"].toInt()));
    }
}
void MacGlassRefraction::repaintPanelMotion(EffectWindow *w) {
    const auto motion=m_windows.value(key(w)).toObject()["genie"].toObject();
    if(motion.isEmpty() || motion["waiting"].toBool())return;
    const auto offset=w->frameGeometry().topLeft();
    const auto trajectory=rectangle(motion["frame"].toObject()).united(rectangle(motion["icon"].toObject()))
        .translated(offset).adjusted(-64,-64,64,64);
    // Damage the scene below, not just the redirected client texture. Unlike
    // the animated QA wallpaper, a normal desktop doesn't repaint for us.
    effects->addRepaint(RectF(trajectory));
    ++m_panelBackgroundRepaints;
}
QRectF MacGlassRefraction::panelMotionBounds(EffectWindow *w) const {
    const auto state=m_windows.value(key(w)).toObject();
    return rectangle(state["input"].toObject()).translated(w->frameGeometry().topLeft());
}
bool MacGlassRefraction::applyPanelMotion(EffectWindow *w,WindowPaintData &data,WindowQuadList &quads) {
    if(!m_panelMotions.contains(w))return false;
    const auto motion=m_windows.value(key(w)).toObject()["genie"].toObject();
    const QPointF offset=w->frameGeometry().topLeft();
    const QRectF localFrame=rectangle(motion["frame"].toObject());
    const QRectF frame=localFrame.translated(offset);
    const QRectF icon=rectangle(motion["icon"].toObject()).translated(offset);
    const double openness=qBound(0.,motion["openness"].toDouble(1),1.);
    const bool clipFrame=motion["clipFrame"].toBool();
    quads=quads.makeRegularGrid(48,96);
    for(auto &quad:quads)for(int i=0;i<4;++i) {
        auto &vertex=quad[i];
        if(clipFrame && !localFrame.contains(QPointF(vertex.x(),vertex.y())))continue;
        const QPointF p=MacGlassGenie::map(QPointF(vertex.x(),vertex.y())+offset-frame.topLeft(),frame,icon,1.-openness)-offset;
        vertex.move(p.x(),p.y());
    }
    if(!clipFrame)data.multiplyOpacity(MacGlassGenie::opacity(openness));
    ++m_panelMotionFrames;
    return true;
}
}
