struct ModelViewProjection
{
    matrix MVP;
};

struct Model
{
    matrix M;
};

//ConstantBuffer<Model> ModelCB : register(b0);
//ConstantBuffer<ModelViewProjection> ModelViewProjectionCB : register(b0);

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

    //OUT.Position = mul(ModelViewProjectionCB.MVP, float4(IN.Position, 1.0f));
    OUT.Position          = float4(IN.Position, 1.0f);
    OUT.Color             = IN.Color;
    OUT.TextureCoordinate = IN.texCoord;

    return OUT;
}