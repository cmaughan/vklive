#version 450
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

layout (triangles) in;
layout (line_strip, max_vertices = 6) out;
layout (location = 0) in vec3 inNormal[];
layout (location = 1) in vec3 inColor[];
layout (location = 2) in vec3 inEyePos[];
layout (location = 3) in vec3 inLightVec[];

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec3 outEyePos;
layout (location = 3) out vec3 outLightVec;

void main(void)
{
    // Convert input triangle vertices to normals
    float normal_length = 1.25;
    for(int i = 0; i < gl_in.length(); i++)
    {
        gl_Position = gl_in[i].gl_Position;
        outNormal = inNormal[i].xyz;
        outColor = inColor[i].xyz;
        outEyePos = inEyePos[i].xyz;
        outLightVec = inLightVec[i].xyz;
        EmitVertex();

        gl_Position = gl_in[i].gl_Position + vec4(inNormal[i].xyz * normal_length, 0.0);
        outNormal = inNormal[i].xyz;
        outColor = inColor[i].xyz;
        outEyePos = inEyePos[i].xyz;
        outLightVec = inLightVec[i].xyz;
        EmitVertex();

        EndPrimitive();
    }

}