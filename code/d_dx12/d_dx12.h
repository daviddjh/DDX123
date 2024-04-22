#pragma once

#include "../pch.h"
#include "shaders.h"
#include "third_party/d3dx12.h"

#define NUM_DESCRIPTOR_RANGES_IN_TABLE 1
#define DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE 100
#define NUM_BACK_BUFFERS 2
#define IS_BOUND_ONLINE_TABLE_SIZE 500

namespace d_dx12 {

    struct Texture;
    struct Texture_Desc;
    struct Buffer;
    struct Buffer_Desc;
    struct Shader;
    struct Shader_Desc;
    struct Upload_Buffer;

    // Copied to GPU memory in a table { Shader Record 1}, {Shader Record 2}
    struct Shader_Record {
        u8 shader_id[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES] = {NULL};       // Not using CBs yet. D3D12RaytracingSimpleLighting.cpp just placed the CB Pointer below the shader_id size in bytes.
    };
    
    // Points to Shader table in GPU memory
    struct Shader_Table {
        D3D12_GPU_VIRTUAL_ADDRESS addr;
        u32 size_in_bytes;
    };

    struct Shader {

        enum Shader_Type {
            TYPE_GRAPHICS,
            TYPE_COMPUTE,
            TYPE_RAY_TRACE,
        };

        enum Input_Type {
            TYPE_INVALID,                   // Used in the binding poinnts array to define unused binding points
            TYPE_CONSTANT_BUFFER,           // CBV
            TYPE_SHADER_RESOURCE,           // SRV
            TYPE_UNORDERED_ACCESS_RESOURCE, // UAV
            TYPE_INLINE_CONSTANT,           // Root Sig Constant
            TYPE_SAMPLER,
        };

        // Parameter Description
        Microsoft::WRL::ComPtr<ID3D12RootSignature> d3d12_root_signature;

        // Graphics Pipeline State
        Microsoft::WRL::ComPtr<ID3D12PipelineState> d3d12_pipeline_state;

        // Shader Pipeline State
        Microsoft::WRL::ComPtr<ID3D12StateObject> d3d12_rt_state_object;

        Shader::Shader_Type type = Shader::Shader_Type::TYPE_GRAPHICS; 

        // Binding points
        struct Binding_Point {
            Shader::Input_Type                 input_type = Shader::Input_Type::TYPE_INVALID;
            D3D12_SHADER_VISIBILITY            shader_visibility = D3D12_SHADER_VISIBILITY_ALL;
            u8                                 root_signature_index;
            u16                                bind_point;
            u16                                bind_space;
            u16                                bind_count;
            d_std::d_string                    name;
            u32                                cb_size;
        };

        Binding_Point binding_points[BINDING_POINT_INDEX_COUNT];

        Buffer* ray_gen_shader_table;
        ID3D12Resource* ray_gen_shader_table_resource;
        Buffer* hit_group_shader_table;
        Buffer* miss_shader_table;

        void d_dx12_release();

    };

    struct Shader_Desc {

        Shader::Shader_Type type = Shader::Shader_Type::TYPE_GRAPHICS; 

        // Shader bytecode
        wchar_t* vertex_shader = nullptr; 
        union {
            wchar_t* pixel_shader = nullptr; 
            wchar_t* compute_shader; 
            wchar_t* ray_trace_shader; 
        };

        u8           num_render_targets              = 1;
        DXGI_FORMAT* render_target_formats           = nullptr;
        bool         depth_buffer_enabled            = true;

        // Vertex Buffer Input Element Descriptions
        struct Input_Element_Desc {

            d_std::d_string name;
            DXGI_FORMAT format;
            UINT        input_slot;    // Vertex buffer slot
            UINT        offset = D3D12_APPEND_ALIGNED_ELEMENT;

        };

        // List of input elements, not nesseceraly within the same stride, since each element
        // layout could be for a different vertex buffer slot
        std::vector<Input_Element_Desc> input_layout; 

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
        CD3DX12_CPU_DESCRIPTOR_HANDLE                   next_cpu_texture_descriptor_handle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE                   next_gpu_descriptor_handle;
        CD3DX12_GPU_DESCRIPTOR_HANDLE                   next_gpu_texture_descriptor_handle;
        D3D12_DESCRIPTOR_HEAP_TYPE                      type;
        u32                                             descriptor_size;
        bool      is_gpu_visible;
        u16       size;           
        u16       texture_table_size;           
        u16       capacity; 

        void init(D3D12_DESCRIPTOR_HEAP_TYPE type, u16 size, bool is_gpu_visible = false);
        Descriptor_Handle get_next_handle();
        Descriptor_Handle get_next_texture_handle();
        Descriptor_Handle get_handle_by_index(u16 index);
        void reset();

        void d_dx12_release();
    };

    #define IS_CBV_BOUND (1 << 0)
    #define IS_SRV_BOUND (1 << 1)
    #define IS_UAV_BOUND (1 << 2)
    struct Bind_Status {
        u8  bind_status; // Tracks bind status with IS_**V_BOUND flags
        u16 cbv_index;
        u16 srv_index;
        u16 uav_index;
    };

    // Resource Manager
    struct Resource_Manager {

        Descriptor_Heap    rtv_descriptor_heap;
        Descriptor_Heap    dsv_descriptor_heap;
        Descriptor_Heap    offline_cbv_srv_uav_descriptor_heap;
        Descriptor_Heap    online_cbv_srv_uav_descriptor_heap[NUM_BACK_BUFFERS];
        Bind_Status        is_bound_online[IS_BOUND_ONLINE_TABLE_SIZE];
        u8                 is_bound_online_index = 0;

        // TODO: ...
        #if 0
        Descriptor_Heap    offline_sampler_descriptor_heap;
        Descriptor_Heap    online_sampelr_descriptor_heap[NUM_BACK_BUFFERS];
        #endif

        void init();
        Texture* create_texture(wchar_t* name, Texture_Desc& desc);
        Buffer*  create_buffer(wchar_t* name, Buffer_Desc& desc);
        Descriptor_Handle load_dyanamic_frame_data(void* ptr, u64 size, u64 alignment);
        void reset_is_bound_online();
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

        Microsoft::WRL::ComPtr<ID3D12Resource>    d3d12_resource = NULL;
        Descriptor_Handle                         offline_descriptor_handle;
        Descriptor_Handle                         online_descriptor_handle;
        D3D12_RESOURCE_STATES                     state;
        USAGE                                     usage = USAGE_NONE;
        u16                                       width;
        u16                                       height;
        DXGI_FORMAT                               format;
        u16                                       is_bound_index;
        float                                     clear_color[4] = {0.0, 0.0, 0.0, 0.0};
        wchar_t*                                  name = NULL;

        void d_dx12_release();
        void resize(u16 width, u16 height);
    };

    struct Texture_Desc{

        Texture::USAGE usage = Texture::USAGE::USAGE_NONE;
        bool           rtv_connect_to_next_swapchain_buffer = false;
        u16            width;
        u16            height;
        DXGI_FORMAT    format;
        float          clear_color[4] = {0.0, 0.0, 0.0, 0.0};
        wchar_t*       name = NULL;

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
        USAGE                                     usage = USAGE_NONE;
        u64                                       number_of_elements;
        u64                                       size_of_each_element;
        wchar_t*                                  name;
        u16                                       is_bound_index;
        u64                                       alignment;
        union{

            D3D12_VERTEX_BUFFER_VIEW              vertex_buffer_view;
            D3D12_INDEX_BUFFER_VIEW               index_buffer_view;

        };
        
        void d_dx12_release();

    };

    struct Buffer_Desc {

        Buffer::USAGE usage = Buffer::USAGE::USAGE_NONE; 
        u64 number_of_elements;
        u64 size_of_each_element;
        DXGI_FORMAT format          = DXGI_FORMAT_UNKNOWN;
        D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_COPY_DEST;
        D3D12_RESOURCE_FLAGS flags  = D3D12_RESOURCE_FLAG_NONE;
        bool create_cbv = true;
        u64 alignment = 0;

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
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> d3d12_command_list;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>     d3d12_command_allocator;
        Resource_Manager*                                  resource_manager;
        Shader*                                            current_bound_shader;

        void transition_texture(Texture* texture, D3D12_RESOURCE_STATES new_state);
        void transition_buffer(Buffer* buffer, D3D12_RESOURCE_STATES new_state);
        void copy_texture(Texture* src_texture, Texture* dst_texture);
        void clear_render_target(Texture* rt, const float* clear_color);
        void clear_render_target(Texture* rt);
        void clear_depth_stencil(Texture* ds, const float depth);
        Upload_Buffer::Allocation load_buffer(Buffer* buffer, u8* data, u64 size, u64 alignment);
        void load_texture_from_file(Texture* texture, const wchar_t* filename);
        void load_decoded_texture_from_memory(Texture* texture, u_ptr data, bool create_mipchain);
        void reset();
        void close();
        void bind_vertex_buffer(Buffer* buffer, u32 slot);
        void bind_index_buffer(Buffer* buffer);
        void bind_handle(Descriptor_Handle handle, u32 binding_point);
        void bind_buffer(Buffer* buffer, Resource_Manager* resource_manager, u32  binding_point, bool write = false);
        u8   bind_texture(Texture* texture, Resource_Manager* resource_manager, u32  binding_point, bool write = false);
        void bind_constant_arguments(void* data, u16 num_32bit_values_to_set, u32  parameter_name);
        void bind_online_descriptor_heap_texture_table(Resource_Manager* resource_manager, u32 binding_point);
        Descriptor_Handle bind_descriptor_handles_to_online_descriptor_heap(Descriptor_Handle handle, size_t count);
        void set_shader(Shader* shader);
        void set_render_targets(u8 num_render_targets, Texture** rt, Texture* ds);
        void set_viewport(float top_left_x, float top_left_y, float width, float height);
        void set_viewport(D3D12_VIEWPORT viewport);
        void set_scissor_rect(float left, float top, float right, float bottom);
        void set_scissor_rect(D3D12_RECT scissor_rect);
        void draw(u32 number_of_verticies);
        void dispatch(u32 threadgroup_count_x, u32 threadgroup_count_y, u32 threadgroup_count_z);
        void draw_indexed(u32 index_count, u32 index_offset, s32 vertex_offset);
        void d_dx12_release();

    };

    #define DYNAMIC_BUFFER_SIZE _64MB * 4
    struct Dynamic_Buffer {

        struct Allocation {
            D3D12_GPU_VIRTUAL_ADDRESS gpu_addr;
            u8* cpu_addr;
            u32 resource_offset;
            Microsoft::WRL::ComPtr<ID3D12Resource2> d3d12_resource;
            u64 aligned_size;
        };

        Microsoft::WRL::ComPtr<ID3D12Resource2> d3d12_resource;

        u8*  inuse_beginning_ptr;        // Beginning of the range of memory in use
        u8*  inuse_end_ptr;	             // Ending of the range of memory in use
        u8*  absolute_beginning_ptr;     // Beginning of the range of memory in use
        u8*  absolute_ending_ptr;        // Ending of the entire ring buffer allocation
        u64  size;                       // Capacity of the dynamic buffer
        u8*  frame_ending_ptrs[NUM_BACK_BUFFERS] = {0}; 
        void init();
        Allocation allocate(u64 size, u64 alignment);
        void reset_frame(u8 frame);
        void save_frame_ptr(u8 frame);
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

    extern Microsoft::WRL::ComPtr<ID3D12Device8>  d3d12_device;
    extern Display                                display;
    extern u8                                     current_backbuffer_index;
    extern d_std::Memory_Arena*                   d_dx12_arena;

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