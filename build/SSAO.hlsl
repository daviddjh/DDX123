#include "common.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

static const int   SSAO_KERNEL_SIZE = 64;
static const float SSAO_RADIUS = 2.5;
static const float SSAO_BIAS = 0.25;

// Output texture
// RWTexture2D<float4> outputTexture : register(u0);
RWTexture2D<float4>               texture_2d_uav_table[] : register(u0, space99);
ConstantBuffer<SSAO_Sample>       ssao_sample            : register(b1, ComputeSpace);
ConstantBuffer<Texture_Index>     ssao_texture_index     : register(b2, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions      : register(b3, ComputeSpace);
ConstantBuffer<Gbuffer_Indices>   gbuffer_indices        : register(b4, ComputeSpace);

cbuffer output_texture_index : register(b0, ComputeSpace){
    uint output_texture_index;
}

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Read linear color from input texture
    //float3 color = texture_2d_uav_table[output_texture_index][DTid.xy].xyz;

    float2 UV = float2(float(DTid.x) / float(output_dimensions.width), float(DTid.y) / float(output_dimensions.height));

    float4 w_position = texture_2d_table[gbuffer_indices.position_index].SampleLevel(
        sampler_1,
        UV,
        0
    );
    
    // Calculate SSAO
    float4 w_normal= texture_2d_table[gbuffer_indices.normal_index].SampleLevel(
        sampler_1,
        UV,
        0
    );
    float3 N = normalize(w_normal.xyz);
    
    // Scales the noise to our screen size, so the noise can wrap an repeat
    float2 noise_scale = float2(output_dimensions.width / 4., output_dimensions.height / 4.0);
    float3 random_vector = texture_2d_table[ssao_texture_index.texture_index].SampleLevel(sampler_1, UV * noise_scale, 0);

    // create our TBN matrix from our random vector an our normal
    float3 tangent = normalize(random_vector - N * dot(random_vector, N));
    float3 bitangent = cross(N, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, N);
    TBN = transpose(TBN);


    // Sample points with SSAO_RADIUS hemisphere and check if they're occluded. If so, add to occlusion variable3
    float occlusion = 0;
    float3 sample_position = float3(0, 0, 0);
    float4 offset = float4(1,1,1,1.0);
    float sample_depth = 0;
    for(int k = 0; k < SSAO_KERNEL_SIZE; k++){
        sample_position = mul(TBN, ssao_sample.ssao_sample[k].xyz);
        sample_position = w_position.xyz + sample_position * SSAO_RADIUS;
        offset = float4(sample_position, 1.0);
        offset = mul(per_frame_data.view_projection_matrix, offset);
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;
        sample_depth = distance(per_frame_data.camera_pos, texture_2d_table[gbuffer_indices.position_index].SampleLevel(sampler_1, float2(offset.x, 1-offset.y), 0).xyz);
        float range_check = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(distance(per_frame_data.camera_pos, w_position.xyz) - sample_depth));
        //float range_check = 1.0;
        occlusion += (sample_depth <= (distance(per_frame_data.camera_pos, sample_position.xyz) + SSAO_BIAS) ? 1.0 : 0.0) * range_check;
    }

    occlusion = pow(1 - (occlusion / SSAO_KERNEL_SIZE),2); // / 10.;

    // Write to output texture
    texture_2d_uav_table[output_texture_index][DTid.xy] = occlusion;
}