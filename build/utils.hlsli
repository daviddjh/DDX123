#ifndef __UTILS__
#define __UTILS__

#include "common.hlsli"
#include "math.hlsli"

void update_reservoir(inout Reservoir reservoir, float3 sample_y, float weight, float p_hat_sample, float rand){

    reservoir.sum_of_weights += weight;
    reservoir.sample_count += 1;//min(light_samples.sample_count + 1, 20);
    if (rand < (weight / (reservoir.sum_of_weights+0.000001))){
        reservoir.sample = sample_y;
        reservoir.p_hat_sample = p_hat_sample;
    }

}

// https://en.wikipedia.org/wiki/Relative_luminance
float rgb_to_relative_luminance(float3 rgb){
    return rgb.r * 0.2126 + rgb.g * 0.7152 + rgb.b * 0.0722;
}

// from: https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
float pcg_hash(uint input)
{
    uint state = input * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint final_int = (word >> 22u) ^ word;
    return final_int * (1.0 / 4294967296.0); // Convert from [0, MAX_INT] int to [0,1] float
}

float pcg_hash_prng(inout uint rng_state)
{
    uint state = rng_state;
    rng_state = rng_state * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    uint final_int = (word >> 22u) ^ word;
    return final_int * (1.0 / 4294967296.0); // Convert from [0, MAX_INT] int to [0,1] float
}

float3 get_rand_float3(inout uint u){
    float3 rand;
    rand.x = pcg_hash_prng(u);
    rand.y = pcg_hash_prng(u);
    rand.z = pcg_hash_prng(u);
    return rand;
}

bool same_hemisphere(float3 a, float3 b){
    return (a.y * b.y) > 0;
}

float copy_sign(float num, float sign_num){
    num = num * sign(sign_num);
    return num;
}

// From: https://www.pbr-book.org/4ed/Geometry_and_Transformations/Spherical_Geometry
float2 sphere_coord_to_square_coord(float3 sphere_coords){

    float u = 0.5f + (atan2(sphere_coords.z, sphere_coords.x) / (2.0f * PI));
    float v = 0.5f - (asin(sphere_coords.y) / PI);
    return float2(u,v);
}

// From: https://www.pbr-book.org/4ed/Geometry_and_Transformations/Spherical_Geometry
float3 square_coord_to_sphere_coord(float2 square_coords){

    float theta = square_coords.x * 2 * PI;  // Azimuthal angle
    float phi = (1.0f - square_coords.y) * PI;  // Polar angle

    // Convert spherical coordinates to Cartesian coordinates
    float x = sin(phi) * cos(theta);
    float y = cos(phi);
    float z = sin(phi) * sin(theta);

    return float3(x, y, z);
}

float2 sample_uniform_disk_concentric(float2 u){
    float2 offset = 2 * u - 1; 
    if (offset.x == 0 && offset.y == 0){
        return offset;
    }

    float theta, r;
    if (abs(offset.x) > abs(offset.y)){
        r = offset.x;
        theta = PI/4 * (offset.y/offset.x);
    } else {
        r = offset.y;
        theta = PI/2 - PI/4 * (offset.x/offset.y);
    }
    return r * float2(cos(theta), sin(theta));
}

// Does the same thing as sample_uniform_disk_concentric, just with a different mapping. Also no branch.
float2 sample_uniform_disk_polar(float2 u){
    float r = sqrt(u[0]);
    float theta = 2 * PI * u[1];
    return float2(r * cos(theta), r * sin(theta));
}

float3 sample_cosign_hemisphere (float2 u){
    float2 d = sample_uniform_disk_concentric(u);
    float  z = max(0, sqrt(1 - (d.x * d.x) - (d.y * d.y)));
    return float3(d.x, d.y, z);
}

float3 cosign_hemisphere_pdf (float cos_theta){
    return cos_theta * INV_PI; // TODO
}

// https://www.pbr-book.org/4ed/Monte_Carlo_Integration/Improving_Efficiency#MultipleImportanceSampling
float power_huristic(float nf, float fpdf, float ng, float gpdf){
    float f = nf * fpdf;
    float g = ng * gpdf;
    return sqr(f) / (sqr(f) + sqr(g));
}

#endif // __UTILS__