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

// Per Model
Albedo
Metalic
Roughness
Optional: AO

*/

float4 main(PixelShaderInput IN) : SV_Target {
    float3 N = normalize(Normal);
    float3 V = normalize(CameraPosition - WorldPos);
    // ...

    light_position = float3(10., 10., 10.);
    light_color    = float3(1., 1., 1.);
    float3 Lo = float3(0.);
    // Only doing this once because we have one light
    for (int i = 0; i < 1; i++){
        float3 L = normalize(light_position - WorldPos);
        float3 H = normalize(V + L);

        float distance = length(light_position - WorldPos);
        float attenuation = 1.0 / (distance * distance);
        float3 radiance = light_color * attenuation;
    }
}