struct VertexShaderOutput
{
    float4 Position : SV_Position;
};

struct Vertex_Input
{
    float3 Position : POSITION;
};

VertexShaderOutput main(Vertex_Input IN)
{
    VertexShaderOutput OUT;

    OUT.Position = float4(IN.Position, 1);

    return OUT;
}