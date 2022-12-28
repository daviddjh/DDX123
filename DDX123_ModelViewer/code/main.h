#pragma once
#include "pch.h"

struct Vertex_Position_Color_Texcoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};
