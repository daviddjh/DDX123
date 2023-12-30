#include "common.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

// Output texture
// RWTexture2D<float4> outputTexture : register(u0);
RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<SSAO_Texture_Index> output_texture_index : register(b0, ComputeSpace);
ConstantBuffer<SSAO_Texture_Index> ssao_texture_index   : register(b1, ComputeSpace);

static const float A = 0.15;
static const float B = 0.50;
static const float C = 0.10;
static const float D = 0.20;
static const float E = 0.02;
static const float F = 0.30;
static const float W = 11.2;

float3 Uncharted2Tonemap(float3 x)
{
   return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Read linear color from input texture
    float3 color = texture_2d_uav_table[output_texture_index.ssao_rotation_texture_index][DTid.xy].xyz;

    float ssao_occlusion = texture_2d_table[ssao_texture_index.ssao_rotation_texture_index][DTid.xy].x;
    
    // Tone Map and Gamma Correct
    color *= ssao_occlusion;
    // color *= 4.0;
    // color *= 0.5;

    // Reinhert
    color = color / (color + float3(1.0, 1.0, 1.0));

    // Filmic curve from: http://filmicworlds.com/blog/filmic-tonemapping-operators/
    // NO GAMMA ADJ REQUIRED, BAKED IN
    // color = max(float3(0.0, 0.0, 0.0), color-0.004);
    // color = (color*(6.2*color+.5))/(color*(6.2*color+1.7)+0.06);
   
    // Uncharted2 tonemap from: http://filmicworlds.com/blog/filmic-tonemapping-operators/
    // Everything looks ok, except for the floor looks too bright??
    // float ExposureBias = 2.0f;
    // float3 curr = Uncharted2Tonemap(ExposureBias*color);

    // float3 whiteScale = 1.0f/Uncharted2Tonemap(W);
    // color = curr*whiteScale;

    // Gamma Correction
    color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  

    // Write to output texture
    texture_2d_uav_table[output_texture_index.ssao_rotation_texture_index][DTid.xy] = float4(color, 1.0);
}