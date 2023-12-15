
/*
*   Deferred Rendering Gbuffer creation pixel shader
*   
*   The purpose of this shader is to take the rasterized attributes
*   of our primitives and collect them into a G Buffer.
*   
*   For now, we will include albedo, normal, position, and roughness / metallic
*   into our Gbuffer
* 
*/

#include "common.hlsli"

ConstantBuffer<Material_Data> material_data : register(b0, PixelSpace);

struct PixelShaderInput
{
    float4 Position            : SV_Position;
    float4 Frag_World_Position : TEXCOORD4;
    float3 t                   : TEXCOORD2;
    float3 n                   : TEXCOORD3;
    float2 TextureCoordinate   : TEXCOORD;
};

struct PixelShaderOutput
{
    float4 color       : SV_TARGET0;
    float4 wposition   : SV_TARGET1;
    float4 wnormal     : SV_TARGET2;
    float4 rough_metal : SV_TARGET3;
};

PixelShaderOutput main(PixelShaderInput IN) {

    ////////////////////////
    // Get Texture Values    
    ////////////////////////

    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 b = normalize(cross(IN.n, IN.t) * 1.).xyz;
    float3x3 TBN = float3x3( normalize(IN.t), normalize(b), normalize(IN.n) );
    TBN = transpose( TBN );

    float2 UV;
    UV.x = IN.TextureCoordinate.x;
    UV.y = IN.TextureCoordinate.y;

    float4 albedo_texture_color = pow(texture_2d_table[material_data.albedo_index].Sample(sampler_1, UV), 2.2);

    // This is to discard transparent pixels in textures. See: Chains and foliage in GLTF2.0 Sponza
    if (albedo_texture_color.a < 0.3) {
        discard;
    }

    float3 wNormal;
    if((material_data.flags & MATERIAL_FLAG_NORMAL_TEXTURE)){

        float3 normal_texture_color = texture_2d_table[material_data.normal_index].Sample(sampler_1, UV).xyz;
        float3 tNormal = (normal_texture_color * 2.) - 1.;
        wNormal = normalize(mul(TBN, tNormal));

    } else {

        wNormal = normalize(IN.n);

    }

    float roughness;
    float metallic;
    if((material_data.flags & MATERIAL_FLAG_ROUGHNESSMETALIC_TEXTURE)){

        float3 rm_texture_color = texture_2d_table[material_data.roughness_metallic_index].Sample(sampler_1, UV).xyz;
        roughness = rm_texture_color.g;
        metallic = rm_texture_color.r;

    } else {

        roughness = 0.5;
        metallic = 0.5;

    }

    float3 ambient = albedo_texture_color.rgb;

    // NOTE: Frag shader will discard output values with 0 alpha. Is there a setting for that?
    PixelShaderOutput output;
    output.color       = float4(0, 0, 0, 1);
    output.color.rgb   = ambient;
    output.wposition   = IN.Frag_World_Position;
    output.wnormal     = float4(wNormal, 1);
    output.rough_metal = float4(metallic, roughness, 0, 1);

    return output;
}