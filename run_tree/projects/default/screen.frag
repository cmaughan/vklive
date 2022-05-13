#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inEyePos;
layout (location = 3) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;

layout (binding = 0) uniform UBO 
{
    vec4 time;
    mat4 projection;
    mat4 model;
    vec4 lightPos;
} ubo;

void main() 
{
    // Temp
    vec2 resolution = vec2(512, 512);
    vec2 rg = (gl_FragCoord.xy / resolution) * 2.0 - 1.0; 
    float t = sin(ubo.time.x * .15f) * 10.0;
    rg.x = sin(rg.x * t) + cos(rg.y * t);
    rg.y = sin(rg.y * t) + cos(rg.x * t);

    outFragColor.zw = vec2(0.0, 1.0);
    outFragColor.xy = rg;
}