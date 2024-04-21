#include "common.hlsli"

// Define the group size
#define GROUP_SIZE_X 8
#define GROUP_SIZE_Y 8

RWTexture2D<float4> texture_2d_uav_table[] : register(u0, space99);

ConstantBuffer<Texture_Index> output_texture_index  : register(b0, ComputeSpace);
ConstantBuffer<Output_Dimensions> output_dimensions : register(b1, ComputeSpace);
// ConstantBuffer<Texture_Index> input_texture_index  : register(b1, ComputeSpace);
// ConstantBuffer<Texture_Index> ssao_texture_index   : register(b2, ComputeSpace);

static float3 background_color = float3(0.4, 0.5, 0.3);
static float3 camera_center = float3(0., 0., 0.);
static float3 up_dir = float4(0., 1., 0., 0.);
static float3 right_dir = float4(1., 0., 0., 0.);
static float  focal_length = 1.;

struct Ray {
    float3 origin;
    float3 direction;
    float3 at(float t){
        return origin + (t*direction);
    }
};

float hit_sphere(const float3 sphere_center, float radius, const Ray ray){
    
    // Quadratic equation solve for sphere equation
    // To determine if the ray hit the sphere

    float3 oc = ray.origin - sphere_center;
    float a = dot(ray.direction, ray.direction);
    float b = 2.0 * dot(oc, ray.direction);
    float c = dot(oc, oc) - radius * radius;
    float discriminant = b*b - 4*a*c;
    if (discriminant < 0.){
        return -1.0;
    } else {
        return (-b - sqrt(discriminant)) / (2.0 * a);
    }
}

float3 ray_color(const Ray ray){

    float t = hit_sphere(float3(0, 0, -1), 0.5, ray);
    if (t > 0.0){
        float3 N = normalize(ray.at(t) - float3(0, 0, -1));
        return 0.5 * float3(N.x + 1, N.y + 1, N.z + 1);
    }
    float a = 0.5 * (ray.direction.y + 1.0);
    return ((1. - a) * float3(1.0, 1.0, 1.0)) + (a * float3(0.5, 0.7, 1.0)); 

}

// The compute shader
[numthreads(GROUP_SIZE_X, GROUP_SIZE_Y, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    float viewport_height = 2.0;
    float viewport_width  = viewport_height * ((float)output_dimensions.width / (float)output_dimensions.height) ;
    float3 viewport_u = float3(viewport_width, 0, 0);
    float3 viewport_v = float3(0, -viewport_height, 0);
    float3 pixel_delta_u = viewport_u / output_dimensions.width;
    float3 pixel_delta_v = viewport_v / output_dimensions.height;

    float3 viewport_upper_left = camera_center - float3(0, 0, focal_length) - (viewport_u / 2) - (viewport_v / 2);
    float3 pixel00_loc = viewport_upper_left + 0.5*(pixel_delta_u + pixel_delta_v);

    float2 xy   = float2(DTid.xy);

    float3 pixel_center = pixel00_loc + (xy.x * pixel_delta_u) + (xy.y * pixel_delta_v);
    float3 ray_direction = normalize(pixel_center - camera_center);

    Ray ray = {float3(0, 0, 0), ray_direction}; 

    float3 color = ray_color(ray); 
    
    // Write to output texture
    texture_2d_uav_table[output_texture_index.texture_index][xy] = float4(color, 1.0);
}