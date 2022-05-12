#version 450

#extension GL_ARB_separate_shader_objects : enable
#extension GL_ARB_shading_language_420pack : enable

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
    vec2 coord = ((gl_FragCoord.xy / resolution) * 2.0 - 1.0);

    vec2 toCenter = vec2(0.5)-coord;
    float angle = atan(toCenter.y, toCenter.x);
    angle *= sin(ubo.time.x * .5);
    float radius = length(toCenter)*4.0;

    vec4 color = vec4(1.0);
    color.x = 1 - cos(angle * ubo.time.x * .01); 
    color.y = sin(angle * ubo.time.x * .01); 
    color.z = 0.5;

    outFragColor = color;
}