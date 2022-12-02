#include "pch.h"
#include "d_dx12.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

#include <chrono>

using namespace d_dx12;
using namespace d_std;
namespace tg = tinygltf;

#define BUFFER_OFFSET(i) ((char *)0 + (i))

struct Vertex_Position_Color_Texcoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
};

struct D_Model {
    Span<Vertex_Position_Color_Texcoord> verticies;
    Span<u16> indicies;
    Span<u8>  texture;
    Texture_Desc texture_desc;
};

// Global Vars, In order of creation
struct D_Renderer {

    HWND hWnd;
    Resource_Manager resource_manager;
    Shader* shader;
    Command_List*  direct_command_lists[NUM_BACK_BUFFERS];
    Texture*       rt[NUM_BACK_BUFFERS];
    Texture*       ds;
    Texture*       sampled_texture;
    Buffer*        vertex_buffer;
    Buffer*        index_buffer;
    Buffer*        constant_buffer;
    d_dx12::Descriptor_Handle imgui_font_handle;
    bool fullscreen_mode = false;
    RECT window_rect;
    tg::TinyGLTF loader;
    Span<D_Model> models;

    int  init();
    void render();
    void shutdown();
    void toggle_fullscreen();
    void load_gltf_model(D_Model& d_model, const char* filename);

};

D_Renderer renderer;
bool application_is_initialized = false;

#define MAX_TICK_SAMPLES 20
int tick_index = 0;
double tick_sum = 0.;
double *tick_list = NULL;

/*******************/

u16 display_width  = 1920;
u16 display_height = 1080;
bool using_v_sync = false;



/*
*   Load a gltf model
*/

void load_mesh(D_Model& d_model, tg::Model& tg_model, tg::Mesh& mesh){

    u8* buffer_arrays[10] = {NULL};

    // Loop threw buffer views
    for(u64 i = 0; i < tg_model.bufferViews.size(); i++){

        // The buffer view were looking at
        const tg::BufferView &buffer_view = tg_model.bufferViews[i];

        char* out_buffer = (char*)calloc(500, sizeof(char));
        sprintf(out_buffer, "Buffer View: %d, byte length: %d, byte offset: %d\n", i, buffer_view.byteLength, buffer_view.byteOffset);
        OutputDebugString(out_buffer);
        free(out_buffer);

        // The buffer our buffer view is referencing
        const tg::Buffer &buffer = tg_model.buffers[buffer_view.buffer];

        // Copy over our data from the buffer
        buffer_arrays[i] = (u8*)malloc(sizeof(u8) * buffer_view.byteLength);
        memccpy(buffer_arrays[i], &buffer.data.at(0) + buffer_view.byteOffset, 0, buffer_view.byteLength);

        #if 0 
        for(u64 j = 0; j < buffer_view.byteLength; j++){

            char* out_buffer = (char*)calloc(500, sizeof(char));
            sprintf(out_buffer, "j: %d : data: %u\n", j, buffer_arrays[i][j]);
            OutputDebugString(out_buffer);
            free(out_buffer);

        }
        #endif

    }

    for(u64 i = 0; i < mesh.primitives.size(); i++){
        tg::Primitive primitive = mesh.primitives[i];

        /////////////////////
        // Indicies
        /////////////////////
        {
            // Get Indicies accessor
            tg::Accessor index_accessor = tg_model.accessors[primitive.indices];
            // Get the buffer fiew our accessor references
            const tg::BufferView &buffer_view = tg_model.bufferViews[index_accessor.bufferView];
            // The buffer our buffer view is referencing
            const tg::Buffer &buffer = tg_model.buffers[buffer_view.buffer];
            // Alloc mem for indicies
            int index_byte_stride = index_accessor.ByteStride(buffer_view);
            d_model.indicies.alloc(index_accessor.count);

            // copy over indicies
            for(u64 i = 0; i < index_accessor.count; i++){
                // WARNING: u16* would need to change with different sizes / byte_strides of indicies
                d_model.indicies.ptr[i] = ((u16*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
            }
        }

        /////////////////////////
        // Primitive attributes
        /////////////////////////
        {
            // Find position attribute and allocate memory
            for (auto &attribute : primitive.attributes){
                if(attribute.first.compare("POSITION") == 0){
                    tg::Accessor accessor = tg_model.accessors[attribute.second];
                    d_model.verticies.alloc(accessor.count, sizeof(Vertex_Position_Color_Texcoord));
                }
            }

            // For each attribute our mesh has
            for (auto &attribute : primitive.attributes){
                // Get the accessor for our attribute
                tg::Accessor accessor = tg_model.accessors[attribute.second];
                // Get the buffer fiew our accessor references
                const tg::BufferView &buffer_view = tg_model.bufferViews[accessor.bufferView];
                // The buffer our buffer view is referencing
                const tg::Buffer &buffer = tg_model.buffers[buffer_view.buffer];

                // Copy over our data from the buffer
                // buffer_arrays[i] = (u8*)malloc(sizeof(u8) * buffer_view.byteLength);

                int byte_stride = accessor.ByteStride(buffer_view);
                int size = 1;
                if(accessor.type != TINYGLTF_TYPE_SCALAR){
                    size = accessor.type;
                }
                
                if(attribute.first.compare("POSITION") == 0){
                    for(int i = 0; i < d_model.verticies.nitems; i++){
                        d_model.verticies.ptr[i].position = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                        d_model.verticies.ptr[i].position.z = -d_model.verticies.ptr[i].position.z;
                    }
                } else if(attribute.first.compare("NORMAL") == 0){
                    // Normals not yet supported
                } else if(attribute.first.compare("TEXCOORD_0") == 0){
                    for(int i = 0; i < d_model.verticies.nitems; i++){
                        d_model.verticies.ptr[i].texture_coordinates = ((DirectX::XMFLOAT2*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                        // Dx12 UV is different than OpenGL / GLTF
                        d_model.verticies.ptr[i].texture_coordinates.y = 1 - d_model.verticies.ptr[i].texture_coordinates.y;
                    }
                } else if(attribute.first.compare("COLOR_0") == 0){
                    for(int i = 0; i < d_model.verticies.nitems; i++){
                        d_model.verticies.ptr[i].color = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                    }
                }

                char* out_buffer = (char*)calloc(500, sizeof(char));
                sprintf(out_buffer, "attribute.first: %s size: %u, count: %u, accessor.componentType: %u, accessor.normalized: %u, byteStride: %u, byteOffset: %u\n", attribute.first.c_str(), size, buffer_view.byteLength / byte_stride, accessor.componentType, accessor.normalized, byte_stride, BUFFER_OFFSET(accessor.byteOffset));
                OutputDebugString(out_buffer);
                free(out_buffer);

                //d_model.verticies.alloc(, sizeof(Vertex_Position_Color_Texcoord));
            }
        }
    }

    if(tg_model.textures.size() > 0){
        tg::Texture &tex = tg_model.textures[0];

        if(tex.source >= 0){
            tg::Image &image = tg_model.images[tex.source];

            d_model.texture_desc.width = image.width;
            d_model.texture_desc.height = image.height;

            DXGI_FORMAT image_format = DXGI_FORMAT_UNKNOWN;
            
            if(image.component == 1){
                if(image.bits == 8){
                    image_format = DXGI_FORMAT_R8_UINT;
                } else if(image.bits == 16){
                    image_format = DXGI_FORMAT_R16_UINT;
                }
            } else if (image.component == 2){
                if(image.bits == 8){
                    image_format = DXGI_FORMAT_R8G8_UINT;
                } else if(image.bits == 16){
                    image_format = DXGI_FORMAT_R16G16_UINT;
                }
            } else if (image.component == 3){
                // ?
            } else if (image.component == 4){
                if(image.bits == 8){
                    image_format = DXGI_FORMAT_R8G8B8A8_UNORM;
                } else if(image.bits == 16){
                    image_format = DXGI_FORMAT_R16G16B16A16_UINT;
                }
            }

            d_model.texture_desc.format     = image_format;
            d_model.texture_desc.usage      = Texture::USAGE::USAGE_SAMPLED;
            d_model.texture_desc.pixel_size = image.component * image.bits / 8;

            d_model.texture.alloc(image.image.size());
            memcpy(d_model.texture.ptr, &image.image.at(0), image.image.size());// * image.component * (image.bits / 8));
            
        }

    }
}

void load_model_nodes(D_Model& d_model, tg::Model& tg_model, tg::Node& node){

    if(node.mesh > -1 && node.mesh < tg_model.meshes.size()){
        load_mesh(d_model, tg_model, tg_model.meshes[node.mesh]);
    }

    for(u64 i = 0; i < node.children.size(); i++){
        load_model_nodes(d_model, tg_model, tg_model.nodes[node.children[i]]);
    }

}

void D_Renderer::load_gltf_model(D_Model& d_model, const char* filename){

    tg::Model tg_model;
    std::string err;
    std::string warn;
    //const std::string model_name = "C:\\dev\\glTF-Sample-Models\\2.0\\BoxVertexColors\\glTF\\BoxVertexColors.gltf";
    bool ret = loader.LoadASCIIFromFile(&tg_model, &err, &warn, filename);

    if (!warn.empty()) {
        printf("Warn: %s\n", warn.c_str());
    }

    if (!err.empty()) {
        printf("Err: %s\n", err.c_str());
    }

    if (!ret) {
        printf("Failed to parse glTF\n");
        return;
    }

    const tg::Scene &scene = tg_model.scenes[tg_model.defaultScene];    
    for(u64 i = 0; i < scene.nodes.size(); i++){
        load_model_nodes(d_model, tg_model, tg_model.nodes[scene.nodes[i]]);
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
    shader->d_dx12_release();

    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        direct_command_lists[i]->d_dx12_release();
        rt[i]->d_dx12_release();
    }

    ds->d_dx12_release();
    sampled_texture->d_dx12_release();
    vertex_buffer->d_dx12_release();
    constant_buffer->d_dx12_release();

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

    window_rect = { 0, 0, display_width, display_height };

    // Need to call this before doing much else!
    d_dx12_init(hWnd, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top);

    /*
     *   Create Resource Manager
     */

    resource_manager.init();

    /*
     *   Create Render Targets and Depth Stencil
     */

    Texture_Desc rt_desc;
    rt_desc.usage = Texture::USAGE::USAGE_RENDER_TARGET;
    rt_desc.rtv_connect_to_next_swapchain_buffer = true;


    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        rt[i] = resource_manager.create_texture(L"Render Target", rt_desc);
    }

    Texture_Desc ds_desc;
    ds_desc.usage = Texture::USAGE::USAGE_DEPTH_STENCIL;
    ds_desc.width = display_width;
    ds_desc.height = display_height;

    ds = resource_manager.create_texture(L"Depth Stencil", ds_desc);

    // Set the current back buffer index
	//current_back_buffer_index = display->d3d12_swap_chain->GetCurrentBackBufferIndex();

    /*
    *   Command Allocator and Command List
    */

    // Create command allocators and command lists
    for(int i = 0; i < NUM_BACK_BUFFERS; i++){
        direct_command_lists[i] = create_command_list(&resource_manager, D3D12_COMMAND_LIST_TYPE_DIRECT);
    }

    /*
     *  Shader / PSO
     */

    Shader_Desc shader_desc;

    //
    // Set compiled shader code
    // 
    shader_desc.vertex_shader = L"VertexShader.cso";
    shader_desc.pixel_shader  = L"PixelShader.cso";

    //
    // Sampler Parameter
    //
    Shader_Desc::Parameter::Static_Sampler_Desc sampler_1_ssd;
    sampler_1_ssd.filter           = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler_1_ssd.comparison_func  = D3D12_COMPARISON_FUNC_NEVER;
    sampler_1_ssd.min_lod          = 0;
    sampler_1_ssd.max_lod          = D3D12_FLOAT32_MAX;

    Shader_Desc::Parameter sampler_1;
    sampler_1.name                = "sampler_1";
    sampler_1.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_STATIC_SAMPLER;
    sampler_1.static_sampler_desc = sampler_1_ssd;

    shader_desc.parameter_list.push_back(sampler_1);

    //
    // Texture Parameter
    //
    Shader_Desc::Parameter albedo_texture;
    albedo_texture.name                = "albedo_texture";
    albedo_texture.usage_type          = Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ;

    shader_desc.parameter_list.push_back(albedo_texture);

    //
    // Input Layout
    // 
    shader_desc.input_layout.push_back({"POSITION", DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({"COLOR"   , DXGI_FORMAT_R32G32B32_FLOAT, 0});
    shader_desc.input_layout.push_back({"TEXCOORD"   , DXGI_FORMAT_R32G32_FLOAT, 0});

    //
    // Create PSO using shader reflection
    // 
    shader = create_shader(shader_desc);

    /*
     *  Upload Command_List
     */

    Command_List* upload_command_list = create_command_list(&resource_manager, D3D12_COMMAND_LIST_TYPE_COPY);
    upload_command_list->reset();

    /*
     *  Upload our vertex buffer
     */

    // NOTE: DONT NEGLECT BACKSIDE CULLING (:
    #if 0
    Vertex_Position_Color_Texcoord verticies[6] = {
        // BOTTOM
        {
            {1.00, -1.00, 0.0},
            {0.0, 0.0, 1.0},
            {0.0, 1.0},
        },
        {
            
            {-1.00, -1.00, 0.0},
            {1.0, 0.0, 0.0},
            {1.0, 1.0},
        },
        {
            {-1.00, 1.00, 0.0},
            {0.0, 1.0, 0.0},
            {1.0, 0.0},
        },
        // TOP
        {
            {-1.00, 1.00, 0.0},
            {0.0, 1.0, 0.0},
            {1.0, 0.0},
        },
        {
            {1.00, 1.00, 0.0},
            {1.0, 0.0, 0.0},
            {0.0, 0.0},
        },
        {
            {1.00, -1.00, 0.0},
            {0.0, 0.0, 1.0},
            {0.0, 1.0},
        }
    };
    #endif

    ///////////////////////
    // GLTF Model
    ///////////////////////

    renderer.models.alloc(sizeof(D_Model), 1);
    D_Model& test_model = renderer.models.ptr[0];

    //load_gltf_model(test_model, "C:\\dev\\glTF-Sample-Models\\2.0\\BoxVertexColors\\glTF\\BoxVertexColors.gltf");
    load_gltf_model(test_model, "C:\\dev\\glTF-Sample-Models\\2.0\\DamagedHelmet\\glTF\\DamagedHelmet.gltf");

    // GLtf Loading is broken..
    //load_gltf_model(test_model, "C:\\dev\\glTF-Sample-Models\\2.0\\Sponza\\glTF\\Sponza.gltf");

    // Doesn't work because of load_buffer error:..
    //load_gltf_model(test_model, "C:\\dev\\glTF-Sample-Models\\2.0\\SciFiHelmet\\glTF\\SciFiHelmet.gltf");

    //////////////////////
    // Model Buffers
    //////////////////////

    // Vertex buffer
    Buffer_Desc vertex_buffer_desc = {};
    vertex_buffer_desc.number_of_elements = test_model.verticies.nitems;
    vertex_buffer_desc.size_of_each_element = sizeof(Vertex_Position_Color_Texcoord);
    vertex_buffer_desc.usage = Buffer::USAGE::USAGE_VERTEX_BUFFER;

    vertex_buffer = resource_manager.create_buffer(L"Vertex Buffer", vertex_buffer_desc);

    upload_command_list->load_buffer(vertex_buffer, (u8*)test_model.verticies.ptr, test_model.verticies.nitems * sizeof(Vertex_Position_Color_Texcoord), sizeof(Vertex_Position_Color_Texcoord));//Fix: sizeof(Vertex_Position_Color));

    // Index Buffer
    Buffer_Desc index_buffer_desc = {};
    index_buffer_desc.number_of_elements = test_model.indicies.nitems;
    index_buffer_desc.size_of_each_element = sizeof(u16);
    index_buffer_desc.usage = Buffer::USAGE::USAGE_INDEX_BUFFER;

    index_buffer = resource_manager.create_buffer(L"Index Buffer", index_buffer_desc);

    upload_command_list->load_buffer(index_buffer, (u8*)test_model.indicies.ptr, test_model.indicies.nitems * sizeof(u16), sizeof(u16));//Fix: sizeof(Vertex_Position_Color));

    // Constant Buffer
    Buffer_Desc cbuffer_desc = {};
    cbuffer_desc.number_of_elements = 1;
    cbuffer_desc.size_of_each_element = sizeof(DirectX::XMFLOAT3);
    cbuffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;

    constant_buffer = resource_manager.create_buffer(L"Test Constant Buffer", cbuffer_desc);

    DirectX::XMFLOAT3 constant_buffer_data = {1.0, 0.0, 1.0};

    upload_command_list->load_buffer(constant_buffer, (u8*)&constant_buffer_data, sizeof(constant_buffer_data), 32);

    // Texture Buffer
    /*
    Texture_Desc texture_desc = {};
    texture_desc.usage = Texture::USAGE::USAGE_SAMPLED;
    */

    sampled_texture = resource_manager.create_texture(L"Sampled_Texture", models.ptr[0].texture_desc);

    //upload_command_list->load_texture_from_file(sampled_texture, L"C:\\dev\\glTF-Sample-Models\\2.0\\DamagedHelmet\\glTF\\Default_albedo.jpg");
    if(models.ptr[0].texture.ptr){
        upload_command_list->load_decoded_texture_from_memory(sampled_texture, models.ptr[0].texture);
    }

    ///////////////////////
    // DearIMGUI
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

    upload_command_list->d_dx12_release();

    // Sets up FPS Counter
    tick_list = (double*)calloc(MAX_TICK_SAMPLES, sizeof(double));

    application_is_initialized = true;

    return 0;

}

void D_Renderer::render(){

    // TODO: Rewrite all of this !!!
    static std::chrono::high_resolution_clock::time_point tp1;
    std::chrono::high_resolution_clock::time_point tp2 = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> frame_time = std::chrono::duration_cast<std::chrono::duration<double>>(tp2 - tp1);
    tp1 = tp2;
    // Unsafe!
    char* buffer = (char*)calloc(500, sizeof(char));
    double frame_ms = frame_time.count() * 1000.;
    double avg_frame_ms = avg_ms_per_tick(frame_ms);
    double fps = 1000. / avg_frame_ms;
    //sprintf(buffer, "Frame time: %f milliseconds\nFPS: %f\n", frame_ms, fps);
    //OutputDebugString(buffer);
    free(buffer);

    Command_List* command_list = direct_command_lists[current_backbuffer_index];
    
    // Resets command list, command allocator, and online cbv_srv_uav descriptor heap in resource manager
    command_list->reset();
    // Fill the command list:
    command_list->set_shader(shader);

    // Transition the RT
    command_list->transition_texture(rt[current_backbuffer_index], D3D12_RESOURCE_STATE_RENDER_TARGET);

    command_list->set_render_targets(rt[current_backbuffer_index], ds);

    // Clear the render target and depth stencil
    //FLOAT clear_color[] = {0.9f, float(current_backbuffer_index), 0.7f, 1.0f};
    FLOAT clear_color[] = {0.2f, 0.2, 0.7f, 1.0f};

    command_list->clear_render_target(rt[current_backbuffer_index], clear_color);
    command_list->clear_depth_stencil(ds, 1.0f);

    // Bind the buffers to the command list somewhere in the root signature
    command_list->bind_vertex_buffer(vertex_buffer, 0);
    command_list->bind_index_buffer(index_buffer);

    // Can't use shader parameters that are optimized out
    //command_list->bind_buffer(constant_buffer, &resource_manager, "color_buffer");

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // vickylovesyou!!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    if(models.ptr[0].texture.ptr){
        command_list->bind_texture(sampled_texture, &resource_manager, "albedo_texture");
    }

    // Dont think this is how draw should work, create a draw command struct to pass...
    command_list->draw(renderer.models.ptr[0].indicies.nitems);

    //////////////////////
    /// Dear IMGUI
    //////////////////////
    // Bind IMGUI Fonts
    Descriptor_Handle online_imgui_font_handle = command_list->bind_descriptor_handles_to_online_descriptor_heap(imgui_font_handle, 1);
    ImGuiIO& imgui_io = ImGui::GetIO();
    imgui_io.Fonts->SetTexID((ImTextureID) online_imgui_font_handle.gpu_descriptor_handle.ptr); 

    // Start IMGUI Frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    // Create IMGUI window
    ImGui::Begin("Info");
    ImGui::Text("FPS: %.3lf", fps);
    ImGui::Text("Frame MS: %.2lf", avg_frame_ms);
    ImGui::End();
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), command_list->d3d12_command_list.Get());

    // Transition RT to presentation state
    command_list->transition_texture(rt[current_backbuffer_index], D3D12_RESOURCE_STATE_PRESENT);

    // Execute Command List
    command_list->close();
    execute_command_list(command_list);

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
                        Span<Texture*> rts_to_resize = { renderer.rt, 2 };
                        toggle_fullscreen(rts_to_resize);
                        renderer.ds->resize(renderer.rt[0]->width, renderer.rt[0]->height);
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
                if (!io.WantCaptureMouse){

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

            case(WM_MOUSEMOVE):
            {
                if (!io.WantCaptureMouse){

                }
                io.AddMousePosEvent(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
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
            #if 0
            // Closes the program
            case(WM_SIZING): 
            {
                OutputDebugString("Resizing!\n");
            }
            break;
            case(WM_SIZE): 
            {
                OutputDebugString("Resized!\n");
            }
            break;
            case(WM_MOVING): 
            {
                OutputDebugString("Moving!\n");
            }
            break;
            case(WM_WINDOWPOSCHANGING): 
            {
                OutputDebugString("Position Changing!\n");
                result = DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
            case(WM_WINDOWPOSCHANGED): 
            {
                OutputDebugString("Position Changed!\n");
                result = DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
            case(WM_LBUTTONDOWN): 
            {
                OutputDebugString("Left Click Down!\n");
            }
            break;
            case(WM_LBUTTONUP): 
            {
                OutputDebugString("Left Click Up!\n");
            }
            break;
            case(WM_NCLBUTTONDOWN): 
            {
                OutputDebugString("Non-Client Left Click Down!\n");
                result = DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
            case(WM_NCLBUTTONUP): 
            {
                OutputDebugString("Non-Client Left Click Up!\n");
                result = DefWindowProc(hWnd, msg, wParam, lParam);
            }
            break;
            case(WM_ENTERSIZEMOVE): 
            {
                OutputDebugString("Enter Size Move Loop!\n");
            }
            break;
            #endif
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
                "David's DirectX Window",  // Name at the top of the window	
                //WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,		// A few attributes for the window like minimize, border, ect..
                WS_OVERLAPPEDWINDOW,		// A few attributes for the window like minimize, border, ect..
                CW_USEDEFAULT,				// DefaultPos X
                CW_USEDEFAULT,				// DefaultPos Y
                display_width,	            // Display(!!) Width
                display_height,             // Display(!!) Height
                nullptr,					// Parent Window
                nullptr,					// Menu (Dont want one)
                hInstance,					// HINSTANCE
                nullptr						// Additional Data
            );

        // Checks that it worked
        if (renderer.hWnd == NULL) {
            OutputDebugString("Couldn't Create Window");
            return -1;
        }

        // Initialize app
        renderer.init();

        OutputDebugString("Application Initialized!\n");

        // Show the window
        ShowWindow(renderer.hWnd, nCmdShow);

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
            }
        }

        renderer.shutdown();

    }

    /*
    *   docs.microsoft.com : Closes the COM library on the current thread, unloads all DLLs loaded by the thread,
    *   frees any other resources that the thread maintains, and forces all RPC connections on the thread to close.
    */


    CoUninitialize();

    return 0;
}
