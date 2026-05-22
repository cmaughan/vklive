#include <metal_stdlib>
#include <metal_raytracing>

using namespace metal;
using namespace metal::raytracing;

struct VklPassChannel
{
    float4 resolution;
    float time;
};

struct VklPassUBO
{
    float iTime;
    float iGlobalTime;
    float iTimeDelta;
    float iFrame;
    float iFrameRate;
    float iSampleRate;
    uint iSceneFlags;
    uint vertexSize;
    float4 iResolution;
    float4 iMouse;
    float4 iDate;
    float4 iSpectrumBands[2];
    float4 iChannelTime;
    float4 iChannelResolution[4];
    float4 ifFragCoordOffsetUniform;
    float4 eye;
    float4x4 model;
    float4x4 view;
    float4x4 projection;
    float4x4 modelViewProjection;
    float4x4 viewInverse;
    float4x4 projectionInverse;
    VklPassChannel iChannel[4];
};

static float4 color_at(const device float* vertices, uint vertexIndex, uint strideFloats)
{
    const device float* v = vertices + vertexIndex * strideFloats;
    return float4(v[5], v[6], v[7], v[8]);
}

static float3 normal_at(const device float* vertices, uint vertexIndex, uint strideFloats)
{
    const device float* v = vertices + vertexIndex * strideFloats;
    return normalize(float3(v[9], v[10], v[11]));
}

kernel void vklive_ray_trace(texture2d<float, access::write> outImage [[texture(0)]],
    instance_acceleration_structure sceneAS [[buffer(0)]],
    constant VklPassUBO& ubo [[buffer(1)]],
    const device float* vertices [[buffer(2)]],
    const device uint* indices [[buffer(3)]],
    uint2 tid [[thread_position_in_grid]])
{
    const uint width = outImage.get_width();
    const uint height = outImage.get_height();
    if (tid.x >= width || tid.y >= height)
    {
        return;
    }

    const float2 uv = (float2(tid) + 0.5f) / float2(width, height);
    const float2 ndc = uv * 2.0f - 1.0f;
    const float4 viewTarget = ubo.projectionInverse * float4(ndc, 1.0f, 1.0f);
    const float3 viewDirection = normalize(viewTarget.xyz / viewTarget.w);
    const float3 worldDirection = normalize((ubo.viewInverse * float4(viewDirection, 0.0f)).xyz);

    ray ray;
    ray.origin = ubo.eye.xyz;
    ray.direction = worldDirection;
    ray.min_distance = 0.001f;
    ray.max_distance = 10000.0f;

    intersector<triangle_data, instancing> intersector;
    intersection_result<triangle_data, instancing> hit = intersector.intersect(ray, sceneAS, 0xff);
    if (hit.type == intersection_type::none)
    {
        outImage.write(float4(0.0f, 0.0f, 0.0f, 1.0f), tid);
        return;
    }

    const uint triangleIndex = hit.primitive_id;
    const uint i0 = indices[triangleIndex * 3 + 0];
    const uint i1 = indices[triangleIndex * 3 + 1];
    const uint i2 = indices[triangleIndex * 3 + 2];
    const uint strideFloats = ubo.vertexSize / 4;

    const float2 bary = hit.triangle_barycentric_coord;
    const float w0 = 1.0f - bary.x - bary.y;
    const float w1 = bary.x;
    const float w2 = bary.y;

    const float4 vertexColor = color_at(vertices, i0, strideFloats) * w0 +
        color_at(vertices, i1, strideFloats) * w1 +
        color_at(vertices, i2, strideFloats) * w2;

    const float3 n = normalize(normal_at(vertices, i0, strideFloats) * w0 +
        normal_at(vertices, i1, strideFloats) * w1 +
        normal_at(vertices, i2, strideFloats) * w2);
    const float diffuse = saturate(dot(abs(n), normalize(float3(0.3f, 0.6f, 0.7f)))) * 0.75f + 0.25f;
    outImage.write(float4(vertexColor.rgb * diffuse, 1.0f), tid);
}
