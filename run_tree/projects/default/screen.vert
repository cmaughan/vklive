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
    outNormal = inNormal;
    outColor = inColor;
    gl_Position = inPos;
    outEyePos = vec3(ubo.model * inPos);
    outLightVec = normalize(ubo.lightPos.xyz - outEyePos);
}
