#include "pch.h"
#include "main.h"

#include "d_dx12.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <chrono>
#include <random>
#include "timeapi.h"

#include "d_core.cpp"
#include "d_dx12.cpp"
#include "model.cpp"
#include "shaders.cpp"
#include "constant_buffers.h"

using namespace d_dx12;
using namespace d_std;
using namespace DirectX;

#define BUFFER_OFFSET(i) ((char *)0 + (i))

// Sets window, rendertargets to 4k resolution
 #define d_4k

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
    RAY_TRACING,
    NUM_RENDER_PASSES
};

static const char* render_pass_names[NUM_RENDER_PASSES] = {
    "Forward_Shading",
    "Deffered_Shading",
    "Ray_Tracing",
};

struct D_Renderer_Config {
    bool fullscreen_mode = false;
    bool imgui_demo      = false;         
    u8   render_pass     = RAY_TRACING;

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


    int  init();
    void render();
    void render_shadow_map(Command_List* command_list);
    void forward_render_pass(Command_List* command_list);
    void deferred_render_pass(Command_List* command_list);
    void compute_rayt_pass(Command_List* command_list);
    void shutdown();
    void toggle_fullscreen();
    void upload_model_to_gpu(Command_List* command_list, D_Model& test_model);
    void bind_and_draw_model(Command_List* command_list, D_Model* model);

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

    // For each primitive group
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

        mesh->vertex_buffer = resource_manager.create_buffer(L"Vertex Buffer", vertex_buffer_desc);

        command_list->load_buffer(mesh->vertex_buffer, (u8*)verticies_buffer.ptr, verticies_buffer.nitems * sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord), sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord));//Fix: sizeof(Vertex_Position_Color));

        // Index Buffer
        Buffer_Desc index_buffer_desc = {};
        index_buffer_desc.number_of_elements = indicies_buffer.nitems;
        index_buffer_desc.size_of_each_element = sizeof(u16);
        index_buffer_desc.usage = Buffer::USAGE::USAGE_INDEX_BUFFER;

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

    ////////////////////////////
    //  Create our command list
    ////////////////////////////

    Command_List* upload_command_list = create_command_list(&resource_manager, D3D12_COMMAND_LIST_TYPE_COPY);
    upload_command_list->reset();


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

    Texture_Index output_texture_index = {};
    output_texture_index.texture_index = command_list->bind_texture(textures.main_output_target, &resource_manager, binding_point_string_lookup("outputTexture"), true);

    Descriptor_Handle output_texture_index_handle = resource_manager.load_dyanamic_frame_data((void*)&output_texture_index, sizeof(Texture_Index), 256);
    command_list->bind_handle(output_texture_index_handle, binding_point_string_lookup("output_texture_index"));

    // Now be bind the texture table to the root signature. 
    // command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_table"));
    command_list->bind_online_descriptor_heap_texture_table(&resource_manager, binding_point_string_lookup("texture_2d_uav_table"));
    command_list->dispatch((int)(display.display_width / 8), (int)(display.display_height / 4), 1);

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
    
    ////////////////////////////////////
    /// Update Render To Display Scale
    ////////////////////////////////////

    per_frame_data.render_to_display_scale = config.display_width / config.render_width;

    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Shadow Map
    /////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    render_shadow_map(command_list);

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
        
    // Clear the render target and depth stencil
    FLOAT rt_clear_color[] = {0.3f, 0.6, 1.0f, 1.0f};
    command_list->clear_render_target(textures.main_render_target);
    command_list->clear_render_target(textures.main_output_target);
    command_list->clear_depth_stencil(textures.ds, 1.0f);

    // Choose render passed based on config. Set by IMGUI below
    switch(config.render_pass){
        case D_Render_Passes::FORWARD_SHADING:
            forward_render_pass(command_list);
            break;
        case D_Render_Passes::DEFERRED_SHADING:
            deferred_render_pass(command_list);
            command_list->set_render_targets(1, &textures.main_output_target, nullptr);
            break;
        case D_Render_Passes::RAY_TRACING:
            compute_rayt_pass(command_list);
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
    u16 ssao_output_index = command_list->bind_texture(textures.main_render_target, &resource_manager, binding_point_string_lookup("ssao_output"));
    //u32 ssao_rotation_index = command_list->bind_texture (textures.ssao_rotation_texture, &resource_manager, 0);
    float render_ratio = 1920. / 1080.;
    ImGui::Image((ImTextureID)resource_manager.online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_handle_by_index(ssao_output_index).gpu_descriptor_handle.ptr, ImVec2(200.*render_ratio, 200.));

    ImGui::End();
    ImGui::Render();

    // Render ImGui on top of everything
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list->d3d12_command_list.Get());


    // Prepare screen buffer for copy destination
    command_list->copy_texture(textures.main_output_target, textures.rt[current_backbuffer_index]);

    // Transition RT to presentation state
    command_list->transition_texture(textures.rt[current_backbuffer_index], D3D12_RESOURCE_STATE_PRESENT);

    // Execute Command List
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
