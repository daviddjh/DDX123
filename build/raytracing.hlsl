#define PFD
#include "common.hlsli"
#include "color_space.hlsli"

RaytracingAccelerationStructure scene : register(t0, space0);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);
ConstantBuffer<Texture_Index>   texture_array_begin : register(b2, ComputeSpace);


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
    uint   hit;
    float  beta;
};
struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

// from: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
uint pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

float3 EnvMap_ImageLe(float2 uv){
    
    Texture2D environment_texture = texture_2d_table[ /* env texture index */ ];
    return environment_texture.SampleLevel(sampler_1, uv, 0);
}

float copy_sign(float num, float sign_num){
    num = num * sign(sign_num);
    return num;
}

// From: https://www.pbr-book.org/4ed/Geometry_and_Transformations/Spherical_Geometry
float3 sphere_coord_to_square_coord(float3 sphere_coords){

    float x = abs(sphere_coords.x);
    float y = abs(sphere_coords.y);
    float z = abs(sphere_coords.z);

    // compute radius r
    float r = max(0, sqrt(1 - z));

    // compute argument to atan
    float a = max(x, y);
    float b = min(x, y);
    b = a == 0 ? 0 : b / a;

    // SLOW - pbr book used a polynomial aproximation
    float phi = atan(b)*2/PI;

    // Extenc phi if input is in the range 45 - 90 degrees
    if(x < y){
        phi = 1 - phi;
    }

    float v = phi * r;
    float u = r - v;

    // if coords in southern hemisphere, mirror u,v
    if (sphere_coords.z < 0){
        float temp = u;
        u = v;
        v = temp;
        u = 1 - u;
        v = 1 - v;
    }

    u = copy_sign(u, sphere_coords.x);
    v = copy_sign(v, sphere_coords.y);

    // Transform from [-1,1] to [0,1]
    return float2(0.5 * (u + 1), 0.5 * (v + 1));
}

// From: https://www.pbr-book.org/4ed/Geometry_and_Transformations/Spherical_Geometry
float3 square_coord_to_sphere_coord(float2 square_coords){

    // convert to [-1, 1], then compute abs
    float u = square_coords.x * 2 - 1;
    float v = square_coords.y * 2 - 1;

    float up = abs(u);
    float vp = abs(v); 

    // Compute radius r for square to sphere mapping
    float signed_distance = 1 - (up + vp);
    float d = abs(signed_distance);
    float r = 1 - d;

    // Compute phi for square to sphere mapping. accunts for the 45deg rotation
    float phi = ( r == 0 ? 1 : (vp - up) / r + 1) * PI / 4;

    float z = copy_sign(1 - sqrt(r), signed_distance);
    float cos_phi = copy_sign(cos(phi), u);
    float sin_phi = copy_sign(sin(phi), v);
    return(cos_phi * r * max(0, sqrt(2 - sqrt(r))),
           sin_phi * r * max(0, sqrt(2 - sqrt(r))), z);
}

float3 EnvMap_SampleLi(float3 current_point, float2 u){
    // TODO: sample uv from a distribution over the image:
    float map_PDF = 1;
    // uv = distribution.sample(u, &mapPDF);
    float2 uv = u;
    float3 w_light = square_coord_to_sphere_coord(uv);
    // float3 wi = render_from_light(w_light);  // This is where we would transform the unit vector from light space to "render" space
    float3 wi = w_light;
    float pdf = map_PDF / (4 * PI);

    // Create Ray
    RayDesc ray;
    ray.Origin    = current_point.xyz;
    ray.Direction = wi;
    ray.TMin = 0.001;
    ray.TMax = 100000.0;

    // Trace the bound scene with ray created above
    RayPayload payload = { float4(0.0, 0.0, 0.0, 0), 0};
    TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);

}

float3 EnvMap_Le(float3 ray_direction){
    float2 uv = sphere_coord_to_square_coord(ray_direction);
    return EnvMap_ImageLe(uv);
}

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

RayDesc create_camera_ray(uint2 pixel_xy){

    float3 camera_center = float3(.0f,.0f,.0f);
    float viewport_height = 2.0;
    float viewport_width  = viewport_height * ((float)output_dimensions.width / (float)output_dimensions.height) ;
    float3 viewport_u = float3(viewport_width, 0, 0);
    float3 viewport_v = float3(0, -viewport_height, 0);
    float3 pixel_delta_u = viewport_u / output_dimensions.width;
    float3 pixel_delta_v = viewport_v / output_dimensions.height;

    float3 viewport_upper_left = camera_center - float3(0, 0, focal_length) - (viewport_u / 2) - (viewport_v / 2);
    float3 pixel00_loc = viewport_upper_left + 0.5*(pixel_delta_u + pixel_delta_v);

    uint2  xy_uint = uint2(pixel_xy);
    float2 xy   = float2(xy_uint);

    float3 pixel_center = pixel00_loc + (xy.x * pixel_delta_u) + (xy.y * pixel_delta_v);
    float3 ray_direction = normalize(pixel_center - camera_center);
    ray_direction = mul(float4(ray_direction, 1.), per_frame_data.view_matrix);
    ray_direction = normalize(ray_direction);
    // ray_direction = mul(per_frame_data.view_matrix, float4(ray_direction, 1.));
    // ray_direction.z = ray_direction.z;

    // Trace the ray.
    RayDesc ray;
    ray.Origin = per_frame_data.camera_pos.xyz;
    ray.Direction = ray_direction;
    // Set the ray's extents.
    // Set TMin to a non-zero small value to avoid aliasing issues due to floating - point errors.
    // TMin should be kept small to prevent missing geometry at close contact areas.
    ray.TMin = 0.01;
    ray.TMax = 100000.0;
    return ray;

}

[shader("raygeneration")]
void MyRaygenShader()
{

    // Get Screen Pixel
    uint2 pixel_xy = DispatchRaysIndex().xy;
    
    // Create Ray from camera
    RayDesc ray = create_camera_ray(pixel_xy);

    // Beginning ray payload ( starting color )
    RayPayload payload = { float4(0.8, 0.4, 0.6, 0) };

    // Trace the bound scene with ray created above
    TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);

    // Write the raytraced color to the output texture.
    float3 output_color = payload.color.rgb;
    texture_2d_uav_table[output_texture_index.texture_index][pixel_xy]= float4(output_color, 1);

    return;
}

[shader("closesthit")]
void MySimplePathTracer(inout RayPayload payload : SV_RayPayload, in MyAttributes attr){

    // Account for emissive surface if light was not sampled
    // End Path if maximum depth rendered
    // Get BSDF and skip over medium boundries
    // Sample direct illumination if sample lights is true
    // Sample outgoing direction at intersecion to continue path

}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload : SV_RayPayload, in MyAttributes attr)
{

    payload.hit = 1;

    float3 barycentrics = float3(1.f - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

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
    vertex1.position *= .1f;
    Vertex_Position_Normal_Tangent_Color_Texturecoord vertex2 = vertex_buffer[indicies.y + g_info.vertex_offset];
    vertex2.position *= .1f;
    Vertex_Position_Normal_Tangent_Color_Texturecoord vertex3 = vertex_buffer[indicies.z + g_info.vertex_offset];
    vertex3.position *= .1f;

    float2 uv_hit  = barycentrics.x * vertex1.texCoord + barycentrics.y * vertex2.texCoord + barycentrics.z * vertex3.texCoord;
    float3 p_hit   = barycentrics.x * vertex1.position + barycentrics.y * vertex2.position + barycentrics.z * vertex3.position;
    float3 n_hit   = barycentrics.x * vertex1.normal   + barycentrics.y * vertex2.normal   + barycentrics.z * vertex3.normal;
    n_hit = normalize(n_hit);

    Texture2D albedo_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + g_info.material_id * 3)];
    Texture2D normal_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + g_info.material_id * 3 + 1)];
    Texture2D roughness_metallic_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + g_info.material_id * 3 + 2)];

    /////////////////////////////////////////////
    //
    // Ray Differentials
    //
    /////////////////////////////////////////////

    // Estimate partial derivitive of world space position w/r/t u and v texture coords
    // https://www.pbr-book.org/4ed/Shapes/Triangle_Meshes#RayndashTriangleIntersection
    float3 dpdu, dpdv;

    float2 duv02 = vertex1.texCoord - vertex3.texCoord;
    float2 duv12 = vertex2.texCoord - vertex3.texCoord;

    float3 dp02 = vertex1.position - vertex3.position;
    float3 dp12 = vertex2.position - vertex3.position;

    float determinant = duv02.x * duv12.y - duv02.y * duv12.x;

    float invdet = 1 / determinant;
    dpdu = (duv12.y * dp02 - duv02.y * dp12) * invdet;
    dpdv = (duv02.x * dp12 - duv12.x * dp02) * invdet;

    // Estimate partial derivitive of world space position w/r/t screen space coords
    // https://www.pbr-book.org/4ed/Textures_and_Materials/Texture_Sampling_and_Antialiasing#FindingtheTextureSamplingRate
    uint2 current_pixel_xy = DispatchRaysIndex().xy;
    RayDesc rx = create_camera_ray(uint2(current_pixel_xy.x + 1, current_pixel_xy.y));
    RayDesc ry = create_camera_ray(uint2(current_pixel_xy.x, current_pixel_xy.y - 1));

    float  d  = -dot(n_hit, p_hit);
    float  tx = (-dot(n_hit, rx.Origin) - d) / dot(n_hit, rx.Direction);  // I think something is broken here?
    // tx *= 0.01f;
    // float tx = RayTCurrent();
    float3 px = rx.Origin + tx * rx.Direction;

    float  ty = (-dot(n_hit, ry.Origin) - d) / dot(n_hit, ry.Direction);  
    // float ty = RayTCurrent();
    // ty *= 0.01f;
    float3 py = ry.Origin + ty * ry.Direction;

    float3 dpdx = px - p_hit;
    float3 dpdy = py - p_hit;

    // Find partial derivitive of (u,v) texture coords w/r/t (x, y) screen space coords
    // https://www.pbr-book.org/4ed/Textures_and_Materials/Texture_Sampling_and_Antialiasing#FindingtheTextureSamplingRate

    float ata00 = dot(dpdu, dpdu);
    float ata01 = dot(dpdu, dpdv);
    float ata11 = dot(dpdv, dpdv);

    invdet = 1 / (ata00 * ata11 - ata01 * ata01);
    invdet = isfinite(invdet) ? invdet : 0.f;     // If equation cannot be solved, then set invdet to zero. This leads to point sampled textures.

    float atb0x = dot(dpdu, dpdx);
    float atb1x = dot(dpdv, dpdx);

    float atb0y = dot(dpdu, dpdy);
    float atb1y = dot(dpdv, dpdy);

    float dudx = (ata11 * atb0x - ata01 * atb1x) * invdet;
    float dvdx = (ata00 * atb1x - ata01 * atb0x) * invdet;
    float dudy = (ata11 * atb0y - ata01 * atb1y) * invdet;
    float dvdy = (ata00 * atb1y - ata01 * atb0y) * invdet;

    dudx = isfinite(dudx) ? clamp(dudx, -1e8f, 1e8f) : 0;
    dvdx = isfinite(dvdx) ? clamp(dvdx, -1e8f, 1e8f) : 0;
    dudy = isfinite(dudy) ? clamp(dudy, -1e8f, 1e8f) : 0;
    dvdy = isfinite(dvdy) ? clamp(dvdy, -1e8f, 1e8f) : 0;

    float2 ddx = float2(dudx, dvdx);
    float2 ddy = float2(dudy, dvdy);

    float4 albedo_sample = albedo_texture.SampleGrad(sampler_1, uv_hit, ddx, ddy);

    payload.color = albedo_sample;

}

[shader("miss")]
void MyMissShader(inout RayPayload payload : SV_RayPayload)
{
    float3 ray = WorldRayDirection();
    payload.color += payload.beta * EnvMap_Le(ray);
}
