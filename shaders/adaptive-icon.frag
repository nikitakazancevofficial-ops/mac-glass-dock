#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float darkness;
    float availability;
    float padding;
    float plateOpacity;
};
layout(binding=1) uniform sampler2D source;
void main() {
    vec2 uv=qt_TexCoord0;
    vec2 p=abs(uv-.5)*2.0;
    float curve=pow(pow(p.x,4.0)+pow(p.y,4.0),.25);
    float aa=max(fwidth(curve),.012);
    float mask=1.0-smoothstep(.96-aa,.96+aa,curve);
    vec3 color=mix(vec3(.9,.94,.98),vec3(.15,.21,.28),darkness);
    vec4 center=texture(source,vec2(.5));
    color=mix(color,center.rgb/max(center.a,.01),.11*center.a);
    color+=vec3((1.0-uv.y)*.075);
    float edge=exp(-abs(curve-.925)*130.0)*.18;
    color+=edge;
    vec4 plate=vec4(color*mask*plateOpacity,mask*plateOpacity);
    // The entire logo is sampled inside a padded square: never cropped to a squircle.
    vec2 logoUV=(uv-.5)/max(.5,1.0-2.0*padding)+.5;
    float within=step(0.0,logoUV.x)*step(logoUV.x,1.0)*step(0.0,logoUV.y)*step(logoUV.y,1.0);
    vec4 logo=texture(source,clamp(logoUV,0.0,1.0))*within;
    fragColor=(logo+plate*(1.0-logo.a))*qt_Opacity*availability;
}
