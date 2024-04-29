#include "common.hlsli"
#include "color_space.hlsli"

RaytracingAccelerationStructure scene : register(t0, space0);
RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);


struct Vertex_Position_Normal_Tangent_Color_Texturecoord
{
    float3 position  : POSITION;
    float3 normal    : NORMAL;
    float3 color     : COLOR;
    float2 texCoord  : TEXCOORD;
    float4 tangent   : TANGENT;
};

struct Geometry_Info {
    uint   vertex_offset;
    uint   index_byte_offset;
    uint   material_id;
    uint   material_flags;
    // matrix model_matrix;
};

StructuredBuffer<Geometry_Info> geometry_info                                     : register(t0, ComputeSpace);
StructuredBuffer<Vertex_Position_Normal_Tangent_Color_Texturecoord> vertex_buffer : register(t1, ComputeSpace);
ByteAddressBuffer index_buffer : register(t2, ComputeSpace);

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
    float3 camera_center = float3(.0f,.0f,.0f);
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
    ray_direction = mul(float4(ray_direction, 1.), per_frame_data.view_matrix);
    // ray_direction = mul(per_frame_data.view_matrix, float4(ray_direction, 1.));
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
    TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);

    // payload.color *= 0.01;
    // payload.color += float4(0.8, 0.0, 0.0, 1.0);

    // Write the raytraced color to the output texture.
    float3 output_color = payload.color.rgb;
    // output_color = apply_reinhert(output_color);
    // output_color = apply_srgb_curve(output_color);
    texture_2d_uav_table[output_texture_index.texture_index][xy]= float4(output_color, 1);
    return;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload : SV_RayPayload, in MyAttributes attr)
{
    float3 barycentrics = float3(1 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    // float T = RayTCurrent() / 100.;
    // float g = 0.4;
    // float b = 0.5;
    // float GI = GeometryIndex();
    // float PI = PrimitiveIndex();
    // float r = 0;//GI / 200;
    // float g = PI / 10000;
    // float b = 0;
    // payload.color = normalize(float4(T, g, b, 1));

    uint geometry_index = GeometryIndex();
    Geometry_Info g_info = geometry_info[geometry_index];
    uint vertex_offset  = g_info.vertex_offset;

    // From MiniEngine Example
    uint primitive_offset_bytes = g_info.index_byte_offset + PrimitiveIndex() * 3 * 2;
    const uint dwordalignedoffset = primitive_offset_bytes & ~3;

    const uint2 four16bitIndicies = index_buffer.Load2(dwordalignedoffset);

    uint3 indicies;
    if(dwordalignedoffset == primitive_offset_bytes){
        indicies.x = four16bitIndicies.x & 0xffff;
        indicies.y = (four16bitIndicies.x >> 16) & 0xffff;
        indicies.z = four16bitIndicies.y & 0xffff;
    } else {
        indicies.x = (four16bitIndicies.x >> 16) & 0xffff;
        indicies.y = four16bitIndicies.y & 0xffff;
        indicies.z = (four16bitIndicies.y >> 16) & 0xffff;
    }

    Vertex_Position_Normal_Tangent_Color_Texturecoord vertex1 = vertex_buffer[indicies.x + g_info.vertex_offset];
    Vertex_Position_Normal_Tangent_Color_Texturecoord vertex2 = vertex_buffer[indicies.y + g_info.vertex_offset];
    Vertex_Position_Normal_Tangent_Color_Texturecoord vertex3 = vertex_buffer[indicies.z + g_info.vertex_offset];

    float2 uv_hit  = barycentrics.x * vertex1.texCoord + barycentrics.y * vertex2.texCoord + barycentrics.z * vertex3.texCoord;

    Texture2D albedo_texture = texture_2d_table[g_info.material_id * 3];

    float4 albedo_sample = albedo_texture.SampleLevel(sampler_1, uv_hit, 0);
    albedo_sample.w = 1.0;

    payload.color = albedo_sample;

}

[shader("miss")]
void MyMissShader(inout RayPayload payload : SV_RayPayload)
{
    payload.color = normalize(float4(0.3, 0.4, 0.6, 1));
}
