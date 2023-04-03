struct Per_Frame_Data {
    float3 light_position;    
    float3 light_color;
    int    shadow_texture_index;
    matrix light_space_matrix;
};

struct ViewProjection
{
    matrix VP;
};

struct Model
{
    matrix M;
};

struct Light_Space_Matrix_Buffer
{
    matrix m;
};

ConstantBuffer<ViewProjection> view_projection_matrix:  register(b1);
ConstantBuffer<Model> model_matrix:                     register(b2);
ConstantBuffer<Per_Frame_Data> per_frame_data:          register(b8);

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
    float4 Position             : SV_Position;
    float4 Frag_Position        : TEXCOORD4;
    float4 Light_Space_Position : TEXCOORD5;
    float3 t                    : TEXCOORD2;
    float3 n                    : TEXCOORD3;
    float2 TextureCoordinate    : TEXCOORD;
};

struct Vertex_Position_Normal_Tangent_Color_Texturecoord
{
    float3 Position  : POSITION;
    float3 Normal    : NORMAL;
    float3 Tangent   : TANGENT;
    float3 Color     : COLOR;
    float2 texCoord  : TEXCOORD;
};

VertexShaderOutput main(Vertex_Position_Normal_Tangent_Color_Texturecoord IN)
{
    VertexShaderOutput OUT;

    // Convert Tangent, Normal vectors to world space:
    float3 n = normalize((mul(model_matrix.M, float4(IN.Normal.xyz, 0.0))).xyz);
    float3 t = normalize((mul(model_matrix.M, float4(IN.Tangent.xyz, 0.0))).xyz);
    
    // I think this is right..
    matrix mvp_matrix     = mul(view_projection_matrix.VP, model_matrix.M);

    // I think this is right..
    OUT.Position             = mul(mvp_matrix, float4(IN.Position, 1.0));
    OUT.Frag_Position        = mul(model_matrix.M, float4(IN.Position, 1.0));
    OUT.Light_Space_Position = mul(per_frame_data.light_space_matrix, OUT.Frag_Position);
    OUT.TextureCoordinate = IN.texCoord;
    OUT.t = t;
    OUT.n = n;

    return OUT;
}