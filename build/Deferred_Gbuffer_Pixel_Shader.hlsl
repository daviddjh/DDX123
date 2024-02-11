
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
    float  tangent_handidness   : TEXCOORD5;
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
    float3 w_Per_Vertex_Normal  = normalize(IN.n);
    float3 w_Per_Vertex_Tangent = normalize(IN.t);
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -IN.tangent_handidness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );
    TBN = transpose( TBN );

    float2 UV;
    UV.x = IN.TextureCoordinate.x;
    UV.y = IN.TextureCoordinate.y;

    float4 albedo_texture_color = pow(texture_2d_table[material_data.albedo_index].Sample(sampler_1, UV), 2.2);

    // This is to discard transparent pixels in textures. See: Chains and foliage in GLTF2.0 Sponza
    if (albedo_texture_color.a < 0.3) {
        discard;
    }

    float4 wNormal;
    if((material_data.flags & MATERIAL_FLAG_NORMAL_TEXTURE)){

        float3 normal_texture_color = texture_2d_table[material_data.normal_index].Sample(sampler_1, UV).xyz;
        float3 tNormal = normalize((normal_texture_color * 2.) - 1.);
        wNormal = float4(normalize(mul(TBN, tNormal)), 1.0);

    } else {

        wNormal = float4(normalize(IN.n), 1.0);

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
    output.wnormal     = wNormal;
    output.rough_metal = float4(metallic, roughness, 0, 1);

    return output;
}