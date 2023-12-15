
#include "common.hlsli"

ConstantBuffer<_Matrix> light_matrix : register(b0, VertexSpace);
ConstantBuffer<_Matrix> model_matrix : register(b1, VertexSpace);

struct Vertex_Position_Normal_Tangent_Color_Texturecoord
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float3 Tangent   : TANGENT;
    float3 Color     : COLOR;
    float2 texCoord  : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 position  : SV_Position;
};

VertexShaderOutput main(Vertex_Position_Normal_Tangent_Color_Texturecoord IN)
{
    VertexShaderOutput OUT;

    // I think this is right..
    OUT.position          = mul(model_matrix._matrix, float4(IN.Position, 1.0));
    OUT.position          = mul(light_matrix._matrix, OUT.position);

    return OUT;
}

