#version 440
layout(location=0) in vec2 texCoord;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float openness;
    vec2 panelSize;
    vec4 iconRect;
};
layout(binding=1) uniform sampler2D source;
void main() {
    float t=clamp(openness/.075,0.,1.);
    float fade=t*t*t*(t*(t*6.-15.)+10.);
    vec2 offset=vec2(0.,1.25*(1.-openness)/max(1.,panelSize.y));
    fragColor=(texture(source,texCoord)*.5+texture(source,texCoord-offset)*.25
        +texture(source,texCoord+offset)*.25)*qt_Opacity*fade;
}
