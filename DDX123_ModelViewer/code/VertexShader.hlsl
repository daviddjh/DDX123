struct ViewProjection
{
    matrix VP;
};

struct Model
{
    matrix M;
};

//ConstantBuffer<Model> ModelCB : register(b0);
//ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

ConstantBuffer<ViewProjection> view_projection_matrix: register(b1);
ConstantBuffer<Model> model_matrix:                    register(b2);

struct VertexPosColor
{
    float3 Position : POSITION;
    float3 Color    : COLOR;
};

struct VertexPosColorTexture
{
    float3 Position : POSITION;
    float3 Color    : COLOR;
    float2 texCoord : TEXCOORD;
};

struct VertexPosTexture
{
    float3 Position : POSITION;
    float2 texCoord : TEXCOORD;
};

struct VertexPositionNormalTexture
{
	float3 Position : POSITION;
	float3 Normal   : NORMAL;
	float2 TextureCoordinate : TEXCOORD;
};

struct VertexShaderOutput
{
    float4 Position : SV_Position;
	float3 Color    : COLOR;
    float2 TextureCoordinate : TEXCOORD;
};

VertexShaderOutput main(VertexPosColorTexture IN)
{
    VertexShaderOutput OUT;

    // I think this is right..
    matrix mvp_matrix     = mul(view_projection_matrix.VP, model_matrix.M);
    // I think this is right..
    OUT.Position          = mul(mvp_matrix, float4(IN.Position, 1.0));
    OUT.Color             = IN.Color;
    OUT.TextureCoordinate = IN.texCoord;

    return OUT;
}