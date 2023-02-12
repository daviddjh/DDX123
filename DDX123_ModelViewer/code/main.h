#pragma once
#include "pch.h"

#if 0
struct Vertex_Position_Color_Texcoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};

struct Vertex_Position_Tangent_Bitangent_Color_Texturecoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 bitangent;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};
#endif

struct Vertex_Position_Normal_Tangent_Color_Texturecoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 normal;
    DirectX::XMFLOAT3 tangent;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};