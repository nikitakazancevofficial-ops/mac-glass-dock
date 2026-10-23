#pragma once
#include <QPointF>
#include <QRectF>

namespace MacGlassDockEdge {
inline bool contains(const QRectF &screen,const QRectF &dockFrame,const QPointF &position) {
    // The layer frame includes 28 logical pixels beside each visible Dock edge.
    // Keep that small margin, while rejecting the rest of the screen's bottom.
    return !screen.isEmpty() && position.x()>=screen.left() && position.x()<screen.right()
        && !dockFrame.isEmpty() && position.x()>=dockFrame.left() && position.x()<dockFrame.right()
        && position.y()>=screen.bottom()-2 && position.y()<screen.bottom();
}
}
