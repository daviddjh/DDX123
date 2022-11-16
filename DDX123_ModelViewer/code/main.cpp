#include "pch.h"
#include "d_dx12.h"
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

#include <chrono>

using namespace d_dx12;

bool application_is_initialized = false;

struct Vertex_Position_Color_Texcoord {
    DirectX::XMFLOAT3 position;
    DirectX::XMFLOAT3 color;
    DirectX::XMFLOAT2 texture_coordinates;
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
    Buffer*        constant_buffer;
    d_dx12::Descriptor_Handle imgui_font_handle;

    int  init();
    void render();
    void shutdown();

};

#define MAX_TICK_SAMPLES 20
int tick_index = 0;
double tick_sum = 0.;
double *tick_list = NULL;

/*******************/

u16 display_width  = 1920;
u16 display_height = 1080;
bool using_v_sync = false;

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

    // Need to call this before doing much else!
    d_dx12_init(hWnd, display_width, display_height);

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

    Buffer_Desc buffer_desc = {};
    buffer_desc.number_of_elements = sizeof(verticies) / sizeof(verticies[0]);
    buffer_desc.size_of_each_element = sizeof(Vertex_Position_Color_Texcoord);
    buffer_desc.usage = Buffer::USAGE::USAGE_VERTEX_BUFFER;

    vertex_buffer = resource_manager.create_buffer(L"Vertex Buffer", buffer_desc);

    upload_command_list->load_buffer(vertex_buffer, (u8*)verticies, sizeof(verticies), sizeof(Vertex_Position_Color_Texcoord));//Fix: sizeof(Vertex_Position_Color));

    Buffer_Desc cbuffer_desc = {};
    cbuffer_desc.number_of_elements = 1;
    cbuffer_desc.size_of_each_element = sizeof(DirectX::XMFLOAT3);
    cbuffer_desc.usage = Buffer::USAGE::USAGE_CONSTANT_BUFFER;

    constant_buffer = resource_manager.create_buffer(L"Test Constant Buffer", cbuffer_desc);

    DirectX::XMFLOAT3 constant_buffer_data = {1.0, 0.0, 1.0};

    upload_command_list->load_buffer(constant_buffer, (u8*)&constant_buffer_data, sizeof(constant_buffer_data), 32);

    Texture_Desc texture_desc = {};
    texture_desc.usage = Texture::USAGE::USAGE_SAMPLED;

    sampled_texture = resource_manager.create_texture(L"Sampled_Texture", texture_desc);

    upload_command_list->load_texture_from_file(sampled_texture, L"mountains.jpg");

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

    // Can't use shader parameters that are optimized out
    //command_list->bind_buffer(constant_buffer, &resource_manager, "color_buffer");

    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // vickylovesyou!!
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

    command_list->bind_texture(sampled_texture, &resource_manager, "albedo_texture");

    // Dont think this is how draw should work, create a draw command struct to pass...
    command_list->draw(6);

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

            // Mouse Button
            case(WM_SYSKEYDOWN):
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
            case(WM_MOVE): 
            {
                OutputDebugString("Moved!\n");
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

        D_Renderer renderer;

        // Create the window using our class..
        renderer.hWnd = CreateWindowEx(
                WS_EX_TOPMOST,
                "David Window Class",		// Defined previously
                "David's DirectX Window",  // Name at the top of the window	
                WS_OVERLAPPEDWINDOW ^ WS_THICKFRAME,		// A few attributes for the window like minimize, border, ect..
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
                else if (msg.message == WM_PAINT) {
                    renderer.render();
                }
                else {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
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
