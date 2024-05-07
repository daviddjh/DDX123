#include "pch.h"
#include "main.h"

#include "d_dx12.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#include "nv_helpers_dx12/ShaderBindingTableGenerator.h"
#include "nv_helpers_dx12/TopLevelASGenerator.h"
#include "nv_helpers_dx12/BottomLevelASGenerator.h"
#include "nv_helpers_dx12/DXRHelper.h"

#include <chrono>
#include <random>
#include "timeapi.h"

#include "d_core.cpp"
#include "d_dx12.cpp"
#include "model.cpp"
#include "shaders.cpp"
#include "constant_buffers.h"

#include "nv_helpers_dx12/ShaderBindingTableGenerator.cpp"
#include "nv_helpers_dx12/TopLevelASGenerator.cpp"
#include "nv_helpers_dx12/BottomLevelASGenerator.cpp"


using namespace d_dx12;
using namespace d_std;
using namespace DirectX;

#define BUFFER_OFFSET(i) ((char *)0 + (i))

// Sets window, rendertargets to 4k resolution
// #define d_4k
// #define NVIDIA

struct D_Camera {
    DirectX::XMVECTOR eye_position;
    DirectX::XMVECTOR eye_direction;
    DirectX::XMVECTOR up_direction;
    float speed = 1.8;
    //float fov   = 100.;
    float fov   = 75.;
};

struct D_Shaders {
    Shader*           pbr_shader;
    Shader*           deferred_g_buffer_shader;
    Shader*           deferred_shading_shader;
    Shader*           shadow_map_shader;
    Shader*           ssao_shader;
    Shader*           post_processing_shader;
    Shader*           compute_rayt_shader;
    Shader*           dxr_rayt_shader;
};

struct D_Textures {
    Texture*          rt[NUM_BACK_BUFFERS];
    Texture*          ds;
    Texture*          shadow_ds;
    Texture*          g_buffer_position;
    Texture*          g_buffer_albedo;
    Texture*          g_buffer_normal;
    Texture*          g_buffer_rough_metal;
    Texture*          sampled_texture;
    Texture*          ssao_rotation_texture;
    Texture*          ssao_output_texture;
    Texture*          main_render_target;    // Size of render resolution - input to post processing
    Texture*          main_output_target;    // Size of output resolution - output of post processing
};

struct D_Buffers{
    Buffer*           full_screen_quad_vertex_buffer;
    Buffer*           full_screen_quad_index_buffer;
    Buffer*           ssao_sample_kernel;
};

enum D_Render_Passes : u8 {
    FORWARD_SHADING,
    DEFERRED_SHADING,
    RAY_TRACING_COMPUTE,
    DXR_RAY_TRACING,
    NUM_RENDER_PASSES
};

static const char* render_pass_names[NUM_RENDER_PASSES] = {
    "Forward_Shading",
    "Deffered_Shading",
    "Ray_Tracing_Compute",
    "DXR_Ray_Tracing",
};

struct D_Renderer_Config {
    bool fullscreen_mode = false;
    bool imgui_demo      = false;         
    u8   render_pass     = D_Render_Passes::DXR_RAY_TRACING;

    #ifdef d_4k
    u16 display_width  = 3840;
    u16 display_height = 2160;
    u16 render_width   = 3840;
    u16 render_height  = 2160;
    #else
    u16 display_width  = 1920;
    u16 display_height = 1080;
    u16 render_width   = 1920;
    u16 render_height  = 1080;
    #endif

};

struct Vertex {
    XMFLOAT3 position;
    XMFLOAT4 color;
};


// Global Vars, In order of creation
struct D_Renderer {
    HWND              hWnd;
    Resource_Manager  resource_manager;
    Command_List*     direct_command_lists[NUM_BACK_BUFFERS];
    D_Shaders         shaders;
    Shader*           shader_array[sizeof(D_Shaders) / sizeof(Shader*)];
    D_Textures        textures;
    D_Buffers         buffers;
    D_Camera          camera;
    D_Renderer_Config config;

    Descriptor_Handle imgui_font_handle;
    RECT              window_rect;
    Span<D_Model>     models;
    Per_Frame_Data    per_frame_data;

    nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_sbtStorage;
    Upload_Buffer::Allocation sbt_upload_storage;
    float m_aspectRatio = 1920. / 1080.;
    Microsoft::WRL::ComPtr<ID3D12Resource> m_vertexBuffer;
    D3D12_VERTEX_BUFFER_VIEW m_vertexBufferView;

    std::vector<Buffer*> blases;
    Buffer* tlas;
    Buffer* instance_buffer;
    
    struct Geometry_Info {
        u32               vertex_offset;
        u32               index_byte_offset;
        u32               material_id;
        u32               material_flags;
        // DirectX::XMMATRIX model_matrix;
    };

    Span<Geometry_Info> geometry_info;
    Buffer*   geometry_info_gpu;



    struct AccelerationStructureBuffers {
        Microsoft::WRL::ComPtr<ID3D12Resource> pScratch;      // Scratch memory for AS builder
        Microsoft::WRL::ComPtr<ID3D12Resource> pResult;       // Where the AS is
        Microsoft::WRL::ComPtr<ID3D12Resource> pInstanceDesc; // Hold the matrices of the instances
    };

    Microsoft::WRL::ComPtr<ID3D12Resource> m_bottomLevelAS; // Storage for the bottom Level AS

    nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
    AccelerationStructureBuffers m_topLevelASBuffers;
    std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>> m_instances;

    /// Create the acceleration structure of an instance
    ///
    /// \param     vVertexBuffers : pair of buffer and vertex count
    /// \return    AccelerationStructureBuffers for TLAS
    AccelerationStructureBuffers CreateBottomLevelAS(
        std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t, uint32_t>> vVertexBuffers, Command_List* command_list);

    /// Create the main acceleration structure that holds
    /// all instances of the scene
    /// \param     instances : pair of BLAS and transform
    void CreateTopLevelAS(
        const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>
            &instances, Command_List* command_list);








    int  init();
    void render();
    void render_shadow_map(Command_List* command_list);
    void forward_render_pass(Command_List* command_list);
    void deferred_render_pass(Command_List* command_list);
    void compute_rayt_pass(Command_List* command_list);
    void dxr_ray_tracing_pass(Command_List* command_list);
    void shutdown();
    void toggle_fullscreen();
    void upload_model_to_gpu(Command_List* command_list, D_Model& test_model);
    void bind_and_draw_model(Command_List* command_list, D_Model* model);
    void calc_acceleration_structure(Command_List* command_list);
    void build_shader_tables(Command_List* command_list, Shader* shader);
};

D_Renderer renderer;
Memory_Arena *per_frame_arena;
bool application_is_initialized = false;

#define MAX_TICK_SAMPLES 20
int tick_index = 0;
double tick_sum = 0.;
double *tick_list = NULL;

/*******************/

bool using_v_sync = false;
bool capturing_mouse = false;

void D_Renderer::upload_model_to_gpu(Command_List* command_list, D_Model& test_model){

    // Strategy: Each primative group gets it's own vertex buffer?
    // Not good, but will do for now
    // ... new stratagy, each mesh gets it's own buffer ..?
    // ++

    //////////////////////
    // Model Buffers
    //////////////////////

    // For each mesh
    for(u64 i = 0; i < test_model.meshes.nitems; i++){

        D_Mesh* mesh = test_model.meshes.ptr + i;

        // Get the number of verticies in this mesh
        u64 number_of_verticies = 0;
        u64 number_of_indicies  = 0;
        for(u64 j = 0; j < mesh->primitive_groups.nitems; j++){

            D_Primitive_Group* primitive_group = mesh->primitive_groups.ptr + j;
            number_of_verticies +=  primitive_group->verticies.nitems;
            number_of_indicies  +=  primitive_group->indicies.nitems;

        }
        mesh->primitive_count = number_of_verticies;
        mesh->index_count     = number_of_indicies;

        // Allocate space for the draw calls
        mesh->draw_calls.alloc(mesh->primitive_groups.nitems);

        // Allocate cpu space for the verticies
        Span<Vertex_Position_Normal_Tangent_Color_Texturecoord> verticies_buffer;
        verticies_buffer.alloc(number_of_verticies);

        // Copy the verticies in all primitive groups to this buffer
        Vertex_Position_Normal_Tangent_Color_Texturecoord* start_vertex_ptr = verticies_buffer.ptr;
        Vertex_Position_Normal_Tangent_Color_Texturecoord* current_vertex_ptr = start_vertex_ptr;

        // Allocate cpu space for the indicies
        Span<u16> indicies_buffer;
        indicies_buffer.alloc(number_of_indicies);

        // Copy the verticies in all primitive groups to this buffer
        u16* start_index_ptr = indicies_buffer.ptr;
        u16* current_index_ptr = start_index_ptr;

        for(u64 j = 0; j < mesh->primitive_groups.nitems; j++){

            // Get the primitive group
            D_Primitive_Group* primitive_group = mesh->primitive_groups.ptr + j;

            // Copy the verticies
            memcpy(current_vertex_ptr, primitive_group->verticies.ptr, primitive_group->verticies.nitems * sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord));

            // Copy the indicies
            memcpy(current_index_ptr, primitive_group->indicies.ptr, primitive_group->indicies.nitems * sizeof(u16));

            // Update draw_call information
            mesh->draw_calls.ptr[j].index_count    = primitive_group->indicies.nitems;
            mesh->draw_calls.ptr[j].index_offset   = current_index_ptr - start_index_ptr;
            mesh->draw_calls.ptr[j].vertex_count   = primitive_group->verticies.nitems;
            mesh->draw_calls.ptr[j].vertex_offset  = current_vertex_ptr - start_vertex_ptr;
            mesh->draw_calls.ptr[j].material_index = primitive_group->material_index;

            // Update pointers
            current_vertex_ptr += primitive_group->verticies.nitems;
            current_index_ptr  += primitive_group->indicies.nitems;
            
        }

        // Vertex buffer
        Buffer_Desc vertex_buffer_desc = {};
        vertex_buffer_desc.number_of_elements = verticies_buffer.nitems;
        vertex_buffer_desc.size_of_each_element = sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord);
        vertex_buffer_desc.usage = Buffer::USAGE::USAGE_VERTEX_BUFFER;
        // vertex_buffer_desc.alignment = 4;

        mesh->vertex_buffer = resource_manager.create_buffer(L"Vertex Buffer", vertex_buffer_desc);

        command_list->load_buffer(mesh->vertex_buffer, (u8*)verticies_buffer.ptr, verticies_buffer.nitems * sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord), sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord));//Fix: sizeof(Vertex_Position_Color));

        // Index Buffer
        Buffer_Desc index_buffer_desc = {};
        index_buffer_desc.number_of_elements = indicies_buffer.nitems;
        index_buffer_desc.size_of_each_element = sizeof(u16);
        index_buffer_desc.usage = Buffer::USAGE::USAGE_INDEX_BUFFER;
        // index_buffer_desc.alignment = 2;

        mesh->index_buffer = resource_manager.create_buffer(L"Index Buffer", index_buffer_desc);

        command_list->load_buffer(mesh->index_buffer, (u8*)indicies_buffer.ptr, indicies_buffer.nitems * sizeof(u16), sizeof(u16));

        indicies_buffer.d_free();
        verticies_buffer.d_free();
    }

    //////////////////////
    //  Materials
    //////////////////////
    for(u32 material_index = 0; material_index < test_model.materials.nitems; material_index++){

        D_Material& material = test_model.materials.ptr[material_index];

        // Albedo Texture
        // TODO: This name is not useful, however I do not know how to handle wchar_t that resource needs, vs char that tinygltf gives me
        material.albedo_texture.texture = resource_manager.create_texture(L"Albedo_Texture", material.albedo_texture.texture_desc);

        if(material.albedo_texture.cpu_texture_data.ptr){
            command_list->load_decoded_texture_from_memory(material.albedo_texture.texture, (u_ptr)material.albedo_texture.cpu_texture_data.ptr, true);
        }

        // Normal Map
        // TODO: This name is not useful, however I do not know how to handle wchar_t that resource needs, vs char that tinygltf gives me
        if(material.material_flags & MATERIAL_FLAG_NORMAL_TEXTURE){
            material.normal_texture.texture = resource_manager.create_texture(L"Normal_Texture", material.normal_texture.texture_desc);

            if(material.normal_texture.cpu_texture_data.ptr){
                command_list->load_decoded_texture_from_memory(material.normal_texture.texture, (u_ptr)material.normal_texture.cpu_texture_data.ptr, true);
            }
        }

        // RoughnessMetallic Map
        // TODO: This name is not useful, however I do not know how to handle wchar_t that resource needs, vs char that tinygltf gives me
        if(material.material_flags & MATERIAL_FLAG_ROUGHNESSMETALLIC_TEXTURE){
            material.roughness_metallic_texture.texture = resource_manager.create_texture(L"RoughnessMetallic_Texture", material.roughness_metallic_texture.texture_desc);

            if(material.roughness_metallic_texture.cpu_texture_data.ptr){
                command_list->load_decoded_texture_from_memory(material.roughness_metallic_texture.texture, (u_ptr)material.roughness_metallic_texture.cpu_texture_data.ptr, true);
            }
        }
    }
}

void D_Renderer::bind_and_draw_model(Command_List* command_list, D_Model* model){

    DirectX::XMMATRIX model_matrix = DirectX::XMMatrixIdentity();
    DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScaling(0.1, 0.1, 0.1);
    model_matrix = DirectX::XMMatrixMultiply(scale_matrix, DirectX::XMMatrixIdentity());
    DirectX::XMMATRIX translation_matrix = DirectX::XMMatrixTranslation(model->coords.x, model->coords.y, model->coords.z);
    model_matrix = DirectX::XMMatrixMultiply(translation_matrix, model_matrix);
    //command_list->bind_constant_arguments(&model_matrix, sizeof(DirectX::XMMATRIX) / 4, binding_point_string_lookup("model_matrix"));
    Descriptor_Handle model_matrix_handle = resource_manager.load_dyanamic_frame_data((void*)&model_matrix, sizeof(DirectX::XMMATRIX), 256);
    command_list->bind_handle(model_matrix_handle, binding_point_string_lookup("model_matrix"));

    // Begin by initializing each textures binding table index to an invalid value
    for(u64 i = 0; i < model->materials.nitems; i++){
        model->materials.ptr[i].albedo_texture.texture_binding_table_index = -1;
        model->materials.ptr[i].normal_texture.texture_binding_table_index = -1;
        model->materials.ptr[i].roughness_metallic_texture.texture_binding_table_index = -1;
    }

    // Then, we copy all the texture descriptors to the texture table in online_descriptor_heap
    for(u64 i = 0; i < model->meshes.nitems; i++){
        D_Mesh* mesh = model->meshes.ptr + i;

        // For each draw call
        for(u64 j = 0; j < mesh->draw_calls.nitems; j++){

            D_Draw_Call draw_call = mesh->draw_calls.ptr[j];
            
            // Check if table index is < 0 (-1). Only bind albedo texture if it needs to be bound to avoid unncessary descriptor copies
            model->materials.ptr[draw_call.material_index].albedo_texture.texture_binding_table_index = command_list->bind_texture(model->materials.ptr[draw_call.material_index].albedo_texture.texture, &resource_manager, 0);

            // If this material uses a normal map
            if(model->materials.ptr[draw_call.material_index].material_flags & MATERIAL_FLAG_NORMAL_TEXTURE){

                // Check if table index is < 0 (-1). Only bind albedo texture if it needs to be bound to avoid unncessary descriptor copies
                model->materials.ptr[draw_call.material_index].normal_texture.texture_binding_table_index = command_list->bind_texture(model->materials.ptr[draw_call.material_index].normal_texture.texture, &resource_manager, 0);

            }

            // If this material uses roughness metallic map
            if(model->materials.ptr[draw_call.material_index].material_flags & MATERIAL_FLAG_ROUGHNESSMETALLIC_TEXTURE){

                // Check if table index is < 0 (-1). Only bind albedo texture if it needs to be bound to avoid unncessary descriptor copies
                model->materials.ptr[draw_call.material_index].roughness_metallic_texture.texture_binding_table_index = command_list->bind_texture(model->materials.ptr[draw_call.material_index].roughness_metallic_texture.texture, &resource_manager, 0);

            }
        }
    }

    // Now be bind the texture table to the root signature. ONLY if the shader wants textures
    // If the "texture_2d_table" binding point isn't in the list of availible binding points, we assume the shader doesn't need textures binded
    constexpr u32 tex_2d_table_index = binding_point_string_lookup("texture_2d_table");
    if(command_list->current_bound_shader->binding_points[tex_2d_table_index].input_type != Shader::Input_Type::TYPE_INVALID){
        command_list->bind_online_descriptor_heap_texture_table(&resource_manager, tex_2d_table_index);
    }

    for(u64 i = 0; i < model->meshes.nitems; i++){
        D_Mesh* mesh = model->meshes.ptr + i;

        // Bind the buffers to the command list somewhere in the root signature
        command_list->bind_vertex_buffer(mesh->vertex_buffer, 0);
        command_list->bind_index_buffer(mesh->index_buffer);

        // For each draw call
        for(u64 j = 0; j < mesh->draw_calls.nitems; j++){

            D_Draw_Call draw_call = mesh->draw_calls.ptr[j];

            D_Material material = model->materials.ptr[draw_call.material_index];


            constexpr u32 material_data_index = binding_point_string_lookup("material_data");
            if(command_list->current_bound_shader->binding_points[material_data_index].input_type != Shader::Input_Type::TYPE_INVALID){

                // Set values for material data cbuffer

                Material_Data material_data_for_shader = {};

                material_data_for_shader.albedo_index             = material.albedo_texture.texture_binding_table_index;
                material_data_for_shader.flags                    = material.material_flags;

                if(material.material_flags & MATERIAL_FLAG_NORMAL_TEXTURE)
                    material_data_for_shader.normal_index             = material.normal_texture.texture_binding_table_index;

                if(material.material_flags & MATERIAL_FLAG_ROUGHNESSMETALLIC_TEXTURE)
                    material_data_for_shader.roughness_metallic_index = material.roughness_metallic_texture.texture_binding_table_index;


                // Upload and bind buffer

                Descriptor_Handle material_data_handle = resource_manager.load_dyanamic_frame_data((void*)&material_data_for_shader, sizeof(Material_Data), 256);
                command_list->bind_handle(material_data_handle, binding_point_string_lookup("material_data"));
                
            }

            command_list->draw_indexed(draw_call.index_count, draw_call.index_offset, draw_call.vertex_offset);

        }
    }
}

/*
*   Release d3d12 obj before exiting application
*/
void D_Renderer::shutdown(){

    // Dear ImGui Shutdown
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    // d_dx12 shutdown

    resource_manager.d_dx12_release();
    shaders.pbr_shader->d_dx12_release();

    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        direct_command_lists[i]->d_dx12_release();
        textures.rt[i]->d_dx12_release();
    }

    textures.ds->d_dx12_release();
    buffers.ssao_sample_kernel->d_dx12_release();

    // Release model resources
    D_Model* model = &models.ptr[0];

    for(u64 i = 0; i < model->meshes.nitems; i++){

        D_Mesh* mesh = model->meshes.ptr + i;
        mesh->index_buffer->d_dx12_release();
        mesh->vertex_buffer->d_dx12_release();

    }

    for(u32 i = 0; i < model->materials.nitems; i++){

        if(model->materials.ptr[i].albedo_texture.texture)
            model->materials.ptr[i].albedo_texture.texture->d_dx12_release();

        if(model->materials.ptr[i].normal_texture.texture)
            model->materials.ptr[i].normal_texture.texture->d_dx12_release();

    }


    // Releases DirectX 12 objects in library 
    d_dx12_shutdown();

#if defined(_DEBUG)
    {
        Microsoft::WRL::ComPtr<IDXGIDebug1> dxgi_debug;
        ThrowIfFailed(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgi_debug)));
        ThrowIfFailed(dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_FLAGS(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL)));
    }
#endif

}

/*
*   FPS / Tick counter
*/
double avg_ms_per_tick(double tick){
    tick_sum -= tick_list[tick_index];
    tick_list[tick_index] = tick;
    tick_sum += tick_list[tick_index];
    tick_index++;
    tick_index = tick_index % MAX_TICK_SAMPLES;
    return ((double)tick_sum/(double)MAX_TICK_SAMPLES);
}


/*
*   Initilize the applization before the window is shown
*/
int D_Renderer::init(){
    PROFILED_FUNCTION();

    ////////////////////////////////
    //   Initialize d_dx12 library
    ////////////////////////////////
    window_rect = { 0, 0, config.display_width, config.display_height };

    // Need to call this before doing much else!
    d_dx12_init(hWnd, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top);


    ///////////////////////////////
    //   Create Resource Manager
    ///////////////////////////////

    resource_manager.init();

    ///////////////////////////////////////////////
    //   Create Render Targets and Depth Stencil
    ///////////////////////////////////////////////

    Texture_Desc rt_desc;
    rt_desc.usage = Texture::USAGE::USAGE_RENDER_TARGET;
    rt_desc.rtv_connect_to_next_swapchain_buffer = true;

    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        textures.rt[i] = resource_manager.create_texture(L"Render Target", rt_desc);
    }

    Texture_Desc main_rt_desc;
    main_rt_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;
    main_rt_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT; // Because we're outputing float lighting values, not RGB to be displayed
    main_rt_desc.width  = config.render_width;
    main_rt_desc.height = config.render_height;
    main_rt_desc.clear_color[0] = 0.3f; 
    main_rt_desc.clear_color[1] = 0.6f; 
    main_rt_desc.clear_color[2] = 1.0f; 
    main_rt_desc.clear_color[3] = 1.0f;
    textures.main_render_target = resource_manager.create_texture(L"Main Render Target", main_rt_desc);

    Texture_Desc main_output_rt_desc;
    main_output_rt_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;
    //main_rt_desc.format = DXGI_FORMAT_R10G10B10A2_UNORM; // HDR
    main_output_rt_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    main_output_rt_desc.width  = config.display_width;
    main_output_rt_desc.height = config.display_height;
    main_output_rt_desc.clear_color[0] = 0.3f; 
    main_output_rt_desc.clear_color[1] = 0.6f; 
    main_output_rt_desc.clear_color[2] = 1.0f; 
    main_output_rt_desc.clear_color[3] = 1.0f;
    textures.main_output_target = resource_manager.create_texture(L"Main Output Target", main_output_rt_desc);

    Texture_Desc ssao_output_desc;
    ssao_output_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;
    ssao_output_desc.format = DXGI_FORMAT_R16_FLOAT;
    ssao_output_desc.width  = config.render_width;
    ssao_output_desc.height = config.render_height;
    textures.ssao_output_texture = resource_manager.create_texture(L"SSAO Render Target", ssao_output_desc);

    Texture_Desc ds_desc;
    ds_desc.usage = Texture::USAGE::USAGE_DEPTH_STENCIL;
    ds_desc.width = config.render_width;
    ds_desc.height = config.render_height;

    textures.ds = resource_manager.create_texture(L"Depth Stencil", ds_desc);

    Texture_Desc shadow_ds_desc;
    shadow_ds_desc.usage = Texture::USAGE::USAGE_DEPTH_STENCIL;
    shadow_ds_desc.width = config.render_width;
    shadow_ds_desc.height = config.render_height;

    textures.shadow_ds = resource_manager.create_texture(L"Shadow Depth Stencil", shadow_ds_desc);

    Texture_Desc g_buffer_albedo_desc;
    g_buffer_albedo_desc.width  = config.render_width;
    g_buffer_albedo_desc.height = config.render_height;
    g_buffer_albedo_desc.format = DXGI_FORMAT_R8G8B8A8_UNORM;
    g_buffer_albedo_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;
    g_buffer_albedo_desc.clear_color[0] = 0.3;
    g_buffer_albedo_desc.clear_color[1] = 0.6;
    g_buffer_albedo_desc.clear_color[2] = 1.0;
    g_buffer_albedo_desc.clear_color[3] = 0.0;

    textures.g_buffer_albedo = resource_manager.create_texture(L"Gbuffer Albedo", g_buffer_albedo_desc);

    Texture_Desc g_buffer_position_desc;
    g_buffer_position_desc.width  = config.render_width;
    g_buffer_position_desc.height = config.render_height;
    g_buffer_position_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    g_buffer_position_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;

    textures.g_buffer_position = resource_manager.create_texture(L"Gbuffer Position", g_buffer_position_desc);

    Texture_Desc g_buffer_normal_desc;
    g_buffer_normal_desc.width  = config.render_width;
    g_buffer_normal_desc.height = config.render_height;
    g_buffer_normal_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    g_buffer_normal_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;

    textures.g_buffer_normal = resource_manager.create_texture(L"Gbuffer Normal", g_buffer_normal_desc);

    Texture_Desc g_buffer_rough_metal_desc;
    g_buffer_rough_metal_desc.width  = config.render_width;
    g_buffer_rough_metal_desc.height = config.render_height;
    g_buffer_rough_metal_desc.format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    g_buffer_rough_metal_desc.usage  = Texture::USAGE::USAGE_RENDER_TARGET;

    textures.g_buffer_rough_metal = resource_manager.create_texture(L"Gbuffer Roughness and Metallic", g_buffer_rough_metal_desc);

    ///////////////////////////////////////////////
    //  Create command allocators and command lists
    ///////////////////////////////////////////////

    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        direct_command_lists[i] = create_command_list(&resource_manager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    ////////////////////////////////////////
    // Set Shaders
    ////////////////////////////////////////

    shaders.pbr_shader               = create_forward_render_pbr_shader();
    shaders.deferred_g_buffer_shader = create_deferred_render_gbuffer_shader();
    shaders.deferred_shading_shader  = create_deferred_render_shading_shader();
    shaders.shadow_map_shader        = create_shadow_mapping_shader();
    shaders.ssao_shader              = create_ssao_shader();
    shaders.post_processing_shader   = create_post_processing_shader();
    shaders.compute_rayt_shader      = create_compute_rayt_shader();
    shaders.dxr_rayt_shader          = create_dxr_rayt_shader();

    ////////////////////////////
    //  Create our command list
    ////////////////////////////

    Command_List* upload_command_list = create_command_list(&resource_manager, D3D12_COMMAND_LIST_TYPE_COPY);
    upload_command_list->reset();

    // Create shader tables in memory for DXR shaders
    build_shader_tables(upload_command_list, shaders.dxr_rayt_shader);

    //////////////////////////
    //  Upload our GLTF Model
    //////////////////////////

    // NOTE: DONT NEGLECT BACKSIDE CULLING (:

    renderer.models.alloc(sizeof(D_Model), 1);
    D_Model& test_model = renderer.models.ptr[0];

    load_gltf_model(test_model, "C:\\dev\\glTF-Sample-Models\\2.0\\Sponza\\glTF\\Sponza.gltf");
    upload_model_to_gpu(upload_command_list, test_model);


    //////////////////////////
    //  SSAO Constant Buffer
    //////////////////////////

    Buffer_Desc ssao_sample_kernel_desc = {};
    ssao_sample_kernel_desc.number_of_elements = 64;
    ssao_sample_kernel_desc.size_of_each_element = sizeof(DirectX::XMFLOAT4);
    ssao_sample_kernel_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;

    buffers.ssao_sample_kernel = resource_manager.create_buffer(L"Test Constant Buffer", ssao_sample_kernel_desc);

    std::uniform_real_distribution<float> random_floats(0., 1.);
    std::default_random_engine generator;

    SSAO_Sample ssao_sample = {};
    DirectX::XMFLOAT4(&ssao_sample_kernel_data)[64] = ssao_sample.ssao_sample;

    for(int i =  0; i < 64; i++){
        // Get random numbers
        ssao_sample_kernel_data[i].x = (random_floats(generator) * 2) - 1;
        ssao_sample_kernel_data[i].y = (random_floats(generator) * 2) - 1;
        ssao_sample_kernel_data[i].z = random_floats(generator);

        // Normalize the random numbers
        float magnitude = sqrtf(ssao_sample_kernel_data[i].x * ssao_sample_kernel_data[i].x + ssao_sample_kernel_data[i].y * ssao_sample_kernel_data[i].y + ssao_sample_kernel_data[i].z * ssao_sample_kernel_data[i].z);
        ssao_sample_kernel_data[i].x = ssao_sample_kernel_data[i].x / magnitude;
        ssao_sample_kernel_data[i].y = ssao_sample_kernel_data[i].y / magnitude;
        ssao_sample_kernel_data[i].z = ssao_sample_kernel_data[i].z / magnitude;

        // Scale the numbers. We want most of them near the center
        float scale = (float)i/64.0;
        scale = 0.1 + scale * (1.0 - 0.1); // lerp
        ssao_sample_kernel_data[i].x *= scale;
        ssao_sample_kernel_data[i].y *= scale;
        ssao_sample_kernel_data[i].z *= scale;

        d_std::os_debug_printf(per_frame_arena, "ssao_sample kernel index: %u, x: %f, y: %f, z: %f\n", i, ssao_sample_kernel_data[i].x, ssao_sample_kernel_data[i].y, ssao_sample_kernel_data[i].z);

    }

    upload_command_list->load_buffer(buffers.ssao_sample_kernel, (u8*)&ssao_sample, sizeof(SSAO_Sample), 32);

    ////////////////////////////
    //  SSAO Rotation Texture
    ////////////////////////////
    Texture_Desc ssao_rotation_texture_desc = { };
    ssao_rotation_texture_desc.format = DXGI_FORMAT_R32G32B32_FLOAT;  // 3 32bit floats (x, y, z)
    ssao_rotation_texture_desc.height = 4;
    ssao_rotation_texture_desc.width  = 4;
    ssao_rotation_texture_desc.name = L"SSAO Rotation Texture";
    ssao_rotation_texture_desc.usage = Texture::USAGE::USAGE_SAMPLED;

    textures.ssao_rotation_texture = resource_manager.create_texture(L"SSAO_Rotation Texture", ssao_rotation_texture_desc);

    Span<DirectX::XMFLOAT3> ssao_rotation_texture_data;
    ssao_rotation_texture_data.alloc(16);
    DirectX::XMFLOAT3* rotation_data_ptr = ssao_rotation_texture_data.ptr;

    for(int i = 0; i < 16; i++){
        rotation_data_ptr[i].x = (random_floats(generator) * 2) - 1;
        rotation_data_ptr[i].y = (random_floats(generator) * 2) - 1;
        rotation_data_ptr[i].z = 0.0;
    }
    
    upload_command_list->load_decoded_texture_from_memory(textures.ssao_rotation_texture, (u_ptr)ssao_rotation_texture_data.ptr, false);

    ////////////////////////////////////////////////
    // Vertex Buffer for full screen quad
    ///////////////////////////////////////////////

    Vertex_Position fsq_verticies[6] = {
        {
            {1.00, -1.00, 0.0},
        },
        {
            
            {-1.00, -1.00, 0.0},
        },
        {
            {-1.00, 1.00, 0.0},
        },
        // TOP
        {
            {-1.00, 1.00, 0.0},
        },
        {
            {1.00, 1.00, 0.0},
        },
        {
            {1.00, -1.00, 0.0},
        }
    };

    // Vertex buffer
    Buffer_Desc vertex_buffer_desc = {};
    vertex_buffer_desc.number_of_elements = 6;
    vertex_buffer_desc.size_of_each_element = sizeof(Vertex_Position);
    vertex_buffer_desc.usage = Buffer::USAGE::USAGE_VERTEX_BUFFER;

    buffers.full_screen_quad_vertex_buffer = resource_manager.create_buffer(L"Full Screen Quad Vertex Buffer", vertex_buffer_desc);

    upload_command_list->load_buffer(buffers.full_screen_quad_vertex_buffer, (u8*)fsq_verticies, 6 * sizeof(Vertex_Position), sizeof(Vertex_Position));

    u16 fsq_index_buffer[] = {
        0, 1, 2,
        3, 4, 5
    };

    // Index Buffer
    Buffer_Desc index_buffer_desc = {};
    index_buffer_desc.number_of_elements = 6;
    index_buffer_desc.size_of_each_element = sizeof(u16);
    index_buffer_desc.usage = Buffer::USAGE::USAGE_INDEX_BUFFER;

    buffers.full_screen_quad_index_buffer = resource_manager.create_buffer(L"Full Screen Quad Index Buffer", index_buffer_desc);

    upload_command_list->load_buffer(buffers.full_screen_quad_index_buffer, (u8*)fsq_index_buffer, 6 * sizeof(u16), sizeof(u16));


    ///////////////////////
    //  Camera
    ///////////////////////

    camera.eye_direction = DirectX::XMVectorSet(0., -.25, -1., 0.);
    camera.eye_position  = DirectX::XMVectorSet(0., 5., 0., 0.);
    camera.up_direction  = DirectX::XMVectorSet(0., 1., 0., 0.);

    ///////////////////////
    //  Light
    ///////////////////////

    per_frame_data.light_position = {0., -2., -1., 0.};
    per_frame_data.light_color    = {20., 20., 20., 0.};

    ///////////////////////
    //  DearIMGUI
    ///////////////////////

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& imgui_io = ImGui::GetIO();

    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(hWnd);

    imgui_font_handle = resource_manager.offline_cbv_srv_uav_descriptor_heap.get_next_handle();

    ImGui_ImplDX12_Init(d3d12_device.Get(), NUM_BACK_BUFFERS, DXGI_FORMAT_R8G8B8A8_UNORM, 
        resource_manager.offline_cbv_srv_uav_descriptor_heap.d3d12_descriptor_heap.Get(),
        imgui_font_handle.cpu_descriptor_handle,
        imgui_font_handle.gpu_descriptor_handle);

    upload_command_list->close();
    execute_command_list(upload_command_list);
    flush_gpu();

    // Compute the Acceleration Structure
    Command_List* command_list = direct_command_lists[current_backbuffer_index];
    command_list->reset();
    calc_acceleration_structure(command_list);
    #ifndef NVIDIA
    // command_list->transition_buffer(shaders.dxr_rayt_shader->hit_group_shader_table, D3D12_RESOURCE_STATE_GENERIC_READ);
    // command_list->transition_buffer(shaders.dxr_rayt_shader->ray_gen_shader_table, D3D12_RESOURCE_STATE_GENERIC_READ);
    // command_list->transition_buffer(shaders.dxr_rayt_shader->miss_shader_table, D3D12_RESOURCE_STATE_GENERIC_READ);
    #endif
    command_list->close();
    execute_command_list(command_list);
    flush_gpu();


    ////////////////
    //  Finish init
    ////////////////

    upload_command_list->d_dx12_release();

    // Sets up FPS Counter
    tick_list = (double*)calloc(MAX_TICK_SAMPLES, sizeof(double));

    application_is_initialized = true;

    DEBUG_LOG("Renderer Initialized!");

    return 0;

}

/*
    TLAS = Top Level Acceleration Structure
    BLAS = Bottom Level Acceleration Structure
    Number of Bottom level acceleration structures = Number of meshes
    Number of Geometry Desc per BLAS = 1?
    Instance Descriptions are used to describe the BLAS to the TLAS
    - Can be useful to instance one BLAS with many transforms

    TODO: fix the main model situation. Define a scene and have the ability to have multiple models
*/

//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//
D_Renderer::AccelerationStructureBuffers D_Renderer::CreateBottomLevelAS(
    std::vector<std::tuple<Microsoft::WRL::ComPtr<ID3D12Resource>, Microsoft::WRL::ComPtr<ID3D12Resource>, uint32_t, uint32_t>> vVertexBuffers, Command_List* command_list) {
  nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;

  // Adding all vertex buffers and not transforming their position.
  for (const auto &buffer : vVertexBuffers) {
    bottomLevelAS.AddVertexBuffer(std::get<0>(buffer).Get(), 0, std::get<2>(buffer), sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord), std::get<1>(buffer).Get(), 0, std::get<3>(buffer), 0, 0);
  }

  // The AS build requires some scratch space to store temporary information.
  // The amount of scratch memory is dependent on the scene complexity.
  UINT64 scratchSizeInBytes = 0;
  // The final AS also needs to be stored in addition to the existing vertex
  // buffers. It size is also dependent on the scene complexity.
  UINT64 resultSizeInBytes = 0;

  bottomLevelAS.ComputeASBufferSizes(d3d12_device.Get(), false, &scratchSizeInBytes,
                                     &resultSizeInBytes);

  // Once the sizes are obtained, the application is responsible for allocating
  // the necessary buffers. Since the entire generation will be done on the GPU,
  // we can directly allocate those on the default heap
  AccelerationStructureBuffers buffers;
  buffers.pScratch = nv_helpers_dx12::CreateBuffer(
      d3d12_device.Get(), scratchSizeInBytes,
      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
      nv_helpers_dx12::kDefaultHeapProps);
  buffers.pResult = nv_helpers_dx12::CreateBuffer(
      d3d12_device.Get(), resultSizeInBytes,
      D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
      nv_helpers_dx12::kDefaultHeapProps);

  // Build the acceleration structure. Note that this call integrates a barrier
  // on the generated AS, so that it can be used to compute a top-level AS right
  // after this method.
  bottomLevelAS.Generate(command_list->d3d12_command_list.Get(), buffers.pScratch.Get(),
                         buffers.pResult.Get(), false, nullptr);

  return buffers;
}

//-----------------------------------------------------------------------------
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
//
void D_Renderer::CreateTopLevelAS(
    const std::vector<std::pair<Microsoft::WRL::ComPtr<ID3D12Resource>, DirectX::XMMATRIX>>
        &instances, Command_List* command_list  // pair of bottom level AS and matrix of the instance
) {
  // Gather all the instances into the builder helper
  for (size_t i = 0; i < instances.size(); i++) {
    m_topLevelASGenerator.AddInstance(instances[i].first.Get(),
                                      instances[i].second, static_cast<UINT>(i),
                                      static_cast<UINT>(0));
  }

  // As for the bottom-level AS, the building the AS requires some scratch space
  // to store temporary data in addition to the actual AS. In the case of the
  // top-level AS, the instance descriptors also need to be stored in GPU
  // memory. This call outputs the memory requirements for each (scratch,
  // results, instance descriptors) so that the application can allocate the
  // corresponding memory
  UINT64 scratchSize, resultSize, instanceDescsSize;

  m_topLevelASGenerator.ComputeASBufferSizes(d3d12_device.Get(), true, &scratchSize,
                                             &resultSize, &instanceDescsSize);

  // Create the scratch and result buffers. Since the build is all done on GPU,
  // those can be allocated on the default heap
  m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
      d3d12_device.Get(), scratchSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
      nv_helpers_dx12::kDefaultHeapProps);
  m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
      d3d12_device.Get(), resultSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
      D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
      nv_helpers_dx12::kDefaultHeapProps);

  // The buffer describing the instances: ID, shader binding information,
  // matrices ... Those will be copied into the buffer by the helper through
  // mapping, so the buffer has to be allocated on the upload heap.
  m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
      d3d12_device.Get(), instanceDescsSize, D3D12_RESOURCE_FLAG_NONE,
      D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);

  // After all the buffers are allocated, or if only an update is required, we
  // can build the acceleration structure. Note that in the case of the update
  // we also pass the existing AS as the 'previous' AS, so that it can be
  // refitted in place.
  m_topLevelASGenerator.Generate(command_list->d3d12_command_list.Get(),
                                 m_topLevelASBuffers.pScratch.Get(),
                                 m_topLevelASBuffers.pResult.Get(),
                                 m_topLevelASBuffers.pInstanceDesc.Get());
}



void D_Renderer::calc_acceleration_structure(Command_List* command_list){

    // Assuming there is one model!!! TODO: Fix this!
    D_Model* main_model = &models.ptr[0];
    //D_Mesh* current_mesh = &main_model->meshes.ptr[0];
    
    u32 num_blas = main_model->meshes.nitems;

    // Prepare the BLAS create structs
    std::vector<UINT64> blas_size(num_blas);
    std::vector<D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC> blas_descs(num_blas);
    Span <D3D12_RAYTRACING_GEOMETRY_DESC> geometry_descs;  // This memory needs to be available when we create the AS
    geometry_descs.alloc(1024);

    ///////////////////////////////
    // Query minimum size for AS
    ///////////////////////////////

    // Fake TLAS description. Used to query how much memory we need
    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO tlas_prebuild_info;
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlas_inputs = {};

    tlas_inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    tlas_inputs.NumDescs = num_blas;
    tlas_inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    tlas_inputs.pGeometryDescs = nullptr;
    tlas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    // Query how much memory we need
    d3d12_device->GetRaytracingAccelerationStructurePrebuildInfo(&tlas_inputs, &tlas_prebuild_info);
    UINT64 scratch_buffer_size_needed = tlas_prebuild_info.ScratchDataSizeInBytes;

    ////////////////////////////////////////////////////////////
    // Create the BLASes
    // - One per mesh. 
    // - # of geometry desc = # of primitive_groups
    ////////////////////////////////////////////////////////////

    for(int i = 0; i < num_blas; i++ ){

        D_Mesh* current_mesh = &main_model->meshes.ptr[i];
        u16 num_geometry_desc = current_mesh->primitive_groups.nitems;
        geometry_info.alloc(num_geometry_desc);

        for(int j = 0; j < num_geometry_desc; j++){

            D_Draw_Call* current_draw_call = &current_mesh->draw_calls.ptr[j];

            // Create geometry descriptions for each primitive group

            D3D12_RAYTRACING_GEOMETRY_DESC* desc = &geometry_descs.ptr[j];
            desc->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
            desc->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            // Define where the vertex + index buffers are + their offsets
            D3D12_RAYTRACING_GEOMETRY_TRIANGLES_DESC& triangles_desc = desc->Triangles;
            triangles_desc.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
            triangles_desc.VertexCount = current_draw_call->vertex_count;
            triangles_desc.VertexBuffer.StrideInBytes = current_mesh->vertex_buffer->vertex_buffer_view.StrideInBytes; // sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord);
            triangles_desc.VertexBuffer.StartAddress = current_mesh->vertex_buffer->d3d12_resource->GetGPUVirtualAddress() + (current_draw_call->vertex_offset * current_mesh->vertex_buffer->vertex_buffer_view.StrideInBytes);
            if(triangles_desc.VertexBuffer.StartAddress % 4 != 0){
                DEBUG_BREAK;
            }

            // TODO: Compute prior. Goes for rasterization passes too..
            DirectX::XMMATRIX model_matrix = DirectX::XMMatrixIdentity();
            DirectX::XMMATRIX scale_matrix = DirectX::XMMatrixScaling(0.1, 0.1, 0.1);
            model_matrix = DirectX::XMMatrixMultiply(scale_matrix, DirectX::XMMatrixIdentity());
            DirectX::XMMATRIX translation_matrix = DirectX::XMMatrixTranslation(main_model->coords.x, main_model->coords.y, main_model->coords.z);
            // geometry_info.ptr[j].model_matrix = DirectX::XMMatrixMultiply(translation_matrix, model_matrix);
            geometry_info.ptr[j].vertex_offset = current_draw_call->vertex_offset;
            geometry_info.ptr[j].index_byte_offset = current_draw_call->index_offset * sizeof(u16);
            geometry_info.ptr[j].material_id    = current_draw_call->material_index;
            geometry_info.ptr[j].material_flags = main_model->materials.ptr[current_draw_call->material_index].material_flags;

            triangles_desc.IndexBuffer = current_mesh->index_buffer->d3d12_resource->GetGPUVirtualAddress() + current_draw_call->index_offset * sizeof(u16);
            if(triangles_desc.IndexBuffer % sizeof(u16) != 0){
                DEBUG_BREAK;
            }
            triangles_desc.IndexCount = current_draw_call->index_count;
            triangles_desc.IndexFormat = DXGI_FORMAT_R16_UINT;
            triangles_desc.Transform3x4 = 0;

        }

        Buffer_Desc geometry_info_gpu_desc;
        geometry_info_gpu_desc.number_of_elements = geometry_info.nitems;
        geometry_info_gpu_desc.size_of_each_element = sizeof(geometry_info.ptr[0]);
        geometry_info_gpu_desc.usage = Buffer::USAGE_CONSTANT_BUFFER;
        geometry_info_gpu = resource_manager.create_buffer(L"Geometry Vertex Offsets", geometry_info_gpu_desc);
        command_list->load_buffer(geometry_info_gpu, (u8*)geometry_info.ptr, geometry_info.nitems * sizeof(geometry_info.ptr[0]), sizeof(geometry_info.ptr[0]));

        // Fill BLAS create struct
        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* blas_desc = &blas_descs[i];
        blas_desc->Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
        blas_desc->Inputs.NumDescs = num_geometry_desc;
        blas_desc->Inputs.pGeometryDescs = geometry_descs.ptr;
        blas_desc->Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE; // Perfered for static geo: https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_raytracing_acceleration_structure_build_flags
        blas_desc->Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO blas_prebuild_info;
        d3d12_device->GetRaytracingAccelerationStructurePrebuildInfo(&blas_desc->Inputs, &blas_prebuild_info);

        blas_size[i] = blas_prebuild_info.ResultDataMaxSizeInBytes;
        // Here we'll make sure to increase the scratch buffer size, if we need it.
        scratch_buffer_size_needed = d_max(blas_prebuild_info.ScratchDataSizeInBytes, scratch_buffer_size_needed);

        if(current_mesh->vertex_buffer->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
            command_list->transition_buffer(current_mesh->vertex_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

        if(current_mesh->index_buffer->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE){
            command_list->transition_buffer(current_mesh->index_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        }

    }

    // How should we create a scratch buffer???
    // Is a UAV buffer in state: D3D12_RESOURCE_STATE_COMMON
    // scratchBuffer.Create(L"Acceleration Structure Scratch Buffer", static_cast<UINT>(scratch_buffer_size_needed), 1);
    Buffer_Desc scratch_buffer_desc;
    scratch_buffer_desc.number_of_elements = 1.;
    scratch_buffer_desc.size_of_each_element = scratch_buffer_size_needed;
    scratch_buffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
    scratch_buffer_desc.state = D3D12_RESOURCE_STATE_COMMON;
    scratch_buffer_desc.create_cbv = false;
    /*
    - The scratch_buffer_size_needed looks way to big!!!
    - Check if it's supposed to be that big. if not, fix it
    - May need to rethink primitive groups / meshes?
    */
    Buffer* scratch_buffer = resource_manager.create_buffer(L"Scratch buffer for building acceleration structure", scratch_buffer_desc);

    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instance_descs (num_blas);
    blases.resize(num_blas);

    for (UINT i = 0; i < blas_descs.size(); i++)
    {

        // Create the BLAS
        Buffer_Desc blas_resource_desc;    
        blas_resource_desc.number_of_elements   = 1.;
        blas_resource_desc.size_of_each_element = blas_size[i];
        blas_resource_desc.usage                = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
        blas_resource_desc.state                = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
        blas_resource_desc.flags                = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
        blas_resource_desc.create_cbv           = false;

        // TODO: In the future, all these BLAS resources should be in the same "page", segmented out of the same placed resource or something
        // Reference: https://developer.nvidia.com/blog/managing-memory-for-acceleration-structures-in-dxr/
        blases[i] = resource_manager.create_buffer(L"BLAS Resource", blas_resource_desc);

        blas_descs[i].DestAccelerationStructureData    = blases[i]->d3d12_resource->GetGPUVirtualAddress();
        blas_descs[i].ScratchAccelerationStructureData = scratch_buffer->d3d12_resource->GetGPUVirtualAddress();
        blas_descs[i].SourceAccelerationStructureData  = NULL;

        D3D12_RAYTRACING_INSTANCE_DESC& instanceDesc = instance_descs[i];

        // Identity matrix
        ZeroMemory(instanceDesc.Transform, sizeof(instanceDesc.Transform));
        instanceDesc.Transform[0][0] = 0.1f;
        instanceDesc.Transform[1][1] = 0.1f;
        instanceDesc.Transform[2][2] = 0.1f;
        // instanceDesc.Transform[0][0] = 1.0f;
        // instanceDesc.Transform[1][1] = 1.0f;
        // instanceDesc.Transform[2][2] = 1.0f;

        instanceDesc.AccelerationStructure = blases[i]->d3d12_resource->GetGPUVirtualAddress();
        instanceDesc.Flags = 0;
        instanceDesc.InstanceID = 0;
        instanceDesc.InstanceMask = 1;
        instanceDesc.InstanceContributionToHitGroupIndex = i;
    }

    // Can't use this because we need the GPUVirtualAddress... Just use the dynamic_buffer directly??
    //
    // CURRENTLY CAUSING GPU FAULT!!!!!!!!!!!!!!!!!!!!!
    // 
    // Descriptor_Handle instances_descs_handle = resource_manager.load_dyanamic_frame_data(instance_descs.data(), instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), 256);

    Buffer_Desc instance_buffer_desc;
    instance_buffer_desc.number_of_elements = instance_descs.size();
    instance_buffer_desc.size_of_each_element = sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    instance_buffer_desc.usage = Buffer::USAGE_CONSTANT_BUFFER;
    instance_buffer = resource_manager.create_buffer(L"Instance Buffer", instance_buffer_desc);
    u32 instance_descs_allocation_size = sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instance_descs.size();
    // Dynamic_Buffer::Allocation dynamic_instance_desc_allocation = dynamic_buffer.allocate(instance_descs_allocation_size, 256);
    // memcpy(dynamic_instance_desc_allocation.cpu_addr, instance_descs.data(), instance_descs_allocation_size);

    command_list->load_buffer(instance_buffer, (u8*)instance_descs.data(), instance_descs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), sizeof(D3D12_RAYTRACING_INSTANCE_DESC));

    //////////////////////////
    // Create TLAS
    //////////////////////////

    // Allocate Dest Memory
    Buffer_Desc tlas_buffer_desc;
    tlas_buffer_desc.number_of_elements   = 1.;
    tlas_buffer_desc.size_of_each_element = tlas_prebuild_info.ResultDataMaxSizeInBytes;
    tlas_buffer_desc.usage                = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
    tlas_buffer_desc.state                = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
    tlas_buffer_desc.flags                = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    tlas_buffer_desc.create_cbv           = false;

    tlas = resource_manager.create_buffer(L"TLAS Resource", tlas_buffer_desc);

    // Specify where the instance data buffer is located.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC tlas_desc;
    tlas_inputs.InstanceDescs = instance_buffer->d3d12_resource->GetGPUVirtualAddress();
    tlas_inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    tlas_desc.Inputs = tlas_inputs;
    tlas_desc.DestAccelerationStructureData = tlas->d3d12_resource->GetGPUVirtualAddress();
    tlas_desc.ScratchAccelerationStructureData = scratch_buffer->d3d12_resource->GetGPUVirtualAddress();
    tlas_desc.SourceAccelerationStructureData = NULL;

    // With all the necessary buffers set up and structures filled in,
    // we can finally tell the GPU to build our acceleration structure

    // Create the BLASes
    for (UINT i = 0; i < blas_descs.size(); i++)
    {
        D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC info;
        info;
        command_list->d3d12_command_list->BuildRaytracingAccelerationStructure(&blas_descs[i], 0, nullptr);
    }

    // NEED BARRIER BETWEEN TOP AND BOTTOM!!

    command_list->d3d12_command_list->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(blases[0]->d3d12_resource.Get()));
    command_list->transition_buffer(instance_buffer, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

    // Create the TLAS
    command_list->d3d12_command_list->BuildRaytracingAccelerationStructure(&tlas_desc, 0, nullptr);

    #if 0  // NVIDIA
    // Create the vertex buffer.
    {
        // Define the geometry for a triangle.
        Vertex triangleVertices[] = {
            {{0.0f, 2500.f * m_aspectRatio, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
            {{2500.f, -2500.f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
            {{-2500.f, -2500.f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}};
            // {{0.0f, 0.25f * m_aspectRatio, 0.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
            // {{0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
            // {{-0.25f, -0.25f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 1.0f, 1.0f}}};

        const UINT vertexBufferSize = sizeof(triangleVertices);

        // Note: using upload heaps to transfer static data like vert buffers is not
        // recommended. Every time the GPU needs it, the upload heap will be
        // marshalled over. Please read up on Default Heap usage. An upload heap is
        // used here for code simplicity and because there are very few verts to
        // actually transfer.
        ThrowIfFailed(d3d12_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD), D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
            IID_PPV_ARGS(&m_vertexBuffer)));

        // Copy the triangle data to the vertex buffer.
        UINT8 *pVertexDataBegin;
        CD3DX12_RANGE readRange(
            0, 0); // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_vertexBuffer->Map(
            0, &readRange, reinterpret_cast<void **>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, triangleVertices, sizeof(triangleVertices));
        m_vertexBuffer->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_vertexBufferView.BufferLocation = m_vertexBuffer->GetGPUVirtualAddress();
        m_vertexBufferView.StrideInBytes = sizeof(Vertex);
        m_vertexBufferView.SizeInBytes = vertexBufferSize;
    }


    //   Build the bottom AS from the Triangle vertex buffer
    // AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS({{m_vertexBuffer.Get(), 
    //                                                                         nullptr, // main_model->meshes.ptr[0].index_buffer->d3d12_resource.Get(),
    //                                                                         3, /*main_model->meshes.ptr[0].vertex_buffer->number_of_elements,*/
    //                                                                         0 /*main_model->meshes.ptr[0].index_buffer->number_of_elements*/ }}
    //                                                                         , command_list);

    AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS({{main_model->meshes.ptr[0].vertex_buffer->d3d12_resource.Get(), 
                                                                            main_model->meshes.ptr[0].index_buffer->d3d12_resource.Get(),
                                                                            main_model->meshes.ptr[0].vertex_buffer->number_of_elements,
                                                                            main_model->meshes.ptr[0].index_buffer->number_of_elements }}
                                                                            , command_list);

    // Just one instance for now
    m_instances = {{bottomLevelBuffers.pResult, XMMatrixIdentity()}};
    CreateTopLevelAS(m_instances, command_list);

    // Flush the command list and wait for it to finish
    command_list->close();
    execute_command_list(command_list);
    flush_gpu();
    // command_list->reset();

    // Once the command list is finished executing, reset it to be reused for
    // rendering

    // Store the AS buffers. The rest of the buffers will be released once we exit
    // the function
    m_bottomLevelAS = bottomLevelBuffers.pResult;
    #endif

}

void D_Renderer::build_shader_tables(Command_List* command_list, Shader* shader){

    Microsoft::WRL::ComPtr<ID3D12StateObjectProperties> state_object_properties;
    ThrowIfFailed(shader->d3d12_rt_state_object.As(&state_object_properties));

    // NVIDIA START
    
    #ifdef NVIDIA
    // The ray generation only uses heap data
    m_sbtHelper.AddRayGenerationProgram(L"MyRaygenShader", {});

    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbtHelper.AddMissProgram(L"MyHitGroup", {});

    // Adding the triangle hit shader
    m_sbtHelper.AddHitGroup(L"MyMissShader", {});

    // Compute the size of the SBT given the number of shaders and their
    // parameters
    uint32_t sbtSize = m_sbtHelper.ComputeSBTSize();

    // Create the SBT on the upload heap. This is required as the helper will use
    // mapping to write the SBT contents. After the SBT compilation it could be
    // copied to the default heap for performance.
    m_sbtStorage = nv_helpers_dx12::CreateBuffer(
        d3d12_device.Get(), sbtSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ, nv_helpers_dx12::kUploadHeapProps);
    if (!m_sbtStorage) {
        throw std::logic_error("Could not allocate the shader binding table");
    }
    // Compile the SBT from the shader and parameters info
    m_sbtHelper.Generate(m_sbtStorage.Get(), state_object_properties.Get());

    // NVIDIA END
    #else
    void* ray_gen_shader_itentifier     = state_object_properties->GetShaderIdentifier(L"MyRaygenShader");
    void* hit_group_shader_itentifier   = state_object_properties->GetShaderIdentifier(L"MyHitGroup");
    void* miss_shader_itentifier        = state_object_properties->GetShaderIdentifier(L"MyMissShader");
    u32 shader_ident_size               = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    u32 shader_record_size;

    void* null_ptr = calloc(1, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

    // Ray gen Shader Table
    {
        shader_record_size = shader_ident_size;  // This will change if we add root constant buffers, then the size will increase to accound for them

        Shader_Record ray_gen_shader_record;
        memcpy(&ray_gen_shader_record.shader_id, ray_gen_shader_itentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        // Old
        Buffer_Desc buffer_desc;
        buffer_desc.create_cbv = false;
        buffer_desc.number_of_elements = 1.;
        buffer_desc.size_of_each_element = shader_record_size;
        buffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
        buffer_desc.alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        shader->ray_gen_shader_table = resource_manager.create_buffer(L"Ray Gen Shader Table", buffer_desc);

        sbt_upload_storage = command_list->load_buffer(shader->ray_gen_shader_table, (u8*)ray_gen_shader_itentifier, shader_record_size, 256);

    }

    // Miss Shader Table
    {
        Shader_Record miss_shader_record;
        memcpy(&miss_shader_record.shader_id, miss_shader_itentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        // memcpy(&miss_shader_record.shader_id, null_ptr, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        shader_record_size = shader_ident_size;  // This will change if we add root constant buffers, then the size will increase to accound for them
        Buffer_Desc buffer_desc;
        buffer_desc.create_cbv = false;
        buffer_desc.number_of_elements = 1.;
        buffer_desc.size_of_each_element = shader_record_size;
        buffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
        buffer_desc.alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        shader->miss_shader_table = resource_manager.create_buffer(L"Miss Shader Table", buffer_desc);

        command_list->load_buffer(shader->miss_shader_table, (u8*)&miss_shader_record, shader_record_size, 256);
    }

    // Hit Group Shader Table
    {
        Shader_Record hit_shader_record;
        memcpy(&hit_shader_record.shader_id, hit_group_shader_itentifier, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

        shader_record_size = shader_ident_size;  // This will change if we add root constant buffers, then the size will increase to accound for them
        Buffer_Desc buffer_desc;
        buffer_desc.create_cbv = false;
        buffer_desc.number_of_elements = 1.;
        buffer_desc.size_of_each_element = shader_record_size;
        buffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;
        buffer_desc.alignment = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;
        shader->hit_group_shader_table = resource_manager.create_buffer(L"Hit Group Shader Table", buffer_desc);

        command_list->load_buffer(shader->hit_group_shader_table, (u8*)&hit_shader_record, shader_record_size, 256);
    }
    #endif
}

void D_Renderer::render_shadow_map(Command_List* command_list){

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shadow Map
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    DirectX::XMVECTOR light_position = DirectX::XMVectorSet((-per_frame_data.light_position.x * 50), (-per_frame_data.light_position.y * 50), (-per_frame_data.light_position.z * 50), 1.0);
    DirectX::XMVECTOR light_direction = DirectX::XMVectorSet((per_frame_data.light_position.x), (per_frame_data.light_position.y), (per_frame_data.light_position.z), 0.0);
    light_direction = DirectX::XMVector4Normalize(light_direction);
    DirectX::XMMATRIX light_view_matrix = DirectX::XMMatrixLookToRH(light_position, light_direction, camera.up_direction);
    DirectX::XMMATRIX light_projection_matrix = DirectX::XMMatrixOrthographicOffCenterRH(-250, 250, -250, 250, -5000, 5000);
    DirectX::XMMATRIX light_view_projection_matrix = DirectX::XMMatrixMultiply(light_view_matrix, light_projection_matrix);
    
    // Dynamic Data
    per_frame_data.light_space_matrix = light_view_projection_matrix;

    // Fill the command list:
    command_list->set_shader(shaders.shadow_map_shader);

    if(textures.shadow_ds->state != D3D12_RESOURCE_STATE_DEPTH_WRITE){
        command_list->transition_texture(textures.shadow_ds, D3D12_RESOURCE_STATE_DEPTH_WRITE);
    }
    // Set up render targets. Here, we are just using a depth buffer
    command_list->set_viewport(display.viewport);
    command_list->set_scissor_rect(display.scissor_rect);
    command_list->set_render_targets(0, NULL, textures.shadow_ds);

    command_list->clear_depth_stencil(textures.shadow_ds, 1.0f);

    //command_list->bind_constant_arguments(&light_view_projection_matrix, sizeof(DirectX::XMMATRIX) / 4, binding_point_string_lookup("light_matrix"));
    Descriptor_Handle light_matrix_handle = resource_manager.load_dyanamic_frame_data((void*)&light_view_projection_matrix, sizeof(DirectX::XMMATRIX), 256);
    command_list->bind_handle(light_matrix_handle, binding_point_string_lookup("light_matrix"));

    bind_and_draw_model(command_list, &renderer.models.ptr[0]);
    
    // Transition shadow ds to Pixel Resource State
    command_list->transition_texture(textures.shadow_ds, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

}


void D_Renderer::forward_render_pass(Command_List* command_list){

    //////////////////////////////////////
    // Forward Shading!
    //////////////////////////////////////
    command_list->set_render_targets(1, &textures.main_output_target, textures.ds);

    command_list->set_shader(shaders.pbr_shader);
    command_list->set_viewport(display.viewport);
    command_list->set_scissor_rect(display.scissor_rect);

    // Bind Shadow Map
    Texture_Index shadow_texture_index = {};
    shadow_texture_index.texture_index = command_list->bind_texture(textures.shadow_ds, &resource_manager, binding_point_string_lookup("Shadow_Map"));

    // Upload Shadow_Map_Index - Constant Buffer requires 256 byte alignment
    Descriptor_Handle shadow_map_index_handle = resource_manager.load_dyanamic_frame_data((void*)&shadow_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(shadow_map_index_handle, binding_point_string_lookup("shadow_texture_index"));

    // Upload per_frame_data - Constant Buffer requires 256 byte alignment
    Descriptor_Handle per_frame_data_handle = resource_manager.load_dyanamic_frame_data((void*)&this->per_frame_data, sizeof(Per_Frame_Data), 256);
    command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));

    bind_and_draw_model(command_list, &renderer.models.ptr[0]);
}

void D_Renderer::deferred_render_pass(Command_List* command_list){

    //////////////////////////////////////
    // Defered Shading!
    //////////////////////////////////////

    ///////////////////////
    // Geometry Pass
    ///////////////////////

    // Transition Albedo Buffer
    if(textures.g_buffer_albedo->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.g_buffer_albedo, D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Transition Position Buffer
    if(textures.g_buffer_position->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.g_buffer_position, D3D12_RESOURCE_STATE_RENDER_TARGET);

    if(textures.g_buffer_normal->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.g_buffer_normal, D3D12_RESOURCE_STATE_RENDER_TARGET);

    if(textures.g_buffer_rough_metal->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.g_buffer_rough_metal, D3D12_RESOURCE_STATE_RENDER_TARGET);

    command_list->clear_render_target(textures.g_buffer_albedo);
    command_list->clear_render_target(textures.g_buffer_position);
    command_list->clear_render_target(textures.g_buffer_normal);

    Texture* render_targets[] = {textures.g_buffer_albedo, textures.g_buffer_position, textures.g_buffer_normal, textures.g_buffer_rough_metal};

    command_list->set_render_targets(4, render_targets, textures.ds);
    command_list->set_shader        (shaders.deferred_g_buffer_shader);
    command_list->set_viewport      (display.viewport);
    command_list->set_scissor_rect  (display.scissor_rect);

    Descriptor_Handle per_frame_data_handle = resource_manager.load_dyanamic_frame_data((void*)&this->per_frame_data, sizeof(Per_Frame_Data), 256);
    command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));

    // command_list->bind_constant_arguments(&view_projection_matrix, sizeof(DirectX::XMMATRIX) / 4, binding_point_string_lookup("view_projection_matrix"));
    // command_list->bind_constant_arguments(&camera.eye_position,    sizeof(DirectX::XMVECTOR),     binding_point_string_lookup("camera_position_buffer"));

    bind_and_draw_model(command_list, &renderer.models.ptr[0]);

    ///////////////////////
    // Shading Pass
    ///////////////////////

    command_list->set_render_targets(1, &textures.main_render_target, NULL);
    command_list->set_viewport      (display.viewport);
    command_list->set_scissor_rect  (display.scissor_rect);
    command_list->set_shader        (shaders.deferred_shading_shader);

    // Bind Buffers
    if(textures.g_buffer_albedo->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        command_list->transition_texture(textures.g_buffer_albedo, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if(textures.g_buffer_position->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        command_list->transition_texture(textures.g_buffer_position, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if(textures.g_buffer_normal->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        command_list->transition_texture(textures.g_buffer_normal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if(textures.g_buffer_rough_metal->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        command_list->transition_texture(textures.g_buffer_rough_metal, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    if(textures.ds->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)
        command_list->transition_texture(textures.ds, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);

    // Bind GBuffer Textures (and indices)
    Gbuffer_Indices gbuffer_indices          = {};
    gbuffer_indices.albedo_index             = command_list->bind_texture (textures.g_buffer_albedo,       &resource_manager, binding_point_string_lookup("Albedo Gbuffer"));
    gbuffer_indices.position_index           = command_list->bind_texture (textures.g_buffer_position,     &resource_manager, binding_point_string_lookup("Position Gbuffer"));
    gbuffer_indices.normal_index             = command_list->bind_texture (textures.g_buffer_normal,       &resource_manager, binding_point_string_lookup("Normal Gbuffer"));
    gbuffer_indices.roughness_metallic_index = command_list->bind_texture (textures.g_buffer_rough_metal,  &resource_manager, binding_point_string_lookup("Roughness and Metallic Gbuffer"));

    Descriptor_Handle gbuffer_indices_handle = resource_manager.load_dyanamic_frame_data((void*)&gbuffer_indices, sizeof(Gbuffer_Indices), 256);
    command_list->bind_handle(gbuffer_indices_handle, binding_point_string_lookup("gbuffer_indices"));
    
    // Bind SSAO Texture (and index)
    Texture_Index ssao_rotation_texture_index          = {};
    ssao_rotation_texture_index.texture_index = command_list->bind_texture (textures.ssao_rotation_texture, &resource_manager, 0);

    Descriptor_Handle ssao_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&ssao_rotation_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(ssao_texture_index_handle, binding_point_string_lookup("ssao_texture_index"));

    // Bind output texture dimensions
    Output_Dimensions output_dimensions = {config.render_width, config.render_height};
    Descriptor_Handle output_dimensions_handle = resource_manager.load_dyanamic_frame_data((void*)&output_dimensions, sizeof(Output_Dimensions), 256);
    command_list->bind_handle(output_dimensions_handle, binding_point_string_lookup("output_dimensions"));

    // Bind SSAO Kernel
    command_list->bind_buffer(buffers.ssao_sample_kernel, &resource_manager, binding_point_string_lookup("ssao_sample"));

    // Bind Shadow Map and upload Shadow_Map_Index - Constant Buffer requires 256 byte alignment
    Texture_Index shadow_texture_index = {};
    shadow_texture_index.texture_index = command_list->bind_texture(textures.shadow_ds, &resource_manager, binding_point_string_lookup("Shadow_Map"));

    Descriptor_Handle shadow_map_index_handle = resource_manager.load_dyanamic_frame_data((void*)&shadow_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(shadow_map_index_handle, binding_point_string_lookup("shadow_texture_index"));

    // Bind per_frame_data - Constant Buffer requires 256 byte alignment
    command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));

    // Now be bind the texture table to the root signature. 
    command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));

    command_list->bind_vertex_buffer(buffers.full_screen_quad_vertex_buffer, 0);
    command_list->bind_index_buffer (buffers.full_screen_quad_index_buffer);
    command_list->draw(6);

    //////////////////////////////////////////////////////////
    // SSAO
    //////////////////////////////////////////////////////////
    {

        command_list->set_shader(shaders.ssao_shader);

        if(textures.g_buffer_albedo->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            command_list->transition_texture(textures.g_buffer_albedo, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        if(textures.g_buffer_position->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            command_list->transition_texture(textures.g_buffer_position, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        if(textures.g_buffer_normal->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            command_list->transition_texture(textures.g_buffer_normal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

        if(textures.g_buffer_rough_metal->state != D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)
            command_list->transition_texture(textures.g_buffer_rough_metal, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        
        // Bind GBuffer Textures (and indices)
        Gbuffer_Indices gbuffer_indices          = {};
        gbuffer_indices.albedo_index             = command_list->bind_texture (textures.g_buffer_albedo,       &resource_manager, binding_point_string_lookup("Albedo Gbuffer"));
        gbuffer_indices.position_index           = command_list->bind_texture (textures.g_buffer_position,     &resource_manager, binding_point_string_lookup("Position Gbuffer"));
        gbuffer_indices.normal_index             = command_list->bind_texture (textures.g_buffer_normal,       &resource_manager, binding_point_string_lookup("Normal Gbuffer"));
        gbuffer_indices.roughness_metallic_index = command_list->bind_texture (textures.g_buffer_rough_metal,  &resource_manager, binding_point_string_lookup("Roughness and Metallic Gbuffer"));

        Descriptor_Handle gbuffer_indices_handle = resource_manager.load_dyanamic_frame_data((void*)&gbuffer_indices, sizeof(Gbuffer_Indices), 256);
        command_list->bind_handle(gbuffer_indices_handle, binding_point_string_lookup("gbuffer_indices"));

        Texture_Index ssao_output_texture_index = {};
        ssao_output_texture_index.texture_index = command_list->bind_texture(textures.ssao_output_texture, &resource_manager, binding_point_string_lookup("outputTexture"), true);
        Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&ssao_output_texture_index, sizeof(Texture_Index), 256);
        command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));

        // Bind SSAO Texture (and index)
        Texture_Index ssao_rotation_texture_index = {};
        ssao_rotation_texture_index.texture_index = command_list->bind_texture (textures.ssao_rotation_texture, &resource_manager, 0);

        Descriptor_Handle ssao_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&ssao_rotation_texture_index, sizeof(Texture_Index), 256);
        command_list->bind_handle(ssao_texture_index_handle, binding_point_string_lookup("ssao_texture_index"));

        // Bind SSAO Kernel
        command_list->bind_buffer(buffers.ssao_sample_kernel, &resource_manager, binding_point_string_lookup("ssao_sample"));

        // Bind output texture dimensions
        Output_Dimensions output_dimensions = {textures.ssao_output_texture->width, textures.ssao_output_texture->height};
        Descriptor_Handle output_dimensions_handle = resource_manager.load_dyanamic_frame_data((void*)&output_dimensions, sizeof(Output_Dimensions), 256);
        command_list->bind_handle(output_dimensions_handle, binding_point_string_lookup("output_dimensions"));

        // Bind per_frame_data - Constant Buffer requires 256 byte alignment
        command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));

        // Now be bind the texture table to the root signature. 
        command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
        command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_uav_table"));
        command_list->dispatch((int)(textures.ssao_output_texture->width / 8), (int)(textures.ssao_output_texture->height / 4), 1);

        // command_list->transition_texture(textures.main_render_target,           D3D12_RESOURCE_STATE_RENDER_TARGET);
    }

    //////////////////////////////////////////////////////////
    // Post Processing
    //////////////////////////////////////////////////////////
    {
        command_list->set_shader(shaders.post_processing_shader);

        command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));
        
        Texture_Index input_texture_index = {};
        input_texture_index.texture_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("input_texture"), true);

        Descriptor_Handle input_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&input_texture_index, sizeof(Texture_Index), 256);
        command_list->bind_handle(input_texture_index_handle, binding_point_string_lookup("input_texture_index"));

        Texture_Index output_texture_index = {};
        output_texture_index.texture_index = command_list->bind_texture(textures.main_output_target, &resource_manager, binding_point_string_lookup("outputTexture"), true);

        Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&output_texture_index, sizeof(Texture_Index), 256);
        command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));

        Texture_Index ssao_texture_index = {};
        ssao_texture_index.texture_index = command_list->bind_texture(textures.ssao_output_texture, &resource_manager, binding_point_string_lookup("ssao_texture"), true);

        Descriptor_Handle ssao_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&ssao_texture_index, sizeof(Texture_Index), 256);
        command_list->bind_handle(ssao_texture_index_handle, binding_point_string_lookup("ssao_texture_index"));

        // Now be bind the texture table to the root signature. 
        command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
        command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_uav_table"));
        command_list->dispatch((int)(display.display_width / 8), (int)(display.display_height / 4), 1);
    }

}

void D_Renderer::compute_rayt_pass(Command_List* command_list){

    //////////////////////////////////////
    // Compute Ray Tracing!
    //////////////////////////////////////

    //////////////////////////////////////////////////////////
    // SSAO
    //////////////////////////////////////////////////////////

    command_list->set_shader(shaders.compute_rayt_shader);

    Descriptor_Handle per_frame_data_handle = resource_manager.load_dyanamic_frame_data((void*)&this->per_frame_data, sizeof(Per_Frame_Data), 256);
    command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));
    
    // Texture_Index input_texture_index = {};
    // input_texture_index.texture_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("input_texture"), true);

    // Descriptor_Handle input_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&input_texture_index, sizeof(Texture_Index), 256);
    // command_list->bind_handle(input_texture_index_handle, binding_point_string_lookup("input_texture_index"));

    Output_Dimensions output_dimensions = {config.display_width, config.display_height};
    Descriptor_Handle output_dimensions_handle = resource_manager.load_dyanamic_frame_data((void*)&output_dimensions, sizeof(Output_Dimensions), 256);
    command_list->bind_handle(output_dimensions_handle, binding_point_string_lookup("output_dimensions"));

    Texture_Index output_texture_index = {};
    output_texture_index.texture_index = command_list->bind_texture(textures.main_output_target, &resource_manager, binding_point_string_lookup("outputTexture"), true);

    Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&output_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));

    // Now be bind the texture table to the root signature. 
    // command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
    command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_uav_table"));
    command_list->dispatch((int)(display.display_width / 8), (int)(display.display_height / 4), 1);

}

void D_Renderer::dxr_ray_tracing_pass(Command_List* command_list){

    //////////////////////////////////////
    // DXR Ray Tracing!
    //////////////////////////////////////

    command_list->set_shader(shaders.dxr_rayt_shader);

    // build_shader_tables(shaders.dxr_rayt_shader);  // Should be done before rendering. only reason it's here is because it's using dyanmic memory to store tables

    Descriptor_Handle per_frame_data_handle = resource_manager.load_dyanamic_frame_data((void*)&this->per_frame_data, sizeof(Per_Frame_Data), 256);
    command_list->bind_handle(per_frame_data_handle, binding_point_string_lookup("per_frame_data"));
    
    // Texture_Index input_texture_index = {};
    // input_texture_index.texture_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("input_texture"), true);

    // Descriptor_Handle input_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&input_texture_index, sizeof(Texture_Index), 256);
    // command_list->bind_handle(input_texture_index_handle, binding_point_string_lookup("input_texture_index"));

    Output_Dimensions output_dimensions = {config.display_width, config.display_height};
    Descriptor_Handle output_dimensions_handle = resource_manager.load_dyanamic_frame_data((void*)&output_dimensions, sizeof(Output_Dimensions), 256);
    command_list->bind_handle(output_dimensions_handle, binding_point_string_lookup("output_dimensions"));

    // Cant use Constant buffer for geometry vertex offsets because we don't know how many we have.
    // Constant buffers need to be "Constant". Cant have variable size or anyting. I think the shader/compiler needs to know the size
    // at compile time.
    // command_list->bind_buffer(geometry_info_gpu, &resource_manager, binding_point_string_lookup("geometry_info"));
    Descriptor_Handle geo_vertex_offset_handle = resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {0};
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements  = geometry_info.nitems;
        srv_desc.Buffer.StructureByteStride = sizeof(geometry_info.ptr[0]);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        d3d12_device->CreateShaderResourceView(geometry_info_gpu->d3d12_resource.Get(), &srv_desc, geo_vertex_offset_handle.cpu_descriptor_handle);
    }
    command_list->bind_handle(geo_vertex_offset_handle, binding_point_string_lookup("geometry_info"));

    // Aquire a handle from the online descriptor heap
    // command_list->bind_buffer(models.ptr[0].meshes.ptr[0].vertex_buffer, &resource_manager, binding_point_string_lookup("vertex_buffer"));
    Descriptor_Handle vertex_buffer_handle = resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {0};
        srv_desc.Format = DXGI_FORMAT_UNKNOWN;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements  = models.ptr[0].meshes.ptr[0].vertex_buffer->number_of_elements;
        srv_desc.Buffer.StructureByteStride = sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
        d3d12_device->CreateShaderResourceView(models.ptr[0].meshes.ptr[0].vertex_buffer->d3d12_resource.Get(), &srv_desc, vertex_buffer_handle.cpu_descriptor_handle);
    }
    command_list->bind_handle(vertex_buffer_handle, binding_point_string_lookup("vertex_buffer"));

    Descriptor_Handle index_buffer_handle = resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

    {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {0};
        srv_desc.Format = DXGI_FORMAT_R32_TYPELESS;
        srv_desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv_desc.Buffer.FirstElement = 0;
        srv_desc.Buffer.NumElements  = models.ptr[0].meshes.ptr[0].index_buffer->number_of_elements / 2; // / 2 because index is 16 bit, and buffer format is 32 bit
        srv_desc.Buffer.StructureByteStride = 0; //sizeof(u16);
        srv_desc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_RAW;
        d3d12_device->CreateShaderResourceView(models.ptr[0].meshes.ptr[0].index_buffer->d3d12_resource.Get(), &srv_desc, index_buffer_handle.cpu_descriptor_handle);
    }
    command_list->bind_handle(index_buffer_handle, binding_point_string_lookup("index_buffer"));

    command_list->d3d12_command_list->SetComputeRootShaderResourceView(command_list->current_bound_shader->binding_points[binding_point_string_lookup("scene")].root_signature_index, tlas->d3d12_resource->GetGPUVirtualAddress());

    // Bind All Materials
    D_Model* main_model = &models.ptr[0];
    u32 current_texture_table_index = 0;  // TODO: EEEEKKKKK shouldn't be here. Need a better way of doing this. Maybe we should bind textures like this for rasterization too?
    for(int i = 0; i < main_model->materials.nitems; i++){
        D_Material* material = &main_model->materials.ptr[i];

        command_list->bind_texture(material->albedo_texture.texture, &resource_manager, 0);

        if(material->material_flags & MATERIAL_FLAG_NORMAL_TEXTURE){
            command_list->bind_texture(material->normal_texture.texture, &resource_manager, 0);
        } else {
            resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_texture_handle();  // Throw away to keep our Material Slots uniform
        }

        if(material->material_flags & MATERIAL_FLAG_ROUGHNESSMETALLIC_TEXTURE){
            command_list->bind_texture(material->roughness_metallic_texture.texture, &resource_manager, 0);
        } else {
            resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_texture_handle();  // Throw away to keep our Material Slots uniform
        }
    }

    // Need to do this last so it doesn't interfere with Geometry_Info.material_id mapping to above material index.. TODO: find a better solution..
    Texture_Index output_texture_index = {};
    output_texture_index.texture_index = command_list->bind_texture(textures.main_output_target, &resource_manager, binding_point_string_lookup("outputTexture"), true);

    Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&output_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));

    // Now be bind the texture table to the root signature. 
    command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
    command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_uav_table"));

    #ifndef NVIDIA
    D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
    dispatchDesc.HitGroupTable.StartAddress             = shaders.dxr_rayt_shader->hit_group_shader_table->d3d12_resource->GetGPUVirtualAddress();
    dispatchDesc.HitGroupTable.SizeInBytes              = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatchDesc.HitGroupTable.StrideInBytes            = dispatchDesc.HitGroupTable.SizeInBytes;
    dispatchDesc.MissShaderTable.StartAddress           = shaders.dxr_rayt_shader->miss_shader_table->d3d12_resource->GetGPUVirtualAddress();
    dispatchDesc.MissShaderTable.SizeInBytes            = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatchDesc.MissShaderTable.StrideInBytes          = dispatchDesc.MissShaderTable.SizeInBytes;
    dispatchDesc.RayGenerationShaderRecord.StartAddress = shaders.dxr_rayt_shader->ray_gen_shader_table->d3d12_resource->GetGPUVirtualAddress();
    dispatchDesc.RayGenerationShaderRecord.SizeInBytes  = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    dispatchDesc.Width  = config.render_width;
    dispatchDesc.Height = config.display_height;
    dispatchDesc.Depth  = 1;
    command_list->d3d12_command_list->DispatchRays(&dispatchDesc);
    #else

        D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed
    // stride.

    // The ray generation shaders are always at the beginning of the SBT.
    uint32_t rayGenerationSectionSizeInBytes =
        m_sbtHelper.GetRayGenSectionSize();
    desc.RayGenerationShaderRecord.StartAddress =
        m_sbtStorage->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes =
        rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray
    // generation shader. We have one miss shader for the camera rays and one
    // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    // also indicate the stride between the two miss shaders, which is the size
    // of a SBT entry
    uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
    desc.MissShaderTable.StartAddress =
        m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
    desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

    // The hit groups section start after the miss shaders. In this sample we
    // have one 1 hit group for the triangle
    uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
    desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() +
                                      rayGenerationSectionSizeInBytes +
                                      missSectionSizeInBytes;
    desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

    // Dimensions of the image to render, identical to a kernel launch dimension
    desc.Width = config.render_width;
    desc.Height = config.render_height;
    desc.Depth = 1;

    command_list->d3d12_command_list->DispatchRays(&desc);
    #endif

}

void D_Renderer::render(){
    {

    PROFILED_SCOPE("CPU_FRAME");

    // TODO: Rewrite all of this !!!
    static std::chrono::high_resolution_clock::time_point tp1;
    std::chrono::high_resolution_clock::time_point tp2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> frame_time = std::chrono::duration_cast<std::chrono::duration<double>>(tp2 - tp1);
    tp1 = tp2;
    double frame_ms = frame_time.count() * 1000.;
    double avg_frame_ms = avg_ms_per_tick(frame_ms);
    double fps = 1000. / avg_frame_ms;

    Command_List* command_list = direct_command_lists[current_backbuffer_index];
    
    // Resets command list, command allocator, and online cbv_srv_uav descriptor heap in resource manager
    command_list->reset();

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /// Dear IMGUI
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Bind IMGUI Fonts
    Descriptor_Handle online_imgui_font_handle = command_list->bind_descriptor_handles_to_online_descriptor_heap(imgui_font_handle, 1);
    ImGuiIO& imgui_io = ImGui::GetIO();
    imgui_io.Fonts->SetTexID((ImTextureID) online_imgui_font_handle.gpu_descriptor_handle.ptr); 

    // Start IMGUI Frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if(config.imgui_demo)
        ImGui::ShowDemoWindow();


    ////////////////////////////
    /// Update Camera Matricies
    ////////////////////////////

    DirectX::XMMATRIX view_matrix = DirectX::XMMatrixLookToRH(camera.eye_position, camera.eye_direction, camera.up_direction);
    DirectX::XMMATRIX projection_matrix = DirectX::XMMatrixPerspectiveFovRH(DirectX::XMConvertToRadians(camera.fov), (f32) config.render_width / (f32) config.render_height, 0.01f, 2500000000.0f);
    per_frame_data.view_projection_matrix = DirectX::XMMatrixMultiply(view_matrix, projection_matrix);
    DirectX::XMStoreFloat4(&per_frame_data.camera_pos, camera.eye_position);
    DirectX::XMVECTOR view_matrix_deter;
    DirectX::XMMATRIX inv_view_matrix = DirectX::XMMatrixInverse(&view_matrix_deter, view_matrix);
    per_frame_data.view_matrix = view_matrix;
    
    // ////////////////////////////////////
    // /// Update Render To Display Scale
    // ////////////////////////////////////

    per_frame_data.render_to_display_scale = config.display_width / config.render_width;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Physically based shading
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // vickylovesyou!!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    // Transition the RT and DS
    if(textures.main_render_target->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.main_render_target, D3D12_RESOURCE_STATE_RENDER_TARGET);

    if(textures.ds->state != D3D12_RESOURCE_STATE_DEPTH_WRITE)
        command_list->transition_texture(textures.ds, D3D12_RESOURCE_STATE_DEPTH_WRITE);

    if(textures.main_output_target->state != D3D12_RESOURCE_STATE_RENDER_TARGET)
        command_list->transition_texture(textures.main_output_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
        
    // // Clear the render target and depth stencil
    FLOAT rt_clear_color[] = {0.3f, 0.6, 1.0f, 1.0f};
    command_list->clear_render_target(textures.main_render_target);
    command_list->clear_render_target(textures.main_output_target);
    command_list->clear_depth_stencil(textures.ds, 1.0f);

    // Choose render passed based on config. Set by IMGUI below
    switch(config.render_pass){
        case D_Render_Passes::FORWARD_SHADING:
            // Shadow Map
            render_shadow_map(command_list);
            // Lighting pass
            forward_render_pass(command_list);
            break;
        case D_Render_Passes::DEFERRED_SHADING:
            // Shadow Map
            render_shadow_map(command_list);
            // Lighting pass
            deferred_render_pass(command_list);
            command_list->transition_texture(textures.main_output_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->set_render_targets(1, &textures.main_output_target, nullptr);
            break;
        case D_Render_Passes::RAY_TRACING_COMPUTE:
            compute_rayt_pass(command_list);
            command_list->transition_texture(textures.main_output_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->set_render_targets(1, &textures.main_output_target, nullptr);
            break;
        case D_Render_Passes::DXR_RAY_TRACING:
            dxr_ray_tracing_pass(command_list);
            command_list->transition_texture(textures.main_output_target, D3D12_RESOURCE_STATE_RENDER_TARGET);
            command_list->set_render_targets(1, &textures.main_output_target, nullptr);
            break;
    }

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Compute Shader!!
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // command_list->set_shader(shaders.post_processing_shader);
    
    // // Bind GBuffer Textures (and indices)
    // Texture_Index output_texture_index = {};
    // output_texture_index.shadow_texture_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("outputTexture"), true);

    // Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&output_texture_index, sizeof(Texture_Index), 256);
    // command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));
    // // Now be bind the texture table to the root signature. 
    // command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
    // command_list->dispatch((int)(display.display_width / 8), (int)(display.display_height / 4), 1);

    // command_list->transition_texture(textures.main_render_target,           D3D12_RESOURCE_STATE_RENDER_TARGET);

    // Create IMGUI window
    ImGui::Begin("Info");
    ImGui::Text("Controls:");
    ImGui::Text("Mouse - Look");
    ImGui::Text("W, A, S, D - Move");
    ImGui::Text("V - VSync on / off");
    ImGui::Text("Space Bar - Full Screen Toggle");
    ImGui::Text("FPS: %.3lf", fps);
    ImGui::Text("Frame MS: %.2lf", avg_frame_ms);
    ImGui::SliderFloat3("Light Position", &this->per_frame_data.light_position.x, -10., 10);
    ImGui::DragFloat3("Light Color", &this->per_frame_data.light_color.x);
    ImGui::SliderFloat("Camera FOV", &this->camera.fov, 35., 120.);
    // Combo box for choosing which render pass to use
    // Copied from imgui_demo.cpp
    if (ImGui::BeginCombo("Render Pass", render_pass_names[config.render_pass], /*flags*/ 0))
    {
        for (int n = 0; n < NUM_RENDER_PASSES; n++)
        {
            const bool is_selected = (config.render_pass == n);
            if (ImGui::Selectable(render_pass_names[n], is_selected))
                config.render_pass = n;

            // Set the initial focus when opening the combo (scrolling + keyboard navigation focus)
            if (is_selected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
    ImGui::Checkbox("Demo Window?", &config.imgui_demo);

    // Render a texture
    // u16 ssao_output_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("ssao_output"));
    // //u32 ssao_rotation_index = command_list->bind_texture (textures.ssao_rotation_texture, &resource_manager, 0);
    // float render_ratio = 1920. / 1080.;
    // ImGui::Image((ImTextureID)resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_handle_by_index(ssao_output_index).gpu_descriptor_handle.ptr, ImVec2(200.*render_ratio, 200.));

    ImGui::End();
    ImGui::Render();

    // Render ImGui on top of everything
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list->d3d12_command_list.Get());


    // // Prepare screen buffer for copy destination
    command_list->copy_texture(textures.main_output_target, textures.rt[current_backbuffer_index]);

    // // Transition RT to presentation state
    command_list->transition_texture(textures.rt[current_backbuffer_index], D3D12_RESOURCE_STATE_PRESENT);

    // // Execute Command List
    command_list->close();
    execute_command_list(command_list);

    } // CPU_FRAME profile scope
    present(using_v_sync);
    
}

/*
*   Window event callback
*/
LRESULT CALLBACK WindowProcess(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {

    LRESULT result = 0;

    if(application_is_initialized) {

        ImGuiIO& io = ImGui::GetIO();

        switch (msg) {

            // Key Press
            // Handle ALT+ENTER
            case(WM_SYSKEYDOWN):
            {
                if ((wParam == VK_RETURN) && (lParam & (1 << 29))) {
                    if (application_is_initialized && CheckTearingSupport()) {
                        Span<Texture*> rts_to_resize = { renderer.textures.rt, 2 };
                        toggle_fullscreen(rts_to_resize);
                        renderer.textures.ds->resize(renderer.textures.rt[0]->width, renderer.textures.rt[0]->height);
                    }
                } else {
                    result = DefWindowProc(hWnd, msg, wParam, lParam);
                }
            }
            break;

            // Mouse Button
            //case(WM_SYSKEYDOWN):
            case(WM_LBUTTONDOWN):
            {
                if (!capturing_mouse && !io.WantCaptureMouse) {
                    SetCapture(hWnd);
                    ShowCursor(false);

                    // These window rects should be saved
                    RECT window_rect;
                    GetWindowRect(hWnd, &window_rect);
                    SetCursorPos((window_rect.left + window_rect.right) / 2, (window_rect.top + window_rect.bottom) / 2);
                    capturing_mouse = true;
                } else {
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, true);
                }
            }
            break;

            case(WM_LBUTTONUP):
            {
                if (!io.WantCaptureMouse){

                } else {
                    io.AddMouseButtonEvent(ImGuiMouseButton_Left, false);
                }
            }
            break;
			case WM_MOUSEMOVE:
			{

				int mouse_x = GET_X_LPARAM(lParam);
			    int mouse_y = GET_Y_LPARAM(lParam);

                if (capturing_mouse && !io.WantCaptureMouse) {

                    // These window rects should be saved
                    RECT window_rect;
                    GetWindowRect(hWnd, &window_rect);

                    POINT transformed_mouse_cord = { mouse_x, mouse_y };
                    ClientToScreen(hWnd, &transformed_mouse_cord);

                    int delta_x = transformed_mouse_cord.x - ((window_rect.left + window_rect.right) / 2);
                    int delta_y = transformed_mouse_cord.y - ((window_rect.top + window_rect.bottom) / 2);

                    // Move coursor to center of the screen
                    SetCursorPos((window_rect.left + window_rect.right) / 2, (window_rect.top + window_rect.bottom) / 2);

                    // Rotate the camera
                    XMVECTOR camera_x_axis        = XMVector3Normalize(XMVector3Cross(renderer.camera.eye_direction, renderer.camera.up_direction));
                    XMMATRIX rotation_matrix      = XMMatrixMultiply(DirectX::XMMatrixRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), (float)-delta_x / 100.), DirectX::XMMatrixRotationAxis(camera_x_axis, (float)(-delta_y) / 100.));
                    renderer.camera.eye_direction = XMVector3Normalize(XMVector3Transform(renderer.camera.eye_direction, rotation_matrix));
                }

                io.AddMousePosEvent(mouse_x, mouse_y);

			}
            break;
			case WM_KEYDOWN:
			{
				bool alt = (::GetAsyncKeyState(VK_MENU) & 0x8000) != 0;
				switch (wParam) {
                    case 'W':
                    {
                        // Translate the camera forward
                        float speed = renderer.camera.speed;
                        XMVECTOR translation_vector = XMVectorMultiply(XMVector3Normalize(renderer.camera.eye_direction), XMVectorSet(speed, speed, speed, 0));
                        renderer.camera.eye_position = XMVectorAdd(renderer.camera.eye_position, translation_vector);
                        io.AddInputCharacter(wParam);
                    }
                    break;
                    case 'A':
                    {
                        // Translate the camera to the left
                        float speed = -renderer.camera.speed;
                        XMVECTOR translation_vector = XMVectorMultiply(XMVector3Normalize(XMVector3Cross(renderer.camera.eye_direction, renderer.camera.up_direction)), XMVectorSet(speed, speed, speed, 0));
                        renderer.camera.eye_position = XMVectorAdd(renderer.camera.eye_position, translation_vector);
                        io.AddInputCharacter(wParam);
                    }
                    break;
                    case 'S':
                    {
                        // Translate the camera backwards
                        float speed = -renderer.camera.speed;
                        XMVECTOR translation_vector = XMVectorMultiply(XMVector3Normalize(renderer.camera.eye_direction), XMVectorSet(speed, speed, speed, 0));
                        renderer.camera.eye_position = XMVectorAdd(renderer.camera.eye_position, translation_vector);
                        io.AddInputCharacter(wParam);
                    }
                    break;
                    case 'D':
                    {
                        // Translate the camera to the left
                        float speed = renderer.camera.speed; 
                        XMVECTOR translation_vector = XMVectorMultiply(XMVector3Normalize(XMVector3Cross(renderer.camera.eye_direction, renderer.camera.up_direction)), XMVectorSet(speed, speed, speed, 0));
                        renderer.camera.eye_position = XMVectorAdd(renderer.camera.eye_position, translation_vector);
                        io.AddInputCharacter(wParam);
                    }
                    break;

                    case VK_ESCAPE:
                    {
                        if (capturing_mouse) {
                            ReleaseCapture();
                            ShowCursor(true);
                            capturing_mouse = false;
                        }
                    }
                    break;

                    case 'V':
                        using_v_sync = !using_v_sync;
                    break;

                    // Full Screen when space bar
                    case(VK_SPACE):
                    {
                        if (application_is_initialized && CheckTearingSupport()) {
                            Span<Texture*> rts_to_resize = { renderer.textures.rt, 2 };
                            toggle_fullscreen(rts_to_resize);
                            renderer.config.display_width = renderer.textures.rt[0]->width;
                            renderer.config.display_height = renderer.textures.rt[0]->height;

                            // Resize size dependent resources
                            renderer.textures.ds->resize(renderer.textures.rt[0]->width, renderer.textures.rt[0]->height);
                            renderer.textures.main_output_target->resize(renderer.textures.rt[0]->width, renderer.textures.rt[0]->height);
                        }
                    }
                    break;

                }
            }
			break;
            // Mouse movement
            
            case(WM_MOVE): 
            {
                OutputDebugString("Moved!\n");
                RECT test = {};
                GetWindowRect(renderer.hWnd, &test);
            }
            break;
            case(WM_EXITSIZEMOVE): 
            {
                OutputDebugString("Exit size move loop!\n");
                //RedrawWindow(hWnd, NULL, NULL, RDW_INTERNALPAINT);
                UpdateWindow(hWnd);
                result = DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            default:
                // if we don't have anything to do, pass it off
                result = DefWindowProc(hWnd, msg, wParam, lParam);
        }
    } else {
        result = DefWindowProc(hWnd, msg, wParam, lParam);
    }

    return result;

}

/*
*   Main entry point
*/
int CALLBACK
WinMain(HINSTANCE hInstance,
        HINSTANCE hPrevInstance,
        LPSTR     lpCmdLine,
        int       nCmdShow)
{
    WNDCLASSEX wndclass = {};
    
    /*
    *   docs.microsoft.com : Initializes the COM library for use by the calling thread, sets the thread's concurrency model, 
    *   and creates a new apartment for the thread if one is required.
    */
    CoInitializeEx(NULL, COINIT_MULTITHREADED);

    /*
    *   docs.microsoft.com : Set the DPI awareness for the current thread to the provided value. 
    */
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    timeBeginPeriod(1);

    // Set up memory arenas
    per_frame_arena = d_std::make_arena(); 

    // Renderer Scope
    {

        /*
        *   Class for creating window
        */
        wndclass.cbSize = sizeof(WNDCLASSEX);
        wndclass.style = CS_HREDRAW | CS_VREDRAW;					// Can Redraw width and height
        wndclass.lpfnWndProc = WindowProcess;						// Where to send messages for this window
        wndclass.cbClsExtra = 0;
        wndclass.cbWndExtra = 0;
        wndclass.hInstance = hInstance;
        wndclass.hIcon = LoadIcon(hInstance, IDI_APPLICATION);		// window icon
        wndclass.hCursor = LoadCursor(nullptr, IDC_ARROW);			// window cursor
        wndclass.hbrBackground = (HBRUSH)COLOR_WINDOWFRAME;			// color of window back
        wndclass.lpszMenuName = NULL;								// Dont want a menu
        wndclass.lpszClassName = "David Window Class";				// Name of the window class
        wndclass.hIconSm = LoadIcon(hInstance, IDI_APPLICATION);	// little icon (taskbar?)

        // Register window class
        if (RegisterClassEx(&wndclass) == 0) {
            OutputDebugString("Couldn't Register Window");
            return -1;
        }

        // Create the window using our class..
        renderer.hWnd = CreateWindowEx(
                WS_EX_TOPMOST,
                "David Window Class",		// Defined previously
                "DDX123",                   // Name at the top of the window	
                //WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,		// A few attributes for the window like minimize, border, ect..
                WS_OVERLAPPEDWINDOW,		     // A few attributes for the window like minimize, border, ect..
                CW_USEDEFAULT,				     // DefaultPos X
                CW_USEDEFAULT,				     // DefaultPos Y
                renderer.config.display_width,	 // Display(!!) Width
                renderer.config.display_height,  // Display(!!) Height
                nullptr,					     // Parent Window
                nullptr,					     // Menu (Dont want one)
                hInstance,                       // HINSTANCE
                nullptr						     // Additional Data
            );

        // Checks that it worked
        if (renderer.hWnd == NULL) {
            DEBUG_ERROR("Couldn't Create Window");
            DEBUG_BREAK;
            return -1;
        }

        // Initialize app
        renderer.init();

        DEBUG_LOG("Application Initialized!");

        // Show the window
        ShowWindow(renderer.hWnd, nCmdShow);

        SetWindowPos(renderer.hWnd, HWND_NOTOPMOST, 0, 0, renderer.config.display_width, renderer.config.display_height, 0);
        // Message loop
        MSG msg = {};
        while (true) {
            if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
            //if(GetMessage(&msg, hWnd, 0, GM_)){
                if (msg.message == WM_QUIT) {
                    break;
                }
                /*
                else if (msg.message == WM_PAINT) {
                    //renderer.render();
                }
                */
                else {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            } else {
                renderer.render();
                per_frame_arena->reset();
                d_dx12_arena->reset();
            }
        }

        // Kill the window
        ShowWindow(renderer.hWnd, SW_HIDE);
        // Shutdown the Renderer
        renderer.shutdown();

    }

    /*
    *   docs.microsoft.com : Closes the COM library on the current thread, unloads all DLLs loaded by the thread,
    *   frees any other resources that the thread maintains, and forces all RPC connections on the thread to close.
    */

    CoUninitialize();

    return 0;
}
