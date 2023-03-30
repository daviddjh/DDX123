struct Light_Matrix_Buffer
{
    matrix light_matrix;
};

struct Model_Matrix_Buffer
{
    matrix model_matrix;
};

ConstantBuffer<Light_Matrix_Buffer> light_matrix: register(b1);
ConstantBuffer<Model_Matrix_Buffer> model_matrix: register(b2);

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
    OUT.position          = mul(model_matrix.model_matrix, float4(IN.Position, 1.0));
    OUT.position          = mul(light_matrix.light_matrix, OUT.position);

    return OUT;
}

