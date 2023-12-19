//#include "common.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4

// Output texture
// RWTexture2D<float4> outputTexture : register(u0);
RWTexture2D<float4> texture_2d_table[] : register(u0, space100);

cbuffer output_texture_index : register(b0, space0){
    uint output_texture_index;
}


// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Read from input texture
    float4 color = texture_2d_table[output_texture_index][DTid.xy];
    
    if(DTid.x > 1000.){
        color += float4(0.2, 0.2, 0., 0.);
    }

    // Write to output texture
    texture_2d_table[output_texture_index][DTid.xy] = color;
}