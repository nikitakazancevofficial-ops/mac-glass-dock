// SPDX-License-Identifier: LicenseRef-MacGlass-Personal-NonCommercial-1.0
#version 140
#include "colormanagement.glsl"
#include "saturation.glsl"
uniform sampler2D sampler;
uniform vec4 modulation;
uniform float motionBlur;
in vec2 texcoord0;
out vec4 fragColor;
void main() {
    vec2 stepSize=vec2(motionBlur)/vec2(textureSize(sampler,0));
    vec4 tex=texture(sampler,texcoord0)*.25;
    tex+=(texture(sampler,texcoord0+vec2(stepSize.x,0))+texture(sampler,texcoord0-vec2(stepSize.x,0))
        +texture(sampler,texcoord0+vec2(0,stepSize.y))+texture(sampler,texcoord0-vec2(0,stepSize.y)))*.125;
    tex+=(texture(sampler,texcoord0+stepSize)+texture(sampler,texcoord0-stepSize)
        +texture(sampler,texcoord0+vec2(stepSize.x,-stepSize.y))+texture(sampler,texcoord0+vec2(-stepSize.x,stepSize.y)))*.0625;
    tex=sourceEncodingToNitsInDestinationColorspace(tex);
    fragColor=nitsToDestinationEncoding(adjustSaturation(tex)*modulation);
}
