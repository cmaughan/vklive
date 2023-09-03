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
};

struct Vertex
{
    vec4 pos_uv_x;           // pos.x, pos.y, pos.z, uv.x
    vec4 uv_y_color_xyz;     // uv.y, color.x, color.y, color.z
    vec4 color_w_normal;     // color.w, normal.x, normal.y, normal.z
};

layout(location = 0) rayPayloadInEXT RayPayload rayPayload;
hitAttributeEXT vec2 attribs;

layout(binding = 0, set = 1) uniform accelerationStructureEXT topLevelAS;
layout(binding = 2, set = 1) buffer Vertices { Vertex v[]; }
vertices;
layout(binding = 3, set = 1) buffer Indices { uint i[]; }
indices;

void main()
{
    ivec3 index =
        ivec3(indices.i[3 * gl_PrimitiveID], indices.i[3 * gl_PrimitiveID + 1],
              indices.i[3 * gl_PrimitiveID + 2]);

    uint vertexBase = gl_InstanceCustomIndexEXT;    
    Vertex v0 = vertices.v[index.x + vertexBase];
    Vertex v1 = vertices.v[index.y + vertexBase];
    Vertex v2 = vertices.v[index.z + vertexBase];

    // Interpolate normal
    const vec3 barycentricCoords =
        vec3(1.0f - attribs.x - attribs.y, attribs.x, attribs.y);
    vec3 normal = normalize(v0.color_w_normal.yzw * barycentricCoords.x +
                            v1.color_w_normal.yzw * barycentricCoords.y +
                            v2.color_w_normal.yzw * barycentricCoords.z);

    vec3 unitDir = normalize(gl_WorldRayDirectionEXT);
    // Basic lighting
    float dot_product = max(dot(-unitDir, normal), 0.0);
    rayPayload.color = v0.uv_y_color_xyz.yzw * vec3(dot_product);
    rayPayload.distance = gl_RayTminEXT;
    rayPayload.normal = normal;

    // Objects with full white vertex color are treated as lights
    vec3 col = v2.uv_y_color_xyz.yzw;
    float light =
        ((col.r == 1.0f) && (col.g == 1.0f) && (col.b == 1.0f))
            ? 1.0f
            : 0.0f;
    if (light == 1.0f)
    {
        rayPayload.color = vec3(1.0);
    }
}