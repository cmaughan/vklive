#version 460
#extension GL_EXT_ray_tracing : enable
#extension GL_GOOGLE_include_directive : enable

//#include "default_parameters.h"

struct RayPayload 
{
    vec3 color;
    float distance;
    vec3 normal;
    float reflector;
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;

void main()
{
    const vec3 gradientStart = vec3(0.5, 0.6, 1.0);
    const vec3 gradientEnd = vec3(1.0);
    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    float t = 0.5 * (unitDir.y + 1.0);
    //rayPayload.color = (1.0-t) * gradientStart + t * gradientEnd;
    rayPayload.color = vec3(0.0f);
    rayPayload.distance = -1.0f;
    rayPayload.normal = vec3(0.0f);
    rayPayload.reflector = 0.0f;
}