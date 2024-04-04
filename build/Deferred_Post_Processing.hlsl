#include "common.hlsli"
#include "color_space.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

// Output texture
// RWTexture2D<float4> outputTexture : register(u0);
RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<Texture_Index> output_texture_index : register(b0, ComputeSpace);
ConstantBuffer<Texture_Index> input_texture_index  : register(b1, ComputeSpace);
ConstantBuffer<Texture_Index> ssao_texture_index   : register(b2, ComputeSpace);

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Read linear color from input texture
    float3 color = texture_2d_table[input_texture_index.texture_index][DTid.xy].xyz;

    // Read ssao occlusion ammount
    float ssao_occlusion = texture_2d_table[ssao_texture_index.texture_index][DTid.xy].x;
    
    // Tone Map and Gamma Correct
    color *= ssao_occlusion;

    // Reinhert
    // color = apply_reinhert(color);

    // ACES
    color = apply_aces_film_curve(color);
    
    // color = ssao_occlusion;

    // Gamma Correction - (Inverse Electrical Optical Transfer Function)
    color = apply_srgb_curve(color);  // - Gamma, for sRGB. REC709. SDR
    
    // Write to output texture
    //   
    // x, y   |  x+1, y
    // ___________________
    // 
    // x, y+1 |  x+1, y+1
    
    uint scale = per_frame_data.render_to_display_scale;

    uint2 xy   = DTid.xy * per_frame_data.render_to_display_scale;

    for(uint i = 0; i < scale; i++){
        for(uint j = 0; j < scale; j++){
            uint2 scaled_pixel_position = float2((xy.x + i), (xy.y + j));
            texture_2d_uav_table[output_texture_index.texture_index][scaled_pixel_position] = float4(color, 1.0);
        }
    }
}