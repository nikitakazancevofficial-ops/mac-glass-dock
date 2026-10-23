#version 440
layout(location=0) in vec4 qt_Vertex;
layout(location=1) in vec2 qt_MultiTexCoord0;
layout(location=0) out vec2 texCoord;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float openness;
    vec2 panelSize;
    vec4 iconRect;
};
float easeRange(float a,float b,float x) {
    float t=clamp((x-a)/(b-a),0.,1.);
    return t*t*t*(t*(t*6.-15.)+10.);
}
void main() {
    // Same two-phase mapping as kwin/geniegeometry.h; no rigid scale substitute.
    vec2 p=qt_Vertex.xy;
    vec2 uv=p/max(panelSize,vec2(1));
    vec2 dst=iconRect.xy+uv*iconRect.zw;
    // A status bar sits above the sheet. Mirror the vertical mapping so the
    // same neck-and-feed motion grows upward from that top edge anchor.
    bool fromTop=iconRect.y+iconRect.w < -32.;
    float sourceY=fromTop ? panelSize.y-p.y : p.y;
    float destinationY=fromTop ? panelSize.y-(iconRect.y+uv.y*iconRect.w) : dst.y;
    float t=clamp(1.-openness,0.,1.);
    float span=fromTop ? panelSize.y-(iconRect.y+iconRect.w) : iconRect.y;
    if(span<32.) p=mix(p,dst,easeRange(0.,1.,t));
    else {
        float pinch=easeRange(0.,.46,t);
        float feed=easeRange(.30,1.,t);
        float gap=max(0.,span-panelSize.y);
        float side=clamp((iconRect.x+iconRect.z*.5-panelSize.x*.5)/max(1.,panelSize.x*.5),-1.,1.);
        float nearSide=side>=0. ? uv.x : 1.-uv.x;
        float tilt=.14*panelSize.y*abs(side)*nearSide*(1.-clamp(uv.y,0.,1.))
            *sin(3.141592654*clamp(t/.46,0.,1.));
        float stretched=sourceY+pinch*gap*easeRange(.18,1.,uv.y)+feed*span+tilt;
        float y=mix(stretched,min(stretched,destinationY),pinch);
        float contraction=pinch*easeRange(0.,1.,clamp(stretched/span,0.,1.));
        p=vec2(mix(p.x,dst.x,contraction),fromTop ? panelSize.y-y : y);
    }
    texCoord=qt_MultiTexCoord0;
    gl_Position=qt_Matrix*vec4(p,qt_Vertex.zw);
}
