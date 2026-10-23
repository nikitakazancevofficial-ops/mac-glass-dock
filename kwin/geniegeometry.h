// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#pragma once
#include <QPointF>
#include <QRectF>
#include <algorithm>
#include <cmath>

namespace MacGlassGenie {
inline double smooth(double low,double high,double value) {
    const double t=std::clamp((value-low)/(high-low),0.,1.);
    return t*t*t*(t*(t*6.-15.)+10.);
}
// Window-local input, global output. Progress is 0=open, 1=inside its icon.
// First pull a curved neck to the icon; then feed the sheet through the neck.
inline QPointF map(const QPointF &point,const QRectF &frame,const QRectF &icon,double progress) {
    const double t=std::clamp(progress,0.,1.);
    const double u=point.x()/std::max(1.,frame.width());
    const double v=point.y()/std::max(1.,frame.height());
    const QPointF original=frame.topLeft()+point;
    const QPointF destination=icon.topLeft()+QPointF(u*icon.width(),v*icon.height());
    if(t<=0)return original;
    if(t>=1)return destination;
    const bool fromTop=icon.bottom()-frame.top() < -32.;
    const double sourceY=fromTop ? frame.height()-point.y() : point.y();
    const double destinationY=fromTop
        ? frame.height()-(icon.top()-frame.top()+v*icon.height())
        : destination.y();
    const double span=fromTop
        ? frame.height()-(icon.bottom()-frame.top())
        : icon.top()-frame.top();
    if(span<32.)return original*(1.-smooth(0.,1.,t))+destination*smooth(0.,1.,t);
    const double pinch=smooth(0.,.46,t);
    const double feed=smooth(.30,1.,t);
    const double gap=std::max(0.,span-frame.height());
    const double side=std::clamp((icon.center().x()-frame.center().x())/std::max(1.,frame.width()*.5),-1.,1.);
    const double nearSide=side>=0 ? u : 1.-u;
    // A short diagonal tension in phase one, gone once the neck is formed.
    // This gives the tilted sheet in the reference without rotating the Dock.
    const double tilt=.14*frame.height()*std::abs(side)*nearSide*(1.-std::clamp(v,0.,1.))
        *std::sin(3.141592653589793*std::clamp(t/.46,0.,1.));
    const double stretchedY=sourceY+pinch*gap*smooth(.18,1.,v)+feed*span+tilt;
    const double y=stretchedY*(1.-pinch)+std::min(stretchedY,destinationY)*pinch;
    const double along=std::clamp(stretchedY/span,0.,1.);
    const double contraction=pinch*smooth(0.,1.,along);
    return {original.x()+(destination.x()-original.x())*contraction,
        frame.top()+(fromTop ? frame.height()-y : y)};
}
inline double opacity(double openness) {return smooth(0.,.075,openness);}
}
