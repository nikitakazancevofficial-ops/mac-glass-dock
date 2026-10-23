#pragma once

#include <QPointF>
#include <QRectF>
#include <algorithm>

namespace MacGlassStatusHit {
inline bool contains(const QRectF &status, const QRectF &screen, const QPointF &position)
{
    if (status.isEmpty() || screen.isEmpty()) return false;
    const QRectF hitArea(status.x(), status.y(),
                         std::max(0., screen.x() + screen.width() - status.x()),
                         status.height());
    return hitArea.contains(position);
}

inline bool compositorOwnsClick(const QRectF &status, const QRectF &screen,
                                const QPointF &position)
{
    // The visible pill is handled by its QML TapHandler. KWin owns only the
    // transparent extension between that pill and the screen's right edge.
    if (status.isEmpty() || screen.isEmpty()) return false;
    const QRectF hitArea(status.x(), status.y(),
                         std::max(0., screen.x() + screen.width() - status.x()),
                         status.height());
    return hitArea.contains(position) && !status.contains(position);
}

inline bool isVisibleStatusRowClick(const QRectF &status, const QPointF &position)
{
    return !status.isEmpty() && status.contains(position);
}
}
