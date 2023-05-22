#pragma once
#include "pch.h"

struct Vertex_Position_Normal_Tangent_Color_Texturecoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};

struct Vertex_Position {
    DirectX::XMFLOAT3 position;
};

struct Per_Frame_Data {
    float light_pos[3] = {0., -2., -1.};    
    float padding = 0.;
    float light_color[3] = {20., 20., 20.};
    int   shadow_texture_index = 0;
    DirectX::XMMATRIX light_space_matrix;
    float camera_pos[3] = {0., 0., 0.};
};
