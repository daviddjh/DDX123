#ifndef __BXDF__
#define __BXDF__

#include "common.hlsli"
#include "math.hlsli"
#include "utils.hlsli"

struct BSDF_Sample {
    float3 sampled_light;
    float3 wi;
    float3 ks;
    float  pdf;
};

// From: https://www.pbr-book.org/4ed/Reflection_Models/Diffuse_Reflection
float3 BxDF_diffuse_f(float3 wo, float3 wi, float3 albedo){
    return albedo * INV_PI;
}

BSDF_Sample BxDF_diffuse_sample_f(float3 wo, float3 albedo, float2 random_u, Hit_Info hit_info){
    BSDF_Sample bsdf_sample;

    bsdf_sample.sampled_light = albedo * INV_PI;
    bsdf_sample.wi = sample_cosign_hemisphere(random_u);
    if(wo.y < 0)
        bsdf_sample.wi.y *= -1;
    
    bsdf_sample.pdf = cosign_hemisphere_pdf(abs_cos_theta(bsdf_sample.wi));

    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 w_Per_Vertex_Normal  = hit_info.n;
    float3 w_Per_Vertex_Tangent = hit_info.t;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -hit_info.t_handedness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );

    bsdf_sample.wi = mul(bsdf_sample.wi, TBN);

    return bsdf_sample;
}

float BxDF_diffuse_pdf(float3 wo, float3 wi){
    return cosign_hemisphere_pdf(abs_cos_theta(wi));
}

//////////////////////////////////////////////////
// Torrance-Sparrow Normal Sampling
//////////////////////////////////////////////////

// These are the transformations of the "microfacets", or tiny ellipsoidal shapes used to model a surface
// 1/alpha_x, 1/alpha_y = 0
// alpha_x, alpha_y ~~ 0 == ellipsoid stretched to flat surface, approximates perfectly specular material
// alpha_x, alpha_y ~~ 0.3 == ellipsoid are large enough to introduce enough normal variation to make the surface apear rough
// when alpha_x == alpha_y, the surface is isotropic.

// Describes the ratio of light that gets reflected over the light that gets refracted
// F0 is the base reflectivity of the surface
// https://en.wikipedia.org/wiki/Schlick%27s_approximation
float3 fresnel_schlick_aprox(float cosTheta, float3 F0){
    return F0 + (float3(1.0,1.0,1.0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Sample normal at a microfacet ( TrowbridgeReitz )
// PBR book section 9.6
float3 TR_sample_wm(float3 w, float2 u, float roughness){
    roughness = pow(roughness, 2);
    float alpha_x = roughness, alpha_y = roughness;
    float3 wh = normalize(float3(alpha_x * w.x, alpha_y * w.y, w.z));               /// CHECK THIS FOR ERRORS. SWAPPING Y and Z from book
    if(wh.z < 0){
        wh = -wh;
    }
    float3 t1 = (w.z < 0.9999f ? normalize(cross(float3(0., 0., 1.), wh)) : float3(1, 0, 0));
    float3 t2 = cross(wh, t1);

    float2 p = sample_uniform_disk_polar(u);

    // Affine transformation of the z interval ( ? )
    float h = sqrt(1- (p.x * p.x));
    p.y = lerp(h, p.y, (1 + wh.z) / 2);
    float pz = sqrt(max(0, 1 - (pow(p.x,2) + pow(p.y,2))));
    float3 nh = p.x * t1 + p.y * t2 + pz * wh;
    nh = normalize(float3(alpha_x * nh.x, alpha_y * nh.y, nh.z));
    return nh;//max(0.000001, nh.z)));
}

// Analytic solution of the masking function for Trowbridge-Reitz distribution:
float TR_lambda(float3 w, float roughness){
    float alpha_x = roughness, alpha_y = roughness;
    float tan2theta = tan_2_theta(w);
    if (isinf(tan2theta) ) return 0.;
    float alpha2 = sqr(cos_phi(w) * alpha_x) + sqr(sin_phi(w) * alpha_y);        // !!!!!!!!!!!!!!!!!! This is probably not neede ( only used for anisotropic materials )
                                                                                 // alpha should just be the surface roughness...
    return (sqrt(1 + alpha2 * tan2theta) - 1) / 2;
}

// Density of microfacet normals
float TR_D(float3 wm, float roughness) {
    float alpha_x = roughness, alpha_y = roughness;
    float tan2theta = tan_2_theta(wm);
    if(isinf(tan2theta)) return 0;
    float cos4theta = sqr(cos_2_theta(wm));
    float e = tan2theta * (sqr(cos_phi(wm) / alpha_x) + 
                           sqr(sin_phi(wm) / alpha_y));
    return 1 / (PI * alpha_x * alpha_y * cos4theta * sqr(1 + e));
}

// Geomtery Masking and Shadowing
float TR_G(float3 wo, float3 wi, float roughness) {
    return 1 / (1 + TR_lambda(wo, roughness) + TR_lambda(wi, roughness));
}

// Density of visible microfacet normals
float TR_D_vis(float3 w, float3 wm, float roughness) {
    // Geometry Masking function:
    float G1 = 1 / (1 + TR_lambda(w, roughness));
    return G1  / abs_cos_theta(w) * TR_D(wm, roughness) * abs(dot(w, wm));
}

// Probability that a microfacet normal was selected
float TR_pdf(float3 w, float3 wm, float roughness) {
    return TR_D_vis(w, wm, roughness);
}

float BxDF_TS_pdf(float3 wo, float3 wi, float roughness)
{
    float3 wm = normalize(wo + wi);

    // Compute PDF for microfact reflection
    // Probability that a wi vector was selected. (basicly TR_pdf adjusted)
    float pdf = TR_pdf(wo, wm, roughness) / (4 * abs(dot(wo, wm)));
    return pdf;
}

float3 BxDF_TS_f(float3 wo, float3 wi, float3 albedo, Hit_Info hit_info, float metallic, float roughness, inout float3 F){
    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 w_Per_Vertex_Normal  = hit_info.n;
    float3 w_Per_Vertex_Tangent = hit_info.t;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -hit_info.t_handedness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );
    float3x3 inv_TBN = transpose(TBN);

    wo = normalize(mul(wo, inv_TBN));
    wi = normalize(mul(wi, inv_TBN));

    float cosTheta_o = abs(cos_theta(wo));
    float cosTheta_i = abs(cos_theta(wi));
    if(cosTheta_o == 0. || cosTheta_i == 0.) return float3(0., 0., 0.);
    float3 wm = wi + wo;
    if((sqr(wm.x) + sqr(wm.y) + sqr(wm.z)) == 0) return float3(0., 0., 0.);
    wm = normalize(wm);

    float3 F0 = float3(0.04, 0.04, 0.04); 
    F0 = lerp(F0, albedo, metallic);
    F = fresnel_schlick_aprox(cosTheta_o, F0);

    return TR_D(wm, roughness) * F * TR_G(wo, wi, roughness) / (4 * cosTheta_i * cosTheta_o);

}

BSDF_Sample BxDF_TS_sample_f(float3 wo, float3 albedo, float2 random_u, Hit_Info hit_info, float metallic, float roughness){
    BSDF_Sample bsdf_sample; 
    bsdf_sample.pdf = 0.;
    bsdf_sample.sampled_light = float3(0., 0., 0.);
    bsdf_sample.wi = float3(0., 0., 0.);

    // Create Tangent-Bitangent-Normal matrix to convert Tangent Space normal to world space normal
    // https://stackoverflow.com/questions/16555669/hlsl-normal-mapping-matrix-multiplication
    float3 w_Per_Vertex_Normal  = hit_info.n;
    float3 w_Per_Vertex_Tangent = hit_info.t;
    
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Gram - Schmidt process
    // Re Orthoganalizes the tangent vector ( ensures 90* between Normal and Tangent)
    // Vectors could be slightly off of 90*
    // Might be more useful in the pixel shader if we built a TBN matrix there, after interpolating tangent and normal through rasterization
    // 
    // Scale Normal by cos(theta), then line between scaled normal and tangent is orthoganal to original normal. Subtract tangent to get new tangent
    w_Per_Vertex_Tangent = normalize(w_Per_Vertex_Tangent - dot(w_Per_Vertex_Tangent, w_Per_Vertex_Normal) * w_Per_Vertex_Normal );
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    float3 w_Per_Vertex_Bitangent = cross(w_Per_Vertex_Normal, w_Per_Vertex_Tangent) * -hit_info.t_handedness;  // Need to multiplay by (negative) tangent handidness to correct for handidness of textures tangent space and DirectX UV space

    float3x3 TBN = float3x3( normalize(w_Per_Vertex_Tangent), normalize(w_Per_Vertex_Bitangent), normalize(w_Per_Vertex_Normal) );
    float3x3 inv_TBN = transpose(TBN);

    wo = normalize(mul(wo, inv_TBN));

    // wm = sampled microfacet normal
    // wo = light exiting the microfacet, on the way to the camera ( somehow )
    // wi = light ray entering the microfact (directly or indirectly) from a light source

    // Sample microfacet normal + compute reflected direction:
    float3 wm = TR_sample_wm(wo, random_u, roughness);

    // Reflect
    float3 wi = -wo + 2 * dot(wo, wm) * wm;

    wi = normalize(wi);

    // Compute PDF for microfact reflection
    // Probability that a wi vector was selected. (basicly TR_pdf adjusted)
    float pdf = TR_pdf(wo, wm, roughness) / (4 * abs(dot(wo, wm)));

    float cosTheta_o = abs(cos_theta(wo));
    float cosTheta_i = abs(cos_theta(wi));

    // Fresnel Factor for conductor BRDF:
    float3 F0 = float3(0.04, 0.04, 0.04); 
    F0 = lerp(F0, albedo, metallic);

    float3 F = fresnel_schlick_aprox(cosTheta_o, F0);

    float3 specular = TR_D(wm, roughness) * F * TR_G(wo, wi, roughness) / (4 * cosTheta_i * cosTheta_o);

    bsdf_sample.pdf = pdf;
    bsdf_sample.sampled_light = specular;
    bsdf_sample.wi = normalize(mul(wi, TBN));
    bsdf_sample.ks = F;

    return bsdf_sample;
}

// Cook - Torrance BRDF
// Combine specular and diffuse brdfs. Use Schlick's Fresnel as ratio between diffuse and specular
float3 BxDF_CT_f(float3 wo, float3 wi, float3 albedo, Hit_Info hit_info, float metallic, float roughness){

    float3 F;
    float3 specular = BxDF_TS_f(wo, wi, albedo, hit_info, metallic, roughness, F);

    float3 diffuse = BxDF_diffuse_f(wo, wi, albedo);

    // Fresnel gives us ratio of specular light
    float3 kS = F;

    // Diffuse is whatever is left
    float3 kD = float3(1.0, 1.0, 1.0) - kS;

    // Metalic materials dont refract
    kD *= 1.0 - metallic;

    // Final Cook Torrance Reflectance Equation
    return (kD * diffuse + specular);
}


#endif // __BXDF__