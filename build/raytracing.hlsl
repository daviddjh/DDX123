#define PFD
#include "common.hlsli"
#include "math.hlsli"
#include "utils.hlsli"
#include "bxdf.hlsli"
#include "color_space.hlsli"

RaytracingAccelerationStructure scene : register(t0, space0);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);
ConstantBuffer<Texture_Index>   texture_array_begin : register(b2, ComputeSpace);
ConstantBuffer<Texture_Index>   env_map_index       : register(b3, ComputeSpace);
ConstantBuffer<Texture_Index>   random_tex_index    : register(b4, ComputeSpace);

ConstantBuffer<Texture_Index>   env_conditional_cdfs_index       : register(b5, ComputeSpace);
ConstantBuffer<Texture_Index>   env_luminance_distribution_index : register(b6, ComputeSpace);

Buffer<float> env_marginal_cdf                            : register(t1, ComputeSpace);
Buffer<float> env_full_conditional_distribution_integrals : register(t2, ComputeSpace);

ConstantBuffer<Env_Map_Importance_Sample_Info> env_importance_sample_info : register(b9, ComputeSpace);

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

StructuredBuffer<Geometry_Info> geometry_info                                     : register(t3, ComputeSpace);
StructuredBuffer<Vertex_Position_Normal_Tangent_Color_Texturecoord> vertex_buffer : register(t4, ComputeSpace);
ByteAddressBuffer index_buffer : register(t5, ComputeSpace);

static float3 background_color = float3(0.4, 0.5, 0.3);
// static float3 camera_center = float3(0., 0., 0.);
static float3 up_dir = float4(0., 1., 0., 0.);
static float3 right_dir = float4(1., 0., 0., 0.);
static float  focal_length = 1.;

static float scene_radius = 1000;

typedef BuiltInTriangleIntersectionAttributes MyAttributes;

struct RayPayload
{
    float4 color;
    float3  beta;
    uint   just_hit;
    uint   recursion_depth;
    uint   random_u;
    float  p_b;
};
struct Viewport
{
    float left;
    float top;
    float right;
    float bottom;
};

struct Light_Sample {
    float3 L;
    float pdf;
    float3 wi;
    float3 p;
    uint occluded;
};

float3 EnvMap_ImageLe(float2 uv){
    
    Texture2D environment_texture = texture_2d_table[env_map_index.texture_index];
    return environment_texture.SampleLevel(sampler_1, uv, 0);
}

float2 sample_env_luminance_distribution(float2 u, inout float map_PDF){

    uint2 fxy = uint2(-1, -1);

    float marg_pdf = -1;
    float cond_pdf = -1;

    // Sample from Marginal CDF, find x coord

    for(uint i = 0; i < (env_importance_sample_info.width + 1); i++){
        if (u.x >= env_marginal_cdf.Load(i)){
            fxy.x = i-1;
        }
    }
    if (env_importance_sample_info.full_marginal_distribution_integral > 0){
        marg_pdf = env_full_conditional_distribution_integrals.Load(fxy.x) / env_importance_sample_info.full_marginal_distribution_integral;
    } else {
        marg_pdf = 0.;
    }

    // Sample Conditional CDF, find y coord

    Texture2D<float> env_conditional_cdfs = texture_2d_float_table[env_conditional_cdfs_index.texture_index];
    for(uint j = 0; j < (env_importance_sample_info.height + 1); j++){
        if (u.y >= env_conditional_cdfs.Load(uint3(fxy.x, j, 0))){
            fxy.y = j - 1;
        }
    }

    Texture2D<float> env_luminance_distribution = texture_2d_float_table[env_luminance_distribution_index.texture_index];
    cond_pdf = env_luminance_distribution.Load(uint3(fxy, 0)) / env_full_conditional_distribution_integrals.Load(fxy.x);

    // Calculate PDF value
    map_PDF = marg_pdf * cond_pdf;

    // Return sampled point
    return (float2(fxy.x, fxy.y) / float2(env_importance_sample_info.width, env_importance_sample_info.height));
    //return u;
}

Light_Sample EnvMap_SampleLi(Hit_Info hit_info, float2 u){

    Light_Sample light_sample;

    // TODO: sample uv from a distribution over the image:
    float map_PDF = 1;
    
    // Sample the luminance PDF. This ensures samples are mostly taken from bright areas.
    // float2 uv = sample_env_luminance_distribution(u, map_PDF);
    // map_PDF = 1;

    // Just a random sample, optionally forced to face upwards
    float2 uv = u;
    float3 w_light = square_coord_to_sphere_coord(uv);

    // // Flip ray to only face upwards
    w_light.y = abs(w_light.y);
    uv = sphere_coord_to_square_coord(w_light);

    // float3 wi = render_from_light(w_light);  // This is where we would transform the unit vector from light space to "render" space
    float3 wi = w_light;
    float pdf = map_PDF / (4 * PI);
    pdf *= 2;

    // Compute ray origin offset
    float3 offset = float3(0.001, 0.001, 0.001) * hit_info.n;
    if(dot(wi,hit_info.n) < 0){
        offset = -offset;
    }

    // Create Ray
    RayDesc ray;
    ray.Origin = hit_info.p.xyz + offset;
    ray.Direction = wi;
    ray.TMin = 0.001;
    ray.TMax = 100000.0;


    // Trace the bound scene with ray created above
    RayPayload payload; //= { float4(1.0, 0.0, 0.0, 0), 0, 1};
    payload.color = float4(1.0, 0.0, 0.0, 0);
    payload.beta = 0;
    payload.just_hit = 1;
    payload.recursion_depth = 0;
    TraceRay(scene, RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);

    if (payload.color.x == 0){
        light_sample.L = EnvMap_ImageLe(uv);
        light_sample.p = hit_info.p.xyz + (wi * 2 * scene_radius);
        light_sample.pdf = pdf;
        light_sample.wi = wi;
        light_sample.occluded = 0;
    } else {
        light_sample.occluded = 1;
    }

    return light_sample;

}

float3 EnvMap_Le(float3 ray_direction){
    float2 uv = sphere_coord_to_square_coord(ray_direction);
    // uv = -uv;
    return EnvMap_ImageLe(uv);
}

bool IsInsideViewport(float2 p, Viewport viewport)
{
    return (p.x >= viewport.left && p.x <= viewport.right)
        && (p.y >= viewport.top && p.y <= viewport.bottom);
}

RayDesc create_camera_ray(uint2 pixel_xy, inout uint random_u){

    float3 camera_center = float3(.0f,.0f,.0f);
    float viewport_height = 2.0;
    float viewport_width  = viewport_height * ((float)output_dimensions.width / (float)output_dimensions.height) ;
    float3 viewport_u = float3(viewport_width, 0, 0);
    float3 viewport_v = float3(0, -viewport_height, 0);
    float3 pixel_delta_u = viewport_u / output_dimensions.width;
    float3 pixel_delta_v = viewport_v / output_dimensions.height;
    float3 u_rand = get_rand_float3(random_u);
    float3 v_rand = get_rand_float3(random_u);
    u_rand *= pixel_delta_u;
    v_rand *= pixel_delta_v;

    float3 viewport_upper_left = camera_center - float3(0, 0, focal_length) - (viewport_u / 2) - (viewport_v / 2);
    float3 pixel00_loc = viewport_upper_left + 0.5*(pixel_delta_u + pixel_delta_v);

    uint2  xy_uint = uint2(pixel_xy);
    float2 xy   = float2(xy_uint);

    float3 pixel_center = pixel00_loc + ((xy.x * pixel_delta_u) + u_rand) + ((xy.y * pixel_delta_v) + v_rand);
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


Hit_Info get_hit_info(float2 barycentrics_2) {
    float3 barycentrics = float3(1.f - barycentrics_2.x - barycentrics_2.y, barycentrics_2.x, barycentrics_2.y);

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

    Hit_Info hit_info;
    hit_info.uv = barycentrics.x * vertex1.texCoord + barycentrics.y * vertex2.texCoord + barycentrics.z * vertex3.texCoord;
    hit_info.p  = barycentrics.x * vertex1.position + barycentrics.y * vertex2.position + barycentrics.z * vertex3.position;
    hit_info.n  = barycentrics.x * vertex1.normal   + barycentrics.y * vertex2.normal   + barycentrics.z * vertex3.normal;
    hit_info.n  = normalize(hit_info.n);
    float4 tangent  = barycentrics.x * vertex1.tangent + barycentrics.y * vertex2.tangent + barycentrics.z * vertex3.tangent;
    hit_info.t = tangent.xyz;
    hit_info.t_handedness = tangent.w;
    hit_info.t  = normalize(hit_info.t);
    hit_info.material_id = g_info.material_id;

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
    uint rand = 0;
    RayDesc rx = create_camera_ray(uint2(current_pixel_xy.x + 1, current_pixel_xy.y), rand);
    RayDesc ry = create_camera_ray(uint2(current_pixel_xy.x, current_pixel_xy.y - 1), rand);

    float  d  = -dot(hit_info.n, hit_info.p);
    float  tx = (-dot(hit_info.n, rx.Origin) - d) / dot(hit_info.n, rx.Direction);  // I think something is broken here?
    // tx *= 0.01f;
    // float tx = RayTCurrent();
    float3 px = rx.Origin + tx * rx.Direction;

    float  ty = (-dot(hit_info.n, ry.Origin) - d) / dot(hit_info.n, ry.Direction);  
    // float ty = RayTCurrent();
    // ty *= 0.01f;
    float3 py = ry.Origin + ty * ry.Direction;

    float3 dpdx = px - hit_info.p;
    float3 dpdy = py - hit_info.p;

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

    hit_info.ddx = float2(dudx, dvdx);
    hit_info.ddy = float2(dudy, dvdy);

    return hit_info;
}


[shader("closesthit")]
void MyPathTracer(inout RayPayload payload : SV_RayPayload, in MyAttributes attr){

    if(payload.just_hit == 1) {
        payload.color.x = 1; 
        return;
    }

    payload.recursion_depth++;

    float2 u; // Random Vector -> TODO
    float3 ray_index = DispatchRaysIndex();
    u.xy = get_rand_float3(payload.random_u).xy;

    float L = 0;
    float beta = payload.beta;

    // Account for emissive surface if light was not sampled
    // End Path if maximum depth rendered
    // Get BSDF and skip over medium boundries
    // Sample direct illumination if sample lights is true

    float3 ray_direction = WorldRayDirection();
    float3 ray_origin    = WorldRayOrigin();
    float3 ray_hit_point = ray_origin + (RayTCurrent() * ray_direction);

    float3 wo = -ray_direction;

    Hit_Info hit_info = get_hit_info(attr.barycentrics);

    Texture2D albedo_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3)];
    Texture2D normal_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 1)];
    Texture2D roughness_metallic_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 2)];

    float4 albedo_sample = albedo_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy);
    float3 normal_sample = normal_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy).xyz;
    normal_sample = normalize((2*normal_sample) - 1);
    float4 roughness_metallic_sample = roughness_metallic_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy);
    float metallic = roughness_metallic_sample.b;
    float roughness = roughness_metallic_sample.g;//1 - roughness_metallic_sample.g;//pow(roughness_metallic_sample.g, 2);

    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 w_Per_Vertex_Normal  = hit_info.n;
    float3 w_Per_Vertex_Tangent = hit_info.t;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -hit_info.t_handedness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );

    //hit_info.wn = mul(TBN, normal_sample.xyz);
    hit_info.wn = mul(normal_sample.xyz, TBN);
    float3 delta = hit_info.wn - hit_info.n;
    hit_info.n = hit_info.wn;
    //hit_info.t += delta;

    Light_Sample env_light_sample = EnvMap_SampleLi(hit_info, u);  
    float light_sample_bxdf_pdf = 0.0;

    u.xy = get_rand_float3(payload.random_u).xy;
    if(u.x > 0.5){
        light_sample_bxdf_pdf = BxDF_TS_pdf(wo, env_light_sample.wi, roughness);
    } else {
        light_sample_bxdf_pdf = BxDF_diffuse_pdf(wo, env_light_sample.wi);
    }

    float env_light_MIS_weight = power_huristic(1, env_light_sample.pdf, 1, light_sample_bxdf_pdf);

    float F;
    float3 f = BxDF_CT_f(wo, env_light_sample.wi, albedo_sample.rgb, hit_info, metallic, roughness) * abs(dot(env_light_sample.wi, hit_info.wn));//);

    // TODO.....
    if(!env_light_sample.occluded)
        payload.color.rgb += payload.beta * (f * env_light_sample.L * env_light_MIS_weight) / (env_light_sample.pdf);;

    // Sample outgoing direction at intersecion to continue path
    BSDF_Sample bsdf_sample;
    if(u.y > 0.5){
        u.xy = get_rand_float3(payload.random_u).xy;
        bsdf_sample = BxDF_TS_sample_f(wo, albedo_sample.rgb, u, hit_info, metallic, roughness);
    } else {
        u.xy = get_rand_float3(payload.random_u).xy;
        bsdf_sample = BxDF_diffuse_sample_f(wo, albedo_sample.rgb, u, hit_info);
    }

    payload.beta *= bsdf_sample.sampled_light * abs(dot(bsdf_sample.wi, hit_info.wn) / bsdf_sample.pdf );
    payload.p_b = bsdf_sample.pdf;

    // Compute ray origin offset
    float3 offset = float3(0.001, 0.001, 0.001) * hit_info.n;
    if(dot(bsdf_sample.wi,hit_info.n) < 0){
        offset = -offset;
    }

    // Create and trace new ray
    RayDesc ray;
    ray.Origin = ray_hit_point + offset;
    ray.Direction = bsdf_sample.wi;
    ray.TMin = 0.0001;
    ray.TMax = 100000.0;

    // Russian Roulette
    u.x = pcg_hash_prng(payload.random_u);
    float max_value = max(payload.beta.r, max(payload.beta.g, payload.beta.b));
    if(max_value < 1.0 && payload.recursion_depth > 1){
        float q = max(0, 1-max_value);
        if (u.x < q){
            return;
        }
        beta /= 1 - q;
    }

    // Trace the bound scene with ray created above
    if(((payload.beta.x > 0.0 && payload.beta.y > 0.0 && payload.beta.z > 0.0)) && payload.recursion_depth < 1)
        TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);
    return;

}

[shader("closesthit")]
void MySimplePathTracer(inout RayPayload payload : SV_RayPayload, in MyAttributes attr){

    if(payload.just_hit == 1) {
        payload.color.x = 1; 
        return;
    }

    payload.recursion_depth++;

    float2 u; // Random Vector -> TODO
    float3 ray_index = DispatchRaysIndex();
    u.xy = get_rand_float3(payload.random_u).xy;

    float L = 0;
    float beta = payload.beta;

    // Account for emissive surface if light was not sampled
    // End Path if maximum depth rendered
    // Get BSDF and skip over medium boundries
    // Sample direct illumination if sample lights is true

    float3 ray_direction = WorldRayDirection();
    float3 ray_origin    = WorldRayOrigin();
    float3 ray_hit_point = ray_origin + (RayTCurrent() * ray_direction);

    float3 wo = -ray_direction;

    Hit_Info hit_info = get_hit_info(attr.barycentrics);

    Texture2D albedo_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3)];
    Texture2D normal_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 1)];
    Texture2D roughness_metallic_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 2)];

    float4 albedo_sample = albedo_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy);
    float3 normal_sample = normal_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy).xyz;
    normal_sample = normalize((2*normal_sample) - 1);
    float4 roughness_metallic_sample = roughness_metallic_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy);
    float metallic = roughness_metallic_sample.b;
    float roughness = roughness_metallic_sample.g;//1 - roughness_metallic_sample.g;//pow(roughness_metallic_sample.g, 2);

    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 w_Per_Vertex_Normal  = hit_info.n;
    float3 w_Per_Vertex_Tangent = hit_info.t;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -hit_info.t_handedness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );

    //hit_info.wn = mul(TBN, normal_sample.xyz);
    hit_info.wn = mul(normal_sample.xyz, TBN);
    float3 delta = hit_info.wn - hit_info.n;
    hit_info.n = hit_info.wn;
    //hit_info.t += delta;

    Light_Sample env_light_sample = EnvMap_SampleLi(hit_info, u);  

    float F;
    float3 f = BxDF_CT_f(wo, env_light_sample.wi, albedo_sample.rgb, hit_info, metallic, roughness) * abs(dot(env_light_sample.wi, hit_info.wn));//);

    if(!env_light_sample.occluded)
        payload.color.rgb += (f * payload.beta * env_light_sample.L) / (1 * env_light_sample.pdf);;

    // Sample outgoing direction at intersecion to continue path
    u.xy = get_rand_float3(payload.random_u).xy;
    BSDF_Sample bsdf_sample;
    if(u.x > 0.5){
        u.xy = get_rand_float3(payload.random_u).xy;
        bsdf_sample = BxDF_TS_sample_f(wo, albedo_sample.rgb, u, hit_info, metallic, roughness);
    } else {
        u.xy = get_rand_float3(payload.random_u).xy;
        bsdf_sample = BxDF_diffuse_sample_f(wo, albedo_sample.rgb, u, hit_info);
    }

    payload.beta *= bsdf_sample.sampled_light * abs(dot(bsdf_sample.wi, hit_info.wn) / bsdf_sample.pdf );

    // Create and trace new ray
    RayDesc ray;
    ray.Origin = ray_hit_point;
    ray.Direction = bsdf_sample.wi;
    ray.TMin = 0.0001;
    ray.TMax = 100000.0;

    // Trace the bound scene with ray created above
    if(((payload.beta.x > 0.0 && payload.beta.y > 0.0 && payload.beta.z > 0.0)) && payload.recursion_depth < 3)
        TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);
    return;

}

[shader("raygeneration")]
void MyRaygenShader()
{

    // Get Screen Pixel
    uint2 pixel_xy = DispatchRaysIndex().xy;
    uint2 texture_loc = pixel_xy % 32;

    Texture2D<uint> random_tex = texture_2d_uint_table[random_tex_index.texture_index];
    uint random_u = random_tex.Load(float3(texture_loc, 0));
    random_u *= pixel_xy.x * pixel_xy.y;
    
    
    // Beginning ray payload ( starting color )
    RayPayload payload; //  = { float4(0.0, 0.0, 0.0, 0), 1.0, 0  };
    payload.color = float4(0.0, 0.0, 0.0, 0);
    payload.random_u = random_u;

    static const uint SAMPLE_COUNT = 25;
    RayDesc ray;
    for(uint i = 0; i < SAMPLE_COUNT; i++){

        // Create Ray from camera
        ray = create_camera_ray(pixel_xy, payload.random_u);
        payload.beta = float3(1.0, 1.0, 1.0);
        payload.just_hit = 0;
        payload.recursion_depth = 0;
        TraceRay(scene, RAY_FLAG_NONE /*RAY_FLAG_CULL_BACK_FACING_TRIANGLES*/, 0xFF, 0, 0, 0, ray, payload);

    }

    // Write the raytraced color to the output texture.
    float3 output_color = payload.color.rgb / SAMPLE_COUNT;
    texture_2d_uav_table[output_texture_index.texture_index][pixel_xy]= float4(output_color, 1);

    return;
}

[shader("closesthit")]
void MyClosestHitShader(inout RayPayload payload : SV_RayPayload, in MyAttributes attr)
{

    Hit_Info hit_info = get_hit_info(attr.barycentrics);

    Texture2D albedo_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3)];
    Texture2D normal_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 1)];
    Texture2D roughness_metallic_texture = texture_2d_table[NonUniformResourceIndex(texture_array_begin.texture_index + hit_info.material_id * 3 + 2)];

    float4 albedo_sample = albedo_texture.SampleGrad(sampler_1, hit_info.uv, hit_info.ddx, hit_info.ddy);

    payload.color = albedo_sample;

}

[shader("miss")]
void MyMissShader(inout RayPayload payload : SV_RayPayload)
{
    if(payload.just_hit == 1) {
        payload.color.x = 0; 
        return;
    }

    float3 ray = WorldRayDirection();
    if(payload.recursion_depth == 0){

        payload.color.rgb += payload.beta * EnvMap_Le(ray);

    } else {

        float p_l = 
        /* Probability of picking this light is 1.0 */                    1.0 * 
        /* PDF of sphere is 1/4PI. We use half of sphere though, so *2 */ (2 / (4 * PI) );

        float w_b = power_huristic(1., payload.p_b, 1., p_l);
        payload.color.rgb += payload.beta * w_b * EnvMap_Le(ray);

    }
}

[shader("miss")]
void MySimpleMissShader(inout RayPayload payload : SV_RayPayload)
{
    if(payload.just_hit == 1) {
        payload.color.x = 0; 
        return;
    }
    float3 ray = WorldRayDirection();
    payload.color.rgb += payload.beta * EnvMap_Le(ray);
}
