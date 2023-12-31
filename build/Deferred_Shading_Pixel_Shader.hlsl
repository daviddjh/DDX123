/*
*   Deferred Rendering Shading Pass
*   
*   The purpose of this shader is use data from the GBuffer and render the scene with it.
*
*/

// common defines, sampler_1, texture_2d_table, per_frame_data
#include "common.hlsli"

static const float SHADOW_BIAS = 0.00010;
static const int   SSAO_KERNEL_SIZE = 64;
static const float SSAO_RADIUS = 2.5;
static const float SSAO_BIAS = 0.25;

// Constant buffer contianing the texture indicies for the GBuffer
ConstantBuffer<Gbuffer_Indices>   gbuffer_indices      : register(b0, PixelSpace);
ConstantBuffer<SSAO_Sample>       ssao_sample          : register(b1, PixelSpace);
ConstantBuffer<Texture_Index>     ssao_texture_index   : register(b2, PixelSpace);
ConstantBuffer<Texture_Index>     shadow_texture_index : register(b3, PixelSpace);
ConstantBuffer<Output_Dimensions> output_dimensions    : register(b4, PixelSpace);

struct PixelShaderInput
{
    float4 Position            : SV_Position;
};

static float2 poissonDisk[9] = {
    float2(0.526, 0.786),
    float2(0.265, 0.312),
    float2(0.806, 0.223),
    float2(0.919, 0.605),
    float2(0.545, 0.116),
    float2(0.082, 0.662),
    float2(0.769, 0.958),
    float2(0.155, 0.919),
    float2(0.465, 0.491)
};

uint rand_xorshift(uint random_number)
{
    // Xorshift algorithm from George Marsaglia's paper
    random_number ^= (random_number << 13);
    random_number ^= (random_number >> 17);
    random_number ^= (random_number << 5);
    return random_number;
}

// Describes the ratio of light that gets reflected over the light that gets refracted
// F0 is the base reflectivity of the surface
float3 fresnelSchlick(float cosTheta, float3 F0){
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Given a roughness value, statistically estimates the relative surface area of microfacets that
// are aligned to the halfway vector
//
// Low roughness = lots of microfacets aligned in small area = bright specular spot
// high roughness = some microfacets aligned over large area = no bright spot, much larger surface area lit to a smaller degree
//
// The specific normal distribution fucntion we are using is Trowbridge-Reitz GGX
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

// The Geometry function approximates relative surface area where microfacets
// oclude light that would've made it to the camera based on roughness
//
// Rough microfacets == microfacets more likely to occlude light = less light
float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}

// Need to calc Geometry function for View direction and light direction, since both can be occluded
float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float calc_shadow_value(float4 frag_pos_light_space, float2 tex_coords){

    float3 proj_coords = frag_pos_light_space.xyz / frag_pos_light_space.w;
    // TODO: why is it work like this??? - Would've thought z coord needed [-1,1] -> [0,1] transform too? 
    float current_depth = proj_coords.z;
    proj_coords = proj_coords * 0.5 + 0.5;

    float x_offset = 1./output_dimensions.width;
    float y_offset = 1./output_dimensions.height;

    float shadow = 0.0;


    // Multi Sample Shadow Blur (Reduces performance due to texture reads, doesn't look very good..)
    uint random_number = asuint((tex_coords.x + tex_coords.y) % 64);
    // NOTE: UV.. V is inverted when coming from light space coords
    const float size = 3;
    for(float y=-size;y<=size;y++){
        for(float x=-size;x<=size;x++){
            float2 uv = float2(proj_coords.x + (x*x_offset), 1 - proj_coords.y + (y*y_offset));

            //int poisson_index = (tex_coords.x * (x + 5) + tex_coords.y * (y + 4)) % 9;

            random_number = rand_xorshift(random_number);
            uv += (ssao_sample.ssao_sample[random_number % 64].rg - 0.5) / 1000.0;
            // uv += (poissonDisk[random_number % 9] - 0.5) / 800.0;

            float closest_depth = texture_2d_table[shadow_texture_index.texture_index].Sample(sampler_1, uv).r;
            shadow += (current_depth - SHADOW_BIAS > closest_depth ? (1/(abs(y)+1))+(1/(abs(x)+1)) : 0.);
        }
    }
    shadow /= 44.333;
    //shadow /= 25;

    // float2 uv = float2(proj_coords.x, 1 - proj_coords.y);
    // float closest_depth = texture_2d_table[shadow_texture_index.shadow_texture_index].Sample(sampler_1, uv).r;
    // shadow += (current_depth - SHADOW_BIAS > closest_depth ? 1.0 : 0.);

    return shadow;
}

float4 main(PixelShaderInput IN) : SV_Target {

    ////////////////////////
    // Get Gbuffer Values    
    ////////////////////////
    float2 UV = float2((IN.Position.x / output_dimensions.width), (IN.Position.y / output_dimensions.height));

    float4 albedo_texture_color = texture_2d_table[gbuffer_indices.albedo_index].Sample(
        sampler_1,
        UV
    );
    if (albedo_texture_color.a < 0.1){
        discard;
    }

    float4 w_position = texture_2d_table[gbuffer_indices.position_index].Sample(
        sampler_1,
        UV
    );
    w_position.w = 1.;

    float4 w_normal= texture_2d_table[gbuffer_indices.normal_index].Sample(
        sampler_1,
        UV
    );

    float4 roughness_metallic = texture_2d_table[gbuffer_indices.roughness_metallic_index].Sample(
        sampler_1,
        UV
    );
    float roughness = roughness_metallic.g;
    float metallic  = roughness_metallic.r;

    // Calculate shading

    float3 N = normalize(w_normal.xyz);
    float3 V = normalize(per_frame_data.camera_pos - w_position.xyz);
    float3 light_position = per_frame_data.light_position;
    float3 light_direction = per_frame_data.light_position;
    float3 light_color    = per_frame_data.light_color;
    float3 Lo = float3(0., 0., 0.);
    
    // Approximation of base reflectivity for fresnel
    float3 F0 = float3(0.04, 0.04, 0.04); 
    F0 = lerp(F0, albedo_texture_color.rgb, metallic);

    // Only doing this once because we have one light
    for (int i = 0; i < 1; i++){
        //float3 L = normalize(light_position - IN.Frag_World_Position);
        float3 L = normalize(-light_direction);
        float3 H = normalize(V + L);

        float distance = length(light_position - w_position.xyz);
        float attenuation = 1.0; // (distance * distance);
        float3 radiance = light_color * attenuation;
        
        // For Frensel - F0 = surface reflection at zero incidence
        float3 F  = fresnelSchlick(max(dot(H, V), 0.0), F0);

        float NDF = DistributionGGX(N, H, roughness);
        float G   = GeometrySmith(N, V, L, roughness);

        // Cook-Torrance BRDF
        float3 numerator = NDF * G * F; 
        float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0)  + 0.0001;
        float specular = numerator / denominator;

        // Fresnel gives us ratio of specular light
        float3 kS = F;
        // Diffuse is whatever is left
        float3 kD = float3(1.0, 1.0, 1.0) - kS;

        // Metalic materials dont refract..
        kD *= 1.0 - metallic;

        // Final Cook Torrance Reflectance Equation
        float NdotL = max(dot(N, L), 0.0);
        Lo += (kD * albedo_texture_color.rgb / PI + specular) * radiance * NdotL;

    }

    // Calculate SSAO
    
    // // Scales the noise to our screen size, so the noise can wrap an repeat
    // float2 noise_scale = float2(output_dimensions.width / 4., output_dimensions.height / 4.0);
    // float3 random_vector = texture_2d_table[ssao_texture_index.ssao_rotation_texture_index].Sample(sampler_1, UV * noise_scale);

    // // create our TBN matrix from our random vector an our normal
    // float3 tangent = normalize(random_vector - N * dot(random_vector, N));
    // float3 bitangent = cross(N, tangent);
    // float3x3 TBN = float3x3(tangent, bitangent, N);
    // TBN = transpose(TBN);


    // // Sample points with SSAO_RADIUS hemisphere and check if they're occluded. If so, add to occlusion variable3
    // float occlusion = 0;
    // float3 sample_position = float3(0, 0, 0);
    // float4 offset = float4(1,1,1,1.0);
    // float sample_depth = 0;
    // for(int k = 0; k < SSAO_KERNEL_SIZE; k++){
    //     sample_position = mul(TBN, ssao_sample.ssao_sample[k].xyz);
    //     sample_position = w_position.xyz + sample_position * SSAO_RADIUS;
    //     offset = float4(sample_position, 1.0);
    //     offset = mul(per_frame_data.view_projection_matrix, offset);
    //     offset.xyz /= offset.w;
    //     offset.xyz = offset.xyz * 0.5 + 0.5;
    //     sample_depth = distance(per_frame_data.camera_pos, texture_2d_table[gbuffer_indices.position_index].Sample(sampler_1, float2(offset.x, 1-offset.y)).xyz);
    //     float range_check = smoothstep(0.0, 1.0, SSAO_RADIUS / abs(distance(per_frame_data.camera_pos, w_position.xyz) - sample_depth));
    //     //float range_check = 1.0;
    //     occlusion += (sample_depth <= (distance(per_frame_data.camera_pos, sample_position.xyz) + SSAO_BIAS) ? 1.0 : 0.0) * range_check;
    // }

    // occlusion = pow(1 - (occlusion / SSAO_KERNEL_SIZE),2);
    float3 ambient = float3(0.014, 0.014, 0.014);
    // ambient += (occlusion/10.);

    ambient = ambient * albedo_texture_color.rgb;
    matrix light_space_matrix = per_frame_data.light_space_matrix;
    float4 frag_pos_light_space = mul(light_space_matrix, w_position);
    float shadow = calc_shadow_value(frag_pos_light_space, float2(w_position.x * w_normal.z, albedo_texture_color.g * w_normal.x));
    float3 color = ambient + (1.0 - shadow) * Lo;
	
    // color = color / (color + float3(1.0, 1.0, 1.0));
    // color = pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));  
   
    return float4(color, 1.0);
    

}