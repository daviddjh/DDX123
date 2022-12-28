#pragma once

#include "pch.h"
#include "main.h"
#include "d_dx12.h"

struct D_Material {
    d_std::Span<u8>  cpu_texture_data;
    d_dx12::Texture_Desc texture_desc;
    d_dx12::Texture* texture;
};

struct D_Primitive_Group {
    D3D_PRIMITIVE_TOPOLOGY primitive_topology;
    d_std::Span<Vertex_Position_Color_Texcoord> verticies;
    d_std::Span<u16> indicies;
    u16 material_index = -1;
    d_dx12::Buffer* vertex_buffer;
    d_dx12::Buffer* index_buffer;
};

struct D_Mesh {
    d_std::Span<D_Primitive_Group> primitive_groups;
};

struct D_Model {
    d_std::Span<D_Material> materials;
    d_std::Span<D_Mesh> meshes;
    DirectX::XMFLOAT3 coords;
};

void load_gltf_model(D_Model& d_model, const char* filename);