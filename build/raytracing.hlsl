#include "common.hlsli"

RaytracingAccelerationStructure scene : register(t0, space0);
RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);

static float3 background_color = float3(0.4, 0.5, 0.3);
// static float3 camera_center = float3(0., 0., 0.);
static float3 up_dir = float4(0., 1., 0., 0.);
static float3 right_dir = float4(1., 0., 0., 0.);
static float  focal_length = 1.;

typedef BuiltInTriangleIntersectionAttributes MyAttributes;
struct RayPayload
{
    float4 color;
};
struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

[shader("raygeneration")]
void MyRaygenShader()
{
    float3 camera_center = per_frame_data.camera_pos.xyz;
    float viewport_height = 2.0;
    float viewport_width  = viewport_height * ((float)output_dimensions.width / (float)output_dimensions.height) ;
    float3 viewport_u = float3(viewport_width, 0, 0);
    float3 viewport_v = float3(0, -viewport_height, 0);
    float3 pixel_delta_u = viewport_u / output_dimensions.width;
    float3 pixel_delta_v = viewport_v / output_dimensions.height;

    float3 viewport_upper_left = camera_center - float3(0, 0, focal_length) - (viewport_u / 2) - (viewport_v / 2);
    float3 pixel00_loc = viewport_upper_left + 0.5*(pixel_delta_u + pixel_delta_v);

    uint2  xy_uint = uint2(DispatchRaysIndex().xy);
    float2 xy   = float2(xy_uint);

    float3 pixel_center = pixel00_loc + (xy.x * pixel_delta_u) + (xy.y * pixel_delta_v);
    float3 ray_direction = normalize(pixel_center - camera_center);
    // ray_direction.z = ray_direction.z;

    // Trace the ray.
    RayDesc ray;
    ray.Origin = per_frame_data.camera_pos.xyz;
    ray.Direction = ray_direction;
    // Set the ray's extents.
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.0;
    ray.TMax = 100000.0;
    RayPayload payload = { float4(0.8, 0.4, 0.6, 0) };
    // RayPayload payload;
    // payload.color = float4(0, 0, 0, 0);

    // // Get the location within the dispatched 2D grid of work items
    // // (often maps to pixels, so this could represent a pixel coordinate).
    // uint2 launchIndex = DispatchRaysIndex().xy;
    // float2 dims = float2(DispatchRaysDimensions().xy);
    // float2 d = (((launchIndex.xy + 0.5f) / dims.xy) * 2.f - 1.f );
    // // Define a ray, consisting of origin, direction, and the min-max distance
    // // values
    // RayDesc ray;
    // ray.Origin = float3(d.x, -d.y, 10);
    // ray.Direction = float3(0, 0, -1);
    // ray.TMin = 0;
    // ray.TMax = 10000;
    // scene;
    TraceRay(scene, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

    // payload.color *= 0.01;
    // payload.color += float4(0.8, 0.0, 0.0, 1.0);

    // Write the raytraced color to the output texture.
    texture_2d_uav_table[output_texture_index.texture_index][xy]= payload.color;
    return;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload, in MyAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    payload.color = float4(1, 0, 0, 1);
}

[shader("miss")]
void MyMissShader(inout RayPayload payload)
{
    payload.color = normalize(float4(0.3, 0.4, 0.6, 1));
}
