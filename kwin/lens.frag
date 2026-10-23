#version 330 core
in vec2 uv;
in vec2 samplePosition;
out vec4 fragColor;
uniform sampler2D backdrop;
uniform vec2 cropSize;
uniform vec4 box;
uniform vec2 lensCenter;
uniform float radius;
uniform float bend;
uniform float band;
uniform float profile;
uniform float blurRadius;
uniform float dispersion;
uniform float zoom;
uniform float opacity;
uniform bool warped;
float sdf(vec2 p,vec2 b,float r) {
    vec2 q=abs(p)-b+r;
    return length(max(q,0.0))+min(max(q.x,q.y),0.0)-r;
}
vec2 texuv(vec2 p) { return clamp(vec2(p.x/cropSize.x,1.0-p.y/cropSize.y),vec2(0),vec2(1)); }
vec3 sampleBlur(vec2 p) {
    if(blurRadius<.01) return texture(backdrop,texuv(p)).rgb;
    vec2 d=vec2(blurRadius,0);
    vec3 c=texture(backdrop,texuv(p)).rgb*.25;
    c+=(texture(backdrop,texuv(p+d)).rgb+texture(backdrop,texuv(p-d)).rgb)*.125;
    d=d.yx;
    c+=(texture(backdrop,texuv(p+d)).rgb+texture(backdrop,texuv(p-d)).rgb)*.125;
    d=vec2(blurRadius*.707);
    c+=(texture(backdrop,texuv(p+d)).rgb+texture(backdrop,texuv(p-d)).rgb
        +texture(backdrop,texuv(p+vec2(d.x,-d.y))).rgb+texture(backdrop,texuv(p+vec2(-d.x,d.y))).rgb)*.0625;
    return c;
}
void main() {
    vec2 p=vec2(uv.x,1.0-uv.y)*cropSize;
    vec2 local=p-(box.xy+box.zw*.5);
    vec2 halfBox=box.zw*.5;
    float r=min(radius,min(halfBox.x,halfBox.y));
    float distance=sdf(local,halfBox,r);
    float aa=max(fwidth(distance),.001);
    float coverage=(1.0-smoothstep(-aa,aa,distance))*opacity;
    if(coverage<.001) discard;
    vec2 q=abs(local)-halfBox+r;
    vec2 normal=normalize(max(q,vec2(.0001)))*sign(local);
    if(max(q.x,q.y)<0.) normal=q.x>q.y ? vec2(sign(local.x),0) : vec2(0,sign(local.y));
    float t=clamp(-distance/max(band,1.),0.,1.);
    // Bell-shaped displacement vanishes on the boundary and at the clear center.
    // This samples real compositor pixels, not a generated wallpaper or client texture.
    float lens=pow(max(sin(t*3.14159265),0.),profile);
    // A moving lens samples the live pixels under its DEFORMED location.
    // UV still describes its original rounded surface, keeping rim/blur intact.
    // Zoom must use screen-space displacement too. Source UVs follow the
    // folding sheet, so using `local` here drags the original background along
    // with it whenever zoom != 1 (notably the user's .94 setting).
    vec2 displaced=(warped ? lensCenter+(samplePosition-lensCenter)/max(zoom,.5)
                          : box.xy+halfBox+local/max(zoom,.5))-normal*bend*lens;
    vec3 color=sampleBlur(displaced);
    if(dispersion*lens>.001) {
        color.r=sampleBlur(displaced+normal*dispersion*lens).r;
        color.b=sampleBlur(displaced-normal*dispersion*lens).b;
    }
    fragColor=vec4(color*coverage,coverage);
}
