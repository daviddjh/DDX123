#include "pch.h"
#include "d_dx12.h"
#include "main.h"

using namespace d_std;
using namespace d_dx12;

// This file is ment for isolation of pipeline creation and shader management

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Forward Render PBR Shader
////////////////////////////////////////////////////////////////////////////////////////////////////

Shader* create_forward_render_pbr_shader()
{

    DEBUG_LOG("Creating Forward Rendering PBR shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.vertex_shader = L"PBRVertexShader.hlsl";
    shader_desc.pixel_shader  = L"PBRPixelShader.hlsl";


    //////////////////////////////////
    //  Specify our shader Parameters
    //////////////////////////////////

    // Sampler Parameter
    Shader_Desc::Parameter::Static_Sampler_Desc sampler_1_ssd;
    sampler_1_ssd.filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_1_ssd.comparison_func  = D3D12_COMPARISON_FUNC_NEVER;
    sampler_1_ssd.min_lod          = 0;
    sampler_1_ssd.max_lod          = D3D12_FLOAT32_MAX;

    Shader_Desc::Parameter sampler_1;
    sampler_1.name                = DSTR(per_frame_arena, "sampler_1");
    sampler_1.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_STATIC_SAMPLER;
    sampler_1.static_sampler_desc = sampler_1_ssd;

    shader_desc.parameter_list.push_back(sampler_1);

    // Bindless Texture Table - Where all our texture descriptors should go. Index is used by our shader to retrieve texture
    Shader_Desc::Parameter texture_2d_table;
    texture_2d_table.name                = DSTR(per_frame_arena, "texture_2d_table");
    texture_2d_table.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ;

    shader_desc.parameter_list.push_back(texture_2d_table);


    // Material Flags
    Shader_Desc::Parameter material_flags;
    material_flags.name                   = DSTR(per_frame_arena, "material_flags");
    material_flags.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    material_flags.number_of_32bit_values = 1;

    shader_desc.parameter_list.push_back(material_flags);

    // Albedo Index
    Shader_Desc::Parameter albedo_index;
    albedo_index.name                   = DSTR(per_frame_arena, "albedo_index");
    albedo_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    albedo_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(albedo_index);

    // Normal Index
    Shader_Desc::Parameter normal_index;
    normal_index.name                   = DSTR(per_frame_arena, "normal_index");
    normal_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    normal_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(normal_index);

    // Roughness-Metallic Index
    Shader_Desc::Parameter roughness_metallic_index;
    roughness_metallic_index.name                   = DSTR(per_frame_arena, "roughness_metallic_index");
    roughness_metallic_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    roughness_metallic_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(roughness_metallic_index);

    // Model Matrix
    Shader_Desc::Parameter model_matrix;
    model_matrix.name                   = DSTR(per_frame_arena, "model_matrix");
    model_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    model_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;
    shader_desc.parameter_list.push_back(model_matrix);

    // View Matrix
    Shader_Desc::Parameter view_projection_matrix;
    view_projection_matrix.name                   = DSTR(per_frame_arena, "view_projection_matrix");
    view_projection_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    view_projection_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;

    shader_desc.parameter_list.push_back(view_projection_matrix);

    // Camera Pos
    Shader_Desc::Parameter camera_pos;
    camera_pos.name                   = DSTR(per_frame_arena, "camera_position_buffer");
    camera_pos.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    camera_pos.number_of_32bit_values = sizeof(DirectX::XMVECTOR);

    shader_desc.parameter_list.push_back(camera_pos);

    // Light Pos
    Shader_Desc::Parameter per_frame_data;
    per_frame_data.name                   = DSTR(per_frame_arena, "per_frame_data");
    per_frame_data.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_CONSTANT_BUFFER;
    per_frame_data.number_of_32bit_values = sizeof(Per_Frame_Data) / 4;

    shader_desc.parameter_list.push_back(per_frame_data);

    /////////////////
    //  Input Layout
    /////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "NORMAL")     , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TANGENT")    , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "COLOR")      , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TEXCOORD")   , DXGI_FORMAT_R32G32_FLOAT, 0});

    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Deferred Rendering G-Buffer Shader
////////////////////////////////////////////////////////////////////////////////////////////////////

Shader* create_deferred_render_gbuffer_shader()
{
    DEBUG_LOG("Creating Deferred Rendering G-Buffer Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.vertex_shader = L"Deferred_Gbuffer_Vertex_Shader.hlsl";
    shader_desc.pixel_shader  = L"Deferred_Gbuffer_Pixel_Shader.hlsl";

    //////////////////////////////////
    //  Specify our shader Parameters
    //////////////////////////////////

    // Sampler Parameter
    Shader_Desc::Parameter::Static_Sampler_Desc sampler_1_ssd;
    sampler_1_ssd.filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_1_ssd.comparison_func  = D3D12_COMPARISON_FUNC_NEVER;
    sampler_1_ssd.min_lod          = 0;
    sampler_1_ssd.max_lod          = D3D12_FLOAT32_MAX;

    Shader_Desc::Parameter sampler_1;
    sampler_1.name                = DSTR(per_frame_arena, "sampler_1");
    sampler_1.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_STATIC_SAMPLER;
    sampler_1.static_sampler_desc = sampler_1_ssd;

    shader_desc.parameter_list.push_back(sampler_1);

    // Bindless Texture Table - Where all our texture descriptors should go. Index is used by our shader to retrieve texture
    Shader_Desc::Parameter texture_2d_table;
    texture_2d_table.name                = DSTR(per_frame_arena, ("texture_2d_table"));
    texture_2d_table.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ;

    shader_desc.parameter_list.push_back(texture_2d_table);

    // Material Flags
    Shader_Desc::Parameter material_flags;
    material_flags.name                   = DSTR(per_frame_arena, "material_flags");
    material_flags.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    material_flags.number_of_32bit_values = 1;

    shader_desc.parameter_list.push_back(material_flags);

    // Albedo Index
    Shader_Desc::Parameter albedo_index;
    albedo_index.name                   = DSTR(per_frame_arena, "albedo_index");
    albedo_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    albedo_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(albedo_index);

    // Normal Index
    Shader_Desc::Parameter normal_index;
    normal_index.name                   = DSTR(per_frame_arena, "normal_index");
    normal_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    normal_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(normal_index);

    // Roughness-Metallic Index
    Shader_Desc::Parameter roughness_metallic_index;
    roughness_metallic_index.name                   = DSTR(per_frame_arena, "roughness_metallic_index");
    roughness_metallic_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    roughness_metallic_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(roughness_metallic_index);

    // Model Matrix
    Shader_Desc::Parameter model_matrix;
    model_matrix.name                   = DSTR(per_frame_arena, "model_matrix");
    model_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    model_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;
    shader_desc.parameter_list.push_back(model_matrix);

    // View Matrix
    Shader_Desc::Parameter view_projection_matrix;
    view_projection_matrix.name                   = DSTR(per_frame_arena, "view_projection_matrix");
    view_projection_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    view_projection_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;

    shader_desc.parameter_list.push_back(view_projection_matrix);

    // Camera Pos
    Shader_Desc::Parameter camera_pos;
    camera_pos.name                   = DSTR(per_frame_arena, "camera_position_buffer");
    camera_pos.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    camera_pos.number_of_32bit_values = sizeof(DirectX::XMVECTOR);

    shader_desc.parameter_list.push_back(camera_pos);

    /////////////////////
    //  Input Layout
    /////////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "NORMAL")     , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TANGENT")    , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "COLOR")      , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TEXCOORD")   , DXGI_FORMAT_R32G32_FLOAT, 0});

    /////////////////////
    //  Render Targets
    /////////////////////

    DXGI_FORMAT rt_formats[] = {

        DXGI_FORMAT_R8G8B8A8_UNORM,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R16G16B16A16_FLOAT,
        DXGI_FORMAT_R16G16B16A16_FLOAT

    };
    shader_desc.render_target_formats = rt_formats;
    shader_desc.num_render_targets    = sizeof(rt_formats)/sizeof(rt_formats[0]);


    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Deferred Rendering Shading Shader (2nd Pass)
////////////////////////////////////////////////////////////////////////////////////////////////////

Shader* create_deferred_render_shading_shader()
{
    DEBUG_LOG("Creating Deferred Shading Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.vertex_shader = L"Deferred_Shading_Vertex_Shader.hlsl";
    shader_desc.pixel_shader  = L"Deferred_Shading_Pixel_Shader.hlsl";

    //////////////////////////////////
    //  Specify our shader Parameters
    //////////////////////////////////

    // Sampler Parameter
    Shader_Desc::Parameter::Static_Sampler_Desc sampler_1_ssd;
    sampler_1_ssd.filter           = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler_1_ssd.comparison_func  = D3D12_COMPARISON_FUNC_NEVER;
    sampler_1_ssd.min_lod          = 0;
    sampler_1_ssd.max_lod          = D3D12_FLOAT32_MAX;

    Shader_Desc::Parameter sampler_1;
    sampler_1.name                = DSTR(per_frame_arena, "sampler_1");
    sampler_1.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_STATIC_SAMPLER;
    sampler_1.static_sampler_desc = sampler_1_ssd;

    shader_desc.parameter_list.push_back(sampler_1);

    // Bindless Texture Table - Where all our texture descriptors should go. Index is used by our shader to retrieve texture
    Shader_Desc::Parameter texture_2d_table;
    texture_2d_table.name                = DSTR(per_frame_arena, "texture_2d_table");
    texture_2d_table.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ;

    shader_desc.parameter_list.push_back(texture_2d_table);

    // Albedo Index
    Shader_Desc::Parameter albedo_index;
    albedo_index.name                   = DSTR(per_frame_arena, "albedo_index");
    albedo_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    albedo_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(albedo_index);

    // Position Index
    Shader_Desc::Parameter position_index;
    position_index.name                   = DSTR(per_frame_arena, "position_index");
    position_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    position_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(position_index);

    // Normal Index
    Shader_Desc::Parameter normal_index;
    normal_index.name                   = DSTR(per_frame_arena, "normal_index");
    normal_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    normal_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(normal_index);

    // Roughness-Metallic Index
    Shader_Desc::Parameter roughness_metallic_index;
    roughness_metallic_index.name                   = DSTR(per_frame_arena, "roughness_metallic_index");
    roughness_metallic_index.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    roughness_metallic_index.number_of_32bit_values = 1;
    shader_desc.parameter_list.push_back(roughness_metallic_index);

    // Deferred Shading Output Dimensions
    Shader_Desc::Parameter output_dimensions;
    output_dimensions.name                   = DSTR(per_frame_arena, "output_dimensions");
    output_dimensions.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    output_dimensions.number_of_32bit_values = 2;
    shader_desc.parameter_list.push_back(output_dimensions);

    // Per Frame Data
    Shader_Desc::Parameter per_frame_data;
    per_frame_data.name                   = DSTR(per_frame_arena, "per_frame_data");
    per_frame_data.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_CONSTANT_BUFFER;
    per_frame_data.number_of_32bit_values = sizeof(Per_Frame_Data) / 4;

    shader_desc.parameter_list.push_back(per_frame_data);

    /////////////////////
    //  Input Layout
    /////////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});

    /////////////////////
    //  Render Targets
    /////////////////////

    DXGI_FORMAT rt_formats[] = {

        DXGI_FORMAT_R8G8B8A8_UNORM

    };
    shader_desc.render_target_formats = rt_formats;
    shader_desc.num_render_targets    = sizeof(rt_formats)/sizeof(rt_formats[0]);
    shader_desc.depth_buffer_enabled  = false;


    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);

}

////////////////////////////////////////////////////////////////////////////////////////////////////
//  Shadow Map Shader
////////////////////////////////////////////////////////////////////////////////////////////////////

Shader* create_shadow_mapping_shader()
{
    DEBUG_LOG("Creating Shadow Mapping Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.vertex_shader = L"ShadowMapVertexShader.hlsl";
    shader_desc.pixel_shader  = L"ShadowMapPixelShader.hlsl";


    // Model Matrix
    Shader_Desc::Parameter model_matrix;
    model_matrix.name                   = DSTR(per_frame_arena, "model_matrix");
    model_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    model_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;
    shader_desc.parameter_list.push_back(model_matrix);

    // Light Space Matrix
    Shader_Desc::Parameter light_space_matrix;
    light_space_matrix.name                   = DSTR(per_frame_arena, "light_matrix");
    light_space_matrix.usage_type             = Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT;
    light_space_matrix.number_of_32bit_values = sizeof(DirectX::XMMATRIX) / 4;
    shader_desc.parameter_list.push_back(light_space_matrix);


    /////////////////
    //  Input Layout
    /////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "NORMAL"  )   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TANGENT" )   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "COLOR"   )   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TEXCOORD")   , DXGI_FORMAT_R32G32_FLOAT, 0});


    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);

}
