
struct Color_Buffer {
    float3 color;
};

ConstantBuffer<Color_Buffer> color_buffer: register(b0);
Texture2D albedo_texture : register(t0);
SamplerState sampler_1 : register(s0);

struct PixelShaderInput
{
    float4 Position  : SV_Position;
    float3 Color     : COLOR;
    float2 TextureCoordinate : TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_Target
{

    //return float4(color_buffer, 1.0);
    float4 texture_color = albedo_texture.Sample(sampler_1, IN.TextureCoordinate);

    float3 final_color = IN.Color + (0 * texture_color.xyz);

    //return texture_color;
    return float4(final_color, 1.0);

}
