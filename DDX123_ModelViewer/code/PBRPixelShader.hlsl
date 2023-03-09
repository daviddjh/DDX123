// Learning from: https://learnopengl.com/PBR/Lighting

/*
// Need:

// Per fragment
FragColor
TexCoords
WorldPos
Normal

// Per Frame
CameraPosition

// Per Draw
Albedo
Metalic
Roughness

*/
#define Tex2DSpace space1
#define MATERIAL_FLAG_NONE                     0x0
#define MATERIAL_FLAG_NORMAL_TEXTURE           0x1
#define MATERIAL_FLAG_ROUGHNESSMETALIC_TEXTURE 0x2

static const float PI = 3.14159265359;

SamplerState sampler_1          : register(s0);
Texture2D texture_2d_table[] : register(t0, Tex2DSpace);

struct Camera_Position {
    float3 camera_position;
};
ConstantBuffer<Camera_Position> camera_position_buffer: register(b4);

struct TextureIndex
{
    int i;
};
ConstantBuffer<TextureIndex> albedo_index:            register(b5);
ConstantBuffer<TextureIndex> normal_index:            register(b6);
ConstantBuffer<TextureIndex> roughness_metallic_index: register(b7);

struct Material_Flags {
    uint flags;
};

ConstantBuffer<Material_Flags> material_flags: register(b3);

struct PixelShaderInput
{
    float4 Position            : SV_Position;
    float4 Frag_World_Position : TEXCOORD4;
    float3 t                   : TEXCOORD2;
    float3 n                   : TEXCOORD3;
    float2 TextureCoordinate   : TEXCOORD;
};

float3 fresnelSchlick(float cosTheta, float3 F0){
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float4 main(PixelShaderInput IN) : SV_Target {

    ////////////////////////
    // Get Texture Values    
    ////////////////////////

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

    float roughness;
    float metallic;
    if((material_flags.flags & MATERIAL_FLAG_ROUGHNESSMETALIC_TEXTURE)){

        float3 rm_texture_color = texture_2d_table[roughness_metallic_index.i].Sample(sampler_1, UV).xyz;
        roughness = rm_texture_color.g;
        metallic = rm_texture_color.r;

    } else {

        roughness = 0.5;
        metallic = 0.5;

    }

    float3 N = normalize(IN.n);
    float3 V = normalize(camera_position_buffer.camera_position - IN.Frag_World_Position);
    float3 light_position = float3(10., 10., 10.);
    float3 light_color    = float3(1., 1., 1.);
    float3 Lo = float3(0., 0., 0.);

    // Only doing this once because we have one light
    for (int i = 0; i < 1; i++){
        float3 L = normalize(light_position - IN.Frag_World_Position);
        float3 H = normalize(V + L);

        float distance = length(light_position - IN.Frag_World_Position);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = light_color * attenuation;
        
        // For Frensel - F0 = surface reflection at zero incidence
        float3 F0 = float3(0.04, 0.04, 0.04); 
        F0 = lerp(F0, albedo_texture_color, metallic);
        float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(wNormal, H, roughness);
        float G   = GeometrySmith(wNormal, V, L, roughness);

        // Cook-Torrance BRDF
        float3 numerator = NDF * G * F; 
        float denominator = 4.0 * max(dot(wNormal, V), 0.0) * max(dot(wNormal, L), 0.0)  + 0.0001;
        float specular = numerator / denominator;

        float3 kS = F;
        float3 kD = float3(1.0, 1.0, 1.0) - kS;

        // Metalic materials dont refract..
        kD *= 1.0 - metallic;

        float NdotL = max(dot(wNormal, L), 0.0);
        Lo += (kD * albedo_texture_color / PI + specular) * radiance * NdotL;

    }

    /*
    Lo /= 30.;
    Lo.r += metallic;
    Lo.g += roughness;
    Lo.b = 1.;
    */
    Lo += albedo_texture_color;

    
    return float4(Lo, 1.0);
}