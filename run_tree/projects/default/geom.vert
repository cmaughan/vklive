#version 450

layout (location = 0) in vec4 inPos;
layout (location = 2) in vec3 inColor;
layout (location = 3) in vec3 inNormal;

layout (binding = 0) uniform UBO 
{
    vec4 time;
    mat4 projection;
    mat4 model;
    vec4 lightPos;
} ubo;

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outEyePos;
layout (location = 3) out vec3 outLightVec;

void main() 
{
    vec4 p = inPos;
    p.xyz = p.xyz + (inNormal * sin(inPos.x * ubo.time.x * 23.5) * .6);
    gl_Position = ubo.projection * ubo.model * p;
    outEyePos = vec3(ubo.model * p);
    outNormal = inNormal;
    outColor = inColor;
    outLightVec = normalize(ubo.lightPos.xyz - outEyePos);
}
