#include "common.hlsli"
#include "utils.hlsli"
#include "bxdf.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4

StructuredBuffer<ReSTIR_DI_Current_Frame_Evaulation_Vars> restir_di_current_frame_reservoir_buffer : register(t1, ComputeSpace);
RWStructuredBuffer<Temporal_Buffer> prev_frame_reservoir_buffer : register(u1, ComputeSpace);

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
    
    /////////////////////////////////////////////
    // UPDATE RESERVOIRS
    /////////////////////////////////////////////

    float rand = pcg_hash_prng(random_u);

    Reservoir temp = current_frame_reservoir;

    Reservoir spacial_reservoir;
    spacial_reservoir.sum_of_weights = 0;
    spacial_reservoir.p_hat_sample   = 0;
    spacial_reservoir.sample         = float3(0,0,0);
    spacial_reservoir.sample_count   = 0;
    spacial_reservoir.W              = 0;
    uint spacial_reservoir_combined_sample_count = 0;


    for(int i = 0; i < 5; i++){

        float2 u = get_rand_float3(random_u);

        u -= 0.5;

        u *= 40.0; // u range of [[-20, 20], [-20, 20]]

        int2 neighbor_xy = pixel_xy + int2(ceil(u));

        rand = pcg_hash_prng(random_u);

        Reservoir neighbor_reservoir = restir_di_current_frame_reservoir_buffer[neighbor_xy.y * 1920 + neighbor_xy.x].reservoir;

        float neighbor_weight = (neighbor_reservoir.p_hat_sample * neighbor_reservoir.W * neighbor_reservoir.sample_count);

        update_reservoir(
            spacial_reservoir, 
            neighbor_reservoir.sample, 
            neighbor_weight,
            neighbor_reservoir.p_hat_sample, 
            rand
            );
        
        spacial_reservoir_combined_sample_count += neighbor_reservoir.sample_count;
    }

    rand = pcg_hash_prng(random_u);

    float current_frame_weight = (current_frame_reservoir.p_hat_sample * current_frame_reservoir.W * current_frame_reservoir.sample_count);

    update_reservoir(
        spacial_reservoir, 
        current_frame_reservoir.sample, 
        current_frame_weight,
        current_frame_reservoir.p_hat_sample, 
        rand
        );
    
    spacial_reservoir_combined_sample_count += current_frame_reservoir.sample_count;
    
    spacial_reservoir.sample_count = spacial_reservoir_combined_sample_count;

    spacial_reservoir.W = (1./(spacial_reservoir.p_hat_sample + 0.0001)) * ((1./(float(spacial_reservoir.sample_count)+0.000001)) * spacial_reservoir.sum_of_weights);

    /////////////////////////////////////////////
    // CALC PIXEL
    /////////////////////////////////////////////

    float2 uv = sphere_coord_to_square_coord(spacial_reservoir.sample);
    float3 L = EnvMap_ImageLe(uv);
    float3 wo = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].wo;
    float3 wi = spacial_reservoir.sample;
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

    float3 output_color = (L * f) * spacial_reservoir.W;// * light_samples.W; //(f * env_light_sample.L * env_light_MIS_weight) / (env_light_sample.pdf);

    // output_color = float3(pixel_xy.x / float(output_dimensions.width), pixel_xy.y / float(output_dimensions.height), 0.0);
    // output_color = current_frame_reservoir.sample;

    // Write to output texture
    texture_2d_uav_table[output_texture_index.texture_index][pixel_xy] = float4(output_color, 1.0);

    prev_frame_reservoir_buffer[pixel_xy.y * 1920 + pixel_xy.x].reservoirs[0] = spacial_reservoir;
    // Update Prev frame Reservoir
    // prev_frame_reservoir_buffer[pixel_xy.y * 1920. + pixel_xy.x] = current_frame_reservoir;

}