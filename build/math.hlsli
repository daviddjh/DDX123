#ifndef __MATH__
#define __MATH__

float sqr(float a){
    return a * a;
}

// Functions for spherical paramerterization of vector w:
float cos_theta(float3 w){
    return w.z;
}

float cos_2_theta(float3 w){
    return w.z * w.z;
}

float sin_2_theta(float3 w){
    return max(0, 1-cos_2_theta(w));
}

float sin_theta(float3 w){
    return sqrt(sin_2_theta(w));
}

float tan_2_theta(float3 w){
    return sin_2_theta(w) / cos_2_theta(w);
}

float cos_phi(float3 w){
    float _sin_theta = sin_theta(w);
    return (_sin_theta == 0) ? 0 : clamp(w.x / _sin_theta, -1, 1);
}

float sin_phi(float3 w){
    float _sin_theta = sin_theta(w);
    return (_sin_theta == 0) ? 0 : clamp(w.y / _sin_theta, -1, 1);
}

float abs_cos_theta(float3 w){
    return abs(w.z);
}

#endif //__MATH__
