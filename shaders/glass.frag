#version 440
layout(location=0) in vec2 qt_TexCoord0;
layout(location=0) out vec4 fragColor;
layout(std140,binding=0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float darkness;
    float glassOpacity;
    float highlightIntensity;
    float thickness;
    float edgeReflection;
    float cornerRadius;
    vec2 resolution;
    vec2 pointer;
    vec4 glassTint;
    float hoverAmount;
    float rimWidth;
    float bevelCurve;
    float fresnelPower;
    float edgeDispersion;
    float innerShadowStrength;
    float shadowOpacity;
    float shadowRadius;
    float highlightSize;
};
float roundedBox(vec2 p, vec2 b, float r) {
    vec2 q=abs(p)-b+r;
    return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
}
void main() {
    vec2 p=(qt_TexCoord0-.5)*resolution;
    vec2 b=max(resolution*.5-18.0,vec2(1));
    float r=min(cornerRadius,min(b.x,b.y));
    float d=roundedBox(p,b,r);
    float aa=max(fwidth(d),.001);
    float coverage=1.0-smoothstep(-aa,aa,d);
    float depth=max(-d,0.0);
    float bevel=1.0-smoothstep(0.0,thickness,depth);
    // Rounded-edge normal: x/y from the SDF gradient, z from its thickness profile.
    vec2 q=abs(p)-b+r;
    vec2 outward=normalize(max(q,vec2(.0001)))*sign(p);
    if (max(q.x,q.y)<0.0) outward=q.x>q.y ? vec2(sign(p.x),0) : vec2(0,sign(p.y));
    float slope=pow(bevel,bevelCurve)*.94;
    vec3 n=normalize(vec3(outward*slope,sqrt(max(.04,1.0-slope*slope))));
    vec2 cursor=(pointer-.5)*(resolution-36.0);
    vec3 light=normalize(vec3((cursor-p)*.003,1.1));
    float fresnel=pow(1.0-n.z,fresnelPower);
    float rim=exp(-abs(d+.85)*1.86/rimWidth);
    float diffuse=dot(n,normalize(vec3(-.45,-.65,1.0)));
    float spec=pow(max(dot(reflect(-light,n),vec3(0,0,1)),0.0),28.0);
    vec2 focusDistance=(p-cursor)/vec2(highlightSize,highlightSize*.65);
    float focus=exp(-dot(focusDistance,focusDistance));
    float broad=exp(-pow((p.y+b.y*.55)/(b.y*.55+1.0),2.0))*.006;
    vec3 tint=mix(vec3(.81,.87,.93),vec3(.065,.10,.15),darkness);
    tint=mix(tint,glassTint.rgb,.12);
    // Transmitted center is deliberately clear. No desktop sampler or invented refraction.
    float body=glassOpacity*(.86+.14*bevel);
    vec3 rgb=tint*body;
    float a=body;
    float shading=bevel*(1.0-diffuse)*.10;
    rgb*=1.0-shading;
    float contourLight=.64-.36*outward.y;
    float reflection=(rim*.26*contourLight+fresnel*.11)*edgeReflection;
    reflection+=(spec*.055*bevel+focus*hoverAmount*(.018+.045*bevel)+broad)*highlightIntensity;
    reflection=clamp(reflection,0.0,.7);
    vec3 spectral=vec3(1.0)+vec3(-outward.x,outward.y*.48,outward.x*1.4)*edgeDispersion*fresnel;
    rgb=spectral*reflection+rgb*(1.0-reflection);
    a=reflection+a*(1.0-reflection);
    float innerShadow=bevel*max(dot(outward,vec2(.4,.8)),0.0)*innerShadowStrength;
    rgb*=1.0-innerShadow;
    a=clamp(a+innerShadow*.3,0.0,1.0)*coverage;
    rgb*=coverage;
    float shadow=shadowOpacity*exp(-max(d,0.0)*2.0/shadowRadius)*(1.0-coverage)*(1.0-smoothstep(10.0,18.0,d));
    fragColor=vec4(rgb,a+shadow*(1.0-a))*qt_Opacity;
}
