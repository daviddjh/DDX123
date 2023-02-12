#pragma once

#include "pch.h"
#include "main.h"
#include "d_dx12.h"

enum D_Material_Flags : u8{
    MATERIAL_FLAG_NONE           = 0x0,
    MATERIAL_FLAG_NORMAL_TEXTURE = 0x1
};

struct D_Texture {

    d_std::Span<u8>  cpu_texture_data;
    d_dx12::Texture_Desc texture_desc;
    d_dx12::Texture* texture = NULL;

};

struct D_Material {

    D_Texture albedo_texture;
    D_Texture normal_texture;
    u32        material_flags = MATERIAL_FLAG_NONE;

};

struct D_Primitive_Group {

    D3D_PRIMITIVE_TOPOLOGY primitive_topology;
    d_std::Span<Vertex_Position_Normal_Tangent_Color_Texturecoord> verticies;
    d_std::Span<u16> indicies;
    u16 material_index = -1;

};

struct D_Draw_Call {

    int material_index;
    int vertex_offset;
    int index_offset;
    int index_count;

};

struct D_Mesh {

    d_std::Span<D_Primitive_Group> primitive_groups;
    d_std::Span<D_Draw_Call> draw_calls;
    d_dx12::Buffer* vertex_buffer;
    d_dx12::Buffer* index_buffer;

};

struct D_Model {

    d_std::Span<D_Material> materials;
    d_std::Span<D_Mesh> meshes;
    DirectX::XMFLOAT3 coords;

};

void load_gltf_model(D_Model& d_model, const char* filename);