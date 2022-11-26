#define WIN32_LEAN_AND_MEAN
#include "windows.h"

#include <d3d12.h>
#include "third_party/d3dx12.h"
#include "dxgi1_6.h"
#include <DirectXMath.h>
#include <d3dcompiler.h>

#include "d_include.h"

#ifdef _DEBUG
#include <dxgidebug.h>
#endif

#if defined(min)
	#undef min
#endif

#if defined(max)
	#undef max
#endif


#define NUM_BACK_BUFFERS 2

// TODO: Make a d_command_queue that has a dx12 command queue and a fence
// TODO: Make a display/surface data structure that has surface size, swap chain, maybe RTV?, display size

namespace d_dx12 {

    struct Texture;
    struct Texture_Desc;
    struct Buffer;
    struct Buffer_Desc;

    struct Shader_Desc {

        // Shader bytecode
        wchar_t* vertex_shader; 
        wchar_t* pixel_shader; 

        // Root Parameters
        // TODO: Should I have different types for Constant Parameters and Descriptor Tables?? Problably..
        struct Parameter {

            std::string name;

            enum Usage_Type {
                TYPE_CONSTANT_BUFFER,   // CBV
                TYPE_TEXTURE_READ,      // SRV
                TYPE_TEXTURE_WRITE,     // UAV
                TYPE_INLINE_CONSTANT,   // Root Sig Constant
                TYPE_STATIC_SAMPLER
            };

            Usage_Type usage_type;

            struct Static_Sampler_Desc {
                D3D12_FILTER filter;
                D3D12_COMPARISON_FUNC comparison_func;
                float min_lod;
                float max_lod;
            };

            union {

                Static_Sampler_Desc static_sampler_desc;
                u16 count = 1;                  // Number of descriptors in descriptor range (Buffer, Texture)
                u16 number_of_32bit_values; // Number of 32bit values in a constant parameter (Constant)

            };

            u16 register_space = 0;
            u16 base_shader_register;
            D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

        };

        // Descriptor Tables / Register Spaces
        std::vector<Parameter> parameter_list;

        // PSO
        //struct Pipeline_State {

            // Vertex Buffer Input Element Descriptions
            struct Input_Element_Desc {

                std::string name;
                DXGI_FORMAT format;
                UINT input_slot;    // Vertex buffer slot
                UINT offset = D3D12_APPEND_ALIGNED_ELEMENT;

            };

            // List of input elements, not nesseceraly within the same stride, since each element
            // layout could be for a different vertex buffer slot
            std::vector<Input_Element_Desc> input_layout; 

        //};

    };

    #define NUM_SHADER_BINDINGS 20
    struct Shader {

        // Parameter Description
        Microsoft::WRL::ComPtr<ID3D12RootSignature> d3d12_root_signature;

        // Graphics Pipeline State
        Microsoft::WRL::ComPtr<ID3D12PipelineState> d3d12_pipeline_state;

        // Binding points
        struct Binding_Point {
            Shader_Desc::Parameter::Usage_Type usage_type;
            D3D12_SHADER_INPUT_BIND_DESC d3d12_binding_desc;
            D3D12_SHADER_VISIBILITY shader_visibility = D3D12_SHADER_VISIBILITY_ALL;
            u8 root_signature_index;
        };

        std::map<std::string, Binding_Point> binding_points;

        void d_dx12_release();

    };

    // Descriptor Handle
    struct Descriptor_Handle {

        CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle;
        D3D12_DESCRIPTOR_HEAP_TYPE    type;

    };

    // Descriptor Heap
    struct Descriptor_Heap {

        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>    d3d12_descriptor_heap = NULL;
        CD3DX12_CPU_DESCRIPTOR_HANDLE                   next_cpu_descriptor_handle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE                   next_gpu_descriptor_handle;
        D3D12_DESCRIPTOR_HEAP_TYPE                      type;
        u32                                             descriptor_size;
        bool                                            is_gpu_visible;

        void init(D3D12_DESCRIPTOR_HEAP_TYPE type, u16 size, bool is_gpu_visible = false);
        Descriptor_Handle get_next_handle();

        void d_dx12_release();
    };

    // Resource Manager
    struct Resource_Manager {

        Descriptor_Heap    rtv_descriptor_heap;
        Descriptor_Heap    dsv_descriptor_heap;
        Descriptor_Heap    offline_cbv_srv_uav_descriptor_heap;
        Descriptor_Heap    online_cbv_srv_uav_descriptor_heap[NUM_BACK_BUFFERS];

        // TODO: ...
        #if 0
        Descriptor_Heap    offline_sampler_descriptor_heap;
        Descriptor_Heap    online_sampelr_descriptor_heap[NUM_BACK_BUFFERS];
        #endif

        void init();
        Texture* create_texture(wchar_t* name, Texture_Desc& desc);
        Buffer*  create_buffer(wchar_t* name, Buffer_Desc& desc);
        void d_dx12_release();
    };

    // Resources (Texture, Buffer)
    struct Texture {

        enum USAGE {
            USAGE_NONE,
            USAGE_STORAGE,
            USAGE_SAMPLED,
            USAGE_RENDER_TARGET,
            USAGE_DEPTH_STENCIL
        };

        Microsoft::WRL::ComPtr<ID3D12Resource>    d3d12_resource;
        Descriptor_Handle                         offline_descriptor_handle;
        Descriptor_Handle                         online_descriptor_handle;
        D3D12_RESOURCE_STATES                     state;
        USAGE                                     usage = USAGE_NONE;
        u16                                       width;
        u16                                       height;
        wchar_t*                                  name = NULL;

        void d_dx12_release();
        void resize(u16 width, u16 height);
    };

    struct Texture_Desc{

        Texture::USAGE usage = Texture::USAGE::USAGE_NONE;
        bool rtv_connect_to_next_swapchain_buffer = true;
        u16  width;
        u16  height;

    };

    struct Buffer {

        enum USAGE {
            USAGE_NONE,
            USAGE_VERTEX_BUFFER,
            USAGE_INDEX_BUFFER,
            USAGE_CONSTANT_BUFFER
        };

        Microsoft::WRL::ComPtr<ID3D12Resource2>   d3d12_resource;
        Descriptor_Handle                         offline_descriptor_handle;
        Descriptor_Handle                         online_descriptor_handle;
        D3D12_RESOURCE_STATES                     state;
        USAGE usage                             = USAGE_NONE;
        u16 number_of_elements;
        u16 size_of_each_element;
        wchar_t*                                  name;
        union{

            D3D12_VERTEX_BUFFER_VIEW              vertex_buffer_view;
            D3D12_INDEX_BUFFER_VIEW               index_buffer_view;

        };
        
        void d_dx12_release();

    };

    struct Buffer_Desc {

        Buffer::USAGE usage = Buffer::USAGE::USAGE_NONE; 
        u16 number_of_elements;
        u16 size_of_each_element;

    };

    // Command Queue
    struct Command_Queue {

        Microsoft::WRL::ComPtr<ID3D12CommandQueue>  d3d12_command_queue;
        Microsoft::WRL::ComPtr<ID3D12Fence>         d3d12_fence;
        u64                                         fence_value;

        void flush();
        u64  signal();
        void wait_for_fence_value(u64 fence_value_to_wait_for);
        void d_dx12_release();
    };

    // Command List
    struct Command_List {
        
        D3D12_COMMAND_LIST_TYPE                            type;
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList1> d3d12_command_list;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     d3d12_command_allocator;
        Resource_Manager*                                  resource_manager;
        Shader*                                            current_bound_shader;

        void transition_texture(Texture* texture, D3D12_RESOURCE_STATES new_state);
        void transition_buffer(Buffer* buffer, D3D12_RESOURCE_STATES new_state);
        void clear_render_target(Texture* rt, const float* clear_color);
        void clear_depth_stencil(Texture* ds, const float depth);
        void load_buffer(Buffer* buffer, u8* data, u64 size, u64 alignment);
        void load_texture_from_file(Texture* texture, const wchar_t* filename);
        void reset();
        void close();
        void bind_vertex_buffer(Buffer* buffer, u32 slot);
        void bind_index_buffer(Buffer* buffer);
        void bind_buffer(Buffer* buffer, Resource_Manager* resource_manager, std::string binding_point);
        void bind_texture(Texture* texture, Resource_Manager* resource_manager, std::string binding_point);
        Descriptor_Handle bind_descriptor_handles_to_online_descriptor_heap(Descriptor_Handle handle, size_t count);
        void set_shader(Shader* shader);
        void set_render_targets(Texture* rt, Texture* ds);
        void draw(u32 number_of_verticies);
        void d_dx12_release();

    };

    struct Upload_Buffer {

        struct Allocation {
            D3D12_GPU_VIRTUAL_ADDRESS gpu_addr;
            u8* cpu_addr;
            u32 resource_offset;
            Microsoft::WRL::ComPtr<ID3D12Resource2> d3d12_resource;
        };

        u32 capacity;
        u32 size;
        u32 offset;
        u8* start_cpu;
        u8* current_cpu;
        D3D12_GPU_VIRTUAL_ADDRESS start_gpu;
        D3D12_GPU_VIRTUAL_ADDRESS current_gpu;
        Microsoft::WRL::ComPtr<ID3D12Resource2> d3d12_resource;

        void init();
        Allocation allocate(u64 size, u64 alignment);
        void d_dx12_release();

    };

    // Display (Basically Just swapchain)
    struct Display {
        Microsoft::WRL::ComPtr<IDXGISwapChain4>         d3d12_swap_chain;
        D3D12_RECT                                      scissor_rect;
        D3D12_VIEWPORT                                  viewport;
        u16                                             display_width;
        u16                                             display_height;
        u8                                              next_buffer = 0;
        bool                                            fullscreen_mode = false;
        HWND                                            win32_hwnd;
        RECT                                            window_rect;
        void d_dx12_release();
    };

    extern Microsoft::WRL::ComPtr<ID3D12Device2>           d3d12_device;
    extern Display           display;
    extern u8 current_backbuffer_index;
    //extern HANDLE                                          fence_event;
    //extern Microsoft::WRL::ComPtr<ID3D12CommandAllocator>  d3d12_command_allocator[NUM_BACK_BUFFERS];
    //extern Microsoft::WRL::ComPtr<ID3D12CommandList>       d3d12_command_lists[NUM_BACK_BUFFERS];
    //extern Microsoft::WRL::ComPtr<ID3D12RootSignature>     d3d12_root_signature;
    //extern Microsoft::WRL::ComPtr<ID3DBlob>                d3d12_vertex_shader_blob;
    //extern Microsoft::WRL::ComPtr<ID3DBlob>                d3d12_pixel_shader_blob;
    //extern Microsoft::WRL::ComPtr<ID3D12PipelineState>     d3d12_pipline_state;

    void d_dx12_init(HWND hWnd, u16 display_width, u16 display_height);
    void d_dx12_shutdown();
    void resize_depth_buffer(Command_Queue* command_queue, u16 width, u16 height);
    void create_display(HWND& hWnd, u32 width, u32 height);
    bool CheckTearingSupport();
    void UpdateRTVs();
    void create_command_queues();
    void create_display();
    void execute_command_list(Command_List* command_list);
    void present(bool using_v_sync);
    void flush_gpu();
    void toggle_fullscreen(d_std::Span<Texture*> rts_to_resize);

    Command_List* create_command_list(Resource_Manager* , D3D12_COMMAND_LIST_TYPE);
    Shader*       create_shader(Shader_Desc& desc);


};