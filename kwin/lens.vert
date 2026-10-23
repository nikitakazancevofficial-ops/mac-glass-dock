#version 330 core
in vec2 position;
in vec2 texcoord;
out vec2 uv;
out vec2 samplePosition;
uniform mat4 modelViewProjectionMatrix;
uniform bool warped;
uniform float renderScale;
uniform vec2 cropOrigin;
void main() {
    uv=texcoord;
    samplePosition=warped ? position/max(renderScale,.001)-cropOrigin : vec2(0);
    gl_Position=modelViewProjectionMatrix*vec4(position,0,1);
}
