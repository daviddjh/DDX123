#include "common.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<Texture_Index> output_texture_index : register(b0, ComputeSpace);
// ConstantBuffer<Texture_Index> input_texture_index  : register(b1, ComputeSpace);
// ConstantBuffer<Texture_Index> ssao_texture_index   : register(b2, ComputeSpace);

static float3 background_color = float3(0.4, 0.5, 0.3);

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 xy   = DTid.xy;
    // Read linear color from input texture
    float3 color = float3(0.0, 0.0, 0.0); 
    
    // Write to output texture
    texture_2d_uav_table[output_texture_index.texture_index][xy] = float4(color, 1.0);
}