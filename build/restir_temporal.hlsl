#include "common.hlsli"
#include "utils.hlsli"
#include "bxdf.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4

StructuredBuffer<ReSTIR_DI_Current_Frame_Evaulation_Vars> restir_di_current_frame_reservoir_buffer : register(t1, ComputeSpace);
RWStructuredBuffer<Reservoir> prev_frame_reservoir_buffer : register(u1, ComputeSpace);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);
ConstantBuffer<Texture_Index>     random_tex_index  : register(b2, ComputeSpace);
ConstantBuffer<Texture_Index>     env_map_index     : register(b3, ComputeSpace);

float3 EnvMap_ImageLe(float2 uv){
    
    Texture2D environment_texture = texture_2d_table[env_map_index.texture_index];
    return environment_texture.SampleLevel(sampler_1, uv, 0);
}

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel_xy = DTid.xy;
    float2 texture_loc = pixel_xy % 32.;
    
    Texture2D<uint> random_tex = texture_2d_uint_table[random_tex_index.texture_index];
    uint random_u = random_tex.Load(float3(texture_loc, 0));
    random_u *= pixel_xy.x * pixel_xy.y;

    Reservoir current_frame_reservoir = restir_di_current_frame_reservoir_buffer[pixel_xy.y * 1920 + pixel_xy.x].reservoir;
    Reservoir prev_frame_reservoir    = prev_frame_reservoir_buffer[pixel_xy.y * 1920 + pixel_xy.x];

    /////////////////////////////////////////////
    // UPDATE RESERVOIRS
    /////////////////////////////////////////////

    float rand = pcg_hash_prng(random_u);

    Reservoir temporal_reservoir;
    temporal_reservoir.sum_of_weights = 0;
    temporal_reservoir.p_hat_sample   = 0;
    temporal_reservoir.sample         = float3(0,0,0);
    temporal_reservoir.sample_count   = 0;
    temporal_reservoir.W              = 0;

    float prev_frame_weight = (prev_frame_reservoir.p_hat_sample * prev_frame_reservoir.W * prev_frame_reservoir.sample_count);

    update_reservoir(
        temporal_reservoir, 
        prev_frame_reservoir.sample, 
        prev_frame_weight,
        prev_frame_reservoir.p_hat_sample, 
        rand
        );

    rand = pcg_hash_prng(random_u);

    float current_frame_weight = (current_frame_reservoir.p_hat_sample * current_frame_reservoir.W * current_frame_reservoir.sample_count);

    update_reservoir(
        temporal_reservoir, 
        current_frame_reservoir.sample, 
        current_frame_weight,
        current_frame_reservoir.p_hat_sample, 
        rand
        );
    
    temporal_reservoir.sample_count = current_frame_reservoir.sample_count + prev_frame_reservoir.sample_count;
    temporal_reservoir.sample_count = min(temporal_reservoir.sample_count, 20. * current_frame_reservoir.sample_count);

    temporal_reservoir.W = (1./temporal_reservoir.p_hat_sample) * ((1./float(temporal_reservoir.sample_count)) * temporal_reservoir.sum_of_weights);

    /////////////////////////////////////////////
    // CALC PIXEL
    /////////////////////////////////////////////

    float2 uv = sphere_coord_to_square_coord(temporal_reservoir.sample);
    float3 L = EnvMap_ImageLe(uv);
    float3 wo = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].wo;
    float3 wi = temporal_reservoir.sample;
    float roughness  = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].roughness;
    float metallic   = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].metallic;
    float3 albedo_rgb = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].albedo_rgb;
    float3 normal = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].normal;
    float3 tangent = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].tangent;
    float tangent_handedness = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].tangent_handedness;

    Hit_Info hit_info;
    hit_info.n = normal;
    hit_info.t = tangent;
    hit_info.t_handedness = tangent_handedness;
    float3 f = BxDF_CT_f(wo, wi, albedo_rgb, hit_info, metallic, roughness) * abs(dot(wi, hit_info.n));

    float3 output_color = (L * f) * temporal_reservoir.W;// * light_samples.W; //(f * env_light_sample.L * env_light_MIS_weight) / (env_light_sample.pdf);

    // output_color = float3(pixel_xy.x / float(output_dimensions.width), pixel_xy.y / float(output_dimensions.height), 0.0);
    // output_color = current_frame_reservoir.sample;

    // Write to output texture
    texture_2d_uav_table[output_texture_index.texture_index][pixel_xy] = float4(output_color, 1.0);

    // Update Prev frame Reservoir
    prev_frame_reservoir_buffer[pixel_xy.y * 1920. + pixel_xy.x] = current_frame_reservoir;

}