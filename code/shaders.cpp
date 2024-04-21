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

    /////////////////
    //  Input Layout
    /////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "NORMAL")     , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "COLOR")      , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TEXCOORD")   , DXGI_FORMAT_R32G32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TANGENT")    , DXGI_FORMAT_R32G32B32A32_FLOAT, 0});

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

    /////////////////////
    //  Input Layout
    /////////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "NORMAL")     , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "COLOR")      , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TEXCOORD")   , DXGI_FORMAT_R32G32_FLOAT, 0});
    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "TANGENT")    , DXGI_FORMAT_R32G32B32A32_FLOAT, 0});

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

    /////////////////////
    //  Input Layout
    /////////////////////

    shader_desc.input_layout.push_back({DSTR(per_frame_arena, "POSITION")   , DXGI_FORMAT_R32G32B32_FLOAT, 0});

    /////////////////////
    //  Render Targets
    /////////////////////

    DXGI_FORMAT rt_formats[] = {

        DXGI_FORMAT_R16G16B16A16_FLOAT

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

Shader* create_post_processing_shader()
{

    DEBUG_LOG("Creating Post Processing Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.type = Shader::Shader_Type::TYPE_COMPUTE;
    shader_desc.compute_shader = L"Deferred_Post_Processing.hlsl";

    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);
}

Shader* create_ssao_shader()
{

    DEBUG_LOG("Creating SSAO Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.type = Shader::Shader_Type::TYPE_COMPUTE;
    shader_desc.compute_shader = L"SSAO.hlsl";

    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);
}

Shader* create_compute_rayt_shader()
{

    DEBUG_LOG("Creating Compute Ray Tracing Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.type = Shader::Shader_Type::TYPE_COMPUTE;
    shader_desc.compute_shader = L"compute_rayt.hlsl";

    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);
}

Shader* create_dxr_rayt_shader()
{

    DEBUG_LOG("Creating DXR Ray Tracing Shader");

    //////////////////////////////////
    //  Create our shader / PSO
    //////////////////////////////////

    Shader_Desc shader_desc;

    //////////////////////////////////
    //  Set compiled shader code
    //////////////////////////////////

    shader_desc.type = Shader::Shader_Type::TYPE_RAY_TRACE;
    shader_desc.ray_trace_shader = L"raytracing.hlsl";

    /////////////////////////////////////////
    //  Create PSO using shader reflection
    /////////////////////////////////////////

    return create_shader(shader_desc);
}