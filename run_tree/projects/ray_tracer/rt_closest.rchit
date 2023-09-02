#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_GOOGLE_include_directive : enable

#include "default_parameters.h"

struct RayPayload 
{
    vec3 color;
    float distance;
    vec3 normal;
    float reflector;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 1) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 1) buffer Vertices { vec4 v[]; } vertices;
layout(binding = 3, set = 1) buffer Indices { uint i[]; } indices;

void main()
{
    //ivec3 index = ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1], indices.i[3 * gl_PrimitiveID + 2]);
    const vec3 barycentricCoords = vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    rayPayload.color = barycentricCoords;
    rayPayload.distance = gl_RayTmaxEXT;
    rayPayload.normal = vec3(0.0,1.0,0.0);
    rayPayload.reflector = 0.0f;
}