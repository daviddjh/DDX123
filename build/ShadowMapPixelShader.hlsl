struct PixelShaderInput
{
    float4 position      : SV_Position;
};

float4 main(PixelShaderInput IN) : SV_Target
{
    // discard;
    // Appease compiler
    return IN.position;

}
