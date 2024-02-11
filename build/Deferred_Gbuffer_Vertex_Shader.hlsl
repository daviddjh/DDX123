
#include "common.hlsli"

ConstantBuffer<_Matrix>        model_matrix           : register(b1, VertexSpace);

struct VertexShaderOutput
{
    float4 Position             : SV_Position;
    float4 Frag_Position        : TEXCOORD4;
    float3 t                    : TEXCOORD2;
    float3 n                    : TEXCOORD3;
    float2 TextureCoordinate    : TEXCOORD;
    float  tangent_handidness   : TEXCOORD5;
};

struct Vertex_Position_Normal_Tangent_Color_Texturecoord
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float3 Color     : COLOR;
    float2 texCoord  : TEXCOORD;
    float4 Tangent   : TANGENT;
};

VertexShaderOutput main(Vertex_Position_Normal_Tangent_Color_Texturecoord IN)
{
    VertexShaderOutput OUT;

    // Convert Tangent, Normal vectors to world space:
    float3 normal    = normalize(IN.Normal.xyz);
    float3 w_normal  = normalize((mul(model_matrix._matrix, float4(normal, 0.0))).xyz);
    float3 tangent   = normalize(IN.Tangent.xyz);
    float3 w_tangent = normalize((mul(model_matrix._matrix, float4(tangent, 0.0))).xyz);
    
    matrix mvp_matrix        = mul(per_frame_data.view_projection_matrix, model_matrix._matrix);

    OUT.Position             = mul(mvp_matrix, float4(IN.Position, 1.0));
    OUT.Frag_Position        = mul(model_matrix._matrix, float4(IN.Position, 1.0));
    OUT.TextureCoordinate    = IN.texCoord;
    OUT.t = w_tangent;
    OUT.n = w_normal;
    OUT.tangent_handidness = IN.Tangent.w;

    return OUT;
}