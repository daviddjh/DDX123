#include "common.hlsli"
#include "utils.hlsli"
#include "bxdf.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 4

StructuredBuffer<Temporal_Buffer> prev_frame_reservoir_buffer : register(t1, ComputeSpace);
RWStructuredBuffer<ReSTIR_DI_Current_Frame_Evaulation_Vars> restir_di_current_frame_reservoir_buffer : register(u1, ComputeSpace);

ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint2 pixel_xy = DTid.xy;
    float2 texture_loc = pixel_xy % 32.;

    Reservoir current_frame_reservoir = prev_frame_reservoir_buffer[pixel_xy.y * 1920 + pixel_xy.x].reservoirs[0];
    float tangent_handedness = restir_di_current_frame_reservoir_buffer[pixel_xy.y * output_dimensions.width + pixel_xy.x].tangent_handedness;
    if(tangent_handedness == 100.) return;
    
    restir_di_current_frame_reservoir_buffer[pixel_xy.y * 1920 + pixel_xy.x].reservoir = current_frame_reservoir;
}