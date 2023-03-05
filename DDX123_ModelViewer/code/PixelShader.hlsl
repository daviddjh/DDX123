#define Tex2DSpace space1

#define MATERIAL_FLAG_NONE           0x0
#define MATERIAL_FLAG_NORMAL_TEXTURE 0x1

struct Color_Buffer {
    float3 color;
};

struct Material_Flags {
    uint flags;
};

struct TextureIndex
{
    int i;
};

ConstantBuffer<Color_Buffer> color_buffer: register(b0);
Texture2D texture_2d_table[] : register(t0, Tex2DSpace);

SamplerState sampler_1   : register(s0);
ConstantBuffer<Material_Flags> material_flags: register(b3);

ConstantBuffer<TextureIndex> albedo_index: register(b4);
ConstantBuffer<TextureIndex> normal_index: register(b5);

struct PixelShaderInput
{
    float4 Position      : SV_Position;
    float4 Frag_Position : TEXCOORD4;
    float3 t             : TEXCOORD2;
    float3 n             : TEXCOORD3;
	float3 Color         : COLOR;
    float2 TextureCoordinate : TEXCOORD;
};

float4 main(PixelShaderInput IN) : SV_Target
{

    const float4 light_pos   = float4(0., 20., 0., 1.);
    const float3 light_color = float3(1., 1., 1.);
    const float3 ambient     = 0.1 * light_color;

    // TBN
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 b = normalize(cross(IN.n, IN.t) * 1.).xyz;
    float3x3 TBN = float3x3( normalize(IN.t), normalize(b), normalize(IN.n) );
    TBN = transpose( TBN );

    float2 UV;
    UV.x = IN.TextureCoordinate.x;
    UV.y = IN.TextureCoordinate.y;
    float4 albedo_texture_color = texture_2d_table[albedo_index.i].Sample(sampler_1, UV);

    // This is to discard transparent pixels in textures. See: Chains and foliage in GLTF2.0 Sponza
    if (albedo_texture_color.a < 0.5) {
        discard;
    }

    float3 wNormal;
    if((material_flags.flags & MATERIAL_FLAG_NORMAL_TEXTURE)){

        float3 normal_texture_color = texture_2d_table[normal_index.i].Sample(sampler_1, UV).xyz;
        float3 tNormal = (normal_texture_color * 2.) - 1.;
        wNormal = mul(TBN, tNormal);

    } else {

        wNormal = normalize(IN.n);

    }

    // Direction of light from the fragment
    // TODO: Why doesn't this work
    float3 light_dir = normalize(light_pos - IN.Frag_Position);

    // Diffuse impact of the light
    float diff_impact = max(dot(wNormal, light_dir), 0.0);
    float3 diffuse = diff_impact * light_color;

    // Attenuation
    float attenuation = clamp(150.0 / length(light_pos - IN.Frag_Position), 0.0, 1.0);

    // Put it all together!
    float3 final_color = attenuation * (ambient + diffuse) * albedo_texture_color.rgb;
    return float4(final_color, 1.0);


    /*
    texture_color.r = texture_color.r * texture_color.a;
    texture_color.g = texture_color.g * texture_color.a;
    texture_color.b = texture_color.b * texture_color.a;
    */


    //float3 final_color = IN.Color + (0 * texture_color.xyz);

    //return texture_color;
    //return float4(final_color, 1.0);

}
