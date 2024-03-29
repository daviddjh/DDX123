#if defined(__cplusplus)
#include <DirectXMath.h>
typedef DirectX::XMFLOAT3 float3;
typedef DirectX::XMFLOAT4 float4;
typedef DirectX::XMMATRIX matrix;
typedef unsigned int        uint;
#define ALIGN_STRUCT __declspec(align(256))
#endif

#ifndef ALIGN_STRUCT
#define ALIGN_STRUCT
#endif

// Per frame global data for all passes
ALIGN_STRUCT struct Per_Frame_Data {
    float4 light_position;    
    float4 light_color;
    float4 camera_pos;
    matrix light_space_matrix;
    matrix view_projection_matrix;
    uint   render_to_display_scale;
};

// Gbuffer Texture Indicies
ALIGN_STRUCT struct Gbuffer_Indices 
{
    uint albedo_index;
    uint position_index;
    uint normal_index;
    uint roughness_metallic_index;
};

// Gbuffer Texture Indicies
ALIGN_STRUCT struct Material_Data
{
    uint albedo_index;
    uint normal_index;
    uint roughness_metallic_index;
    uint flags;
};

// Pass Output Dimensions
ALIGN_STRUCT struct Output_Dimensions
{
    uint width;
    uint height;
};

// Static SSAO sample buffer
ALIGN_STRUCT struct SSAO_Sample
{
    float4 ssao_sample[64];
};

// Texture Index - used to send indexes of textures in texture table
ALIGN_STRUCT struct Texture_Index
{
    uint texture_index;
};

// Model Matrix
ALIGN_STRUCT struct _Matrix 
{
    matrix _matrix;
};