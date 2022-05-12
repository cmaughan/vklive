#version 450

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec3 inEyePos;
layout (location = 3) in vec3 inLightVec;

layout (location = 0) out vec4 outFragColor;

void main() 
{
    vec3 Color = inColor;
    vec3 Eye = normalize(-inEyePos);
    vec3 Reflected = normalize(reflect(-inLightVec, inNormal)); 
    vec4 IAmbient = vec4(0.1, 0.3, 0.1, 1.0);
    vec4 IDiffuse = vec4(max(dot(inNormal, inLightVec), 0.0)) * .7;
    float specular = 1.0;
    vec4 ISpecular = vec4(0.0);
    if (dot(inEyePos, inNormal) < 0.0)
    {
        ISpecular = vec4(0.9, 0.1, 0.1, 1.0) * pow(max(dot(Reflected, Eye), 0.0), 2.0) * specular; 
    }
    outFragColor = IAmbient + vec4((IDiffuse) * vec4(Color, 1.0) + ISpecular);
  
}