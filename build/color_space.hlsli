// Assumes input color is linear
// Tone maps colors between [0, 1)
// Washed out highlights
// Output color still needs gamma correction
float3 apply_reinhert(float3 color){
    return color / (color + float3(1.0, 1.0, 1.0));
}

// Assumes input color is linear
// Returns color in ~sRGB color space
// "Gamma Corrected" - (Inverse Electrical Optical Transfer Function)
float3 apply_srgb_curve(float3 color){
    return pow(color, float3(1.0/2.2, 1.0/2.2, 1.0/2.2));
}

// ACES Film curve from: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 apply_aces_film_curve(float3 color)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((color * (a * color +b)) / (color * (c * color + d) + e));
}

// TODO add rec2020 curve - for HDR
