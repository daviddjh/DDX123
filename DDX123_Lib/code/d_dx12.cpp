#include "pch.h"
#include "d_dx12.h"
#include "WICTextureLoader.h"
#include "DirectXTex.h"

#define NUM_DESCRIPTOR_RANGES_IN_TABLE 1
#define DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE 75

// Are we using WARP ?
#define USING_WARP 0

using namespace d_std;
using namespace DirectX;

namespace d_dx12 {

    Microsoft::WRL::ComPtr<ID3D12Device2>           d3d12_device;
    Command_Queue                                   direct_command_queue;
    Command_Queue                                   copy_command_queue;
    u64                                             frame_fence_values[NUM_BACK_BUFFERS];
    Display                                         display;
    Upload_Buffer                                   upload_buffer;
    Dynamic_Buffer                                  dynamic_buffer;
	
    u8 current_backbuffer_index = 0;
    bool is_tearing_supported = false;


    /* 
    *   Initialize the library by creating the d3d12 device and debug stuff
    */
    void d_dx12_init(HWND hWnd, u16 display_width, u16 display_height){
            
        /*
        *   INIT / DEBUG
        */

        UINT dxgi_factory_flags = 0;

#if defined(_DEBUG)
        Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
        Microsoft::WRL::ComPtr<ID3D12Debug1> debugController1;
        Microsoft::WRL::ComPtr<IDXGIInfoQueue> debugInfoQueue;

        // Enable Debug Layer
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
            debugController->EnableDebugLayer();

            dxgi_factory_flags |= DXGI_CREATE_FACTORY_DEBUG;
        }
        else {
            OutputDebugString("Failed to Enable Debug Layer\n");
            DEBUG_BREAK;
        }

        // Enable Gpu Based Validation
        if (SUCCEEDED(debugController->QueryInterface(IID_PPV_ARGS(&debugController1)))) {
            debugController1->SetEnableGPUBasedValidation(true);
        }
        else {
            OutputDebugString("Failed Set GPU Based Validation True\n");
            DEBUG_BREAK;
        }

        // Set up info queue
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugInfoQueue)))) {
            debugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            debugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);
            debugInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_WARNING, true);
        } 

        DXGI_INFO_QUEUE_MESSAGE_ID hide_ids[] = {

            // DXGI ERROR: IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides.
            80

        };
        DXGI_INFO_QUEUE_FILTER filter = {};
        filter.DenyList.NumIDs = _countof(hide_ids);
        filter.DenyList.pIDList = hide_ids;
        debugInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);

#endif // _DEBUG

        Microsoft::WRL::ComPtr<IDXGIFactory6> dxgi_factory;
        Microsoft::WRL::ComPtr<ID3D12Device2> temp_device;

        // Get DXGIFactory
        if (FAILED(CreateDXGIFactory2(dxgi_factory_flags, IID_PPV_ARGS(dxgi_factory.GetAddressOf())))) {
            OutputDebugString("Failed to create dxgi_factory\n");
            DEBUG_BREAK;
        }
        
        // If Not using WARP, go through the avalible adapters and choose the one with the most memory
        if (!USING_WARP) {

            Microsoft::WRL::ComPtr<IDXGIAdapter1> temp_adapter1;
            Microsoft::WRL::ComPtr<IDXGIAdapter1> temp_adapter4;
            DXGI_ADAPTER_DESC1 pDesc;
            SIZE_T AdapterMemSize = 0;  // As a rule, we pick the adapter with the most memory
            char buffer[500];
            for (UINT i = 0; dxgi_factory->EnumAdapters1(i, &temp_adapter1) == S_OK; i++) {

                temp_adapter1->GetDesc1(&pDesc);
                if (pDesc.DedicatedVideoMemory > AdapterMemSize &&
                    SUCCEEDED(D3D12CreateDevice(temp_adapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
                    AdapterMemSize = pDesc.DedicatedVideoMemory;
                    temp_adapter1.As(&temp_adapter4);
                    sprintf(buffer, "Found adapter: %ls with %uMB\n", pDesc.Description, unsigned int (pDesc.DedicatedVideoMemory >> 20));
                    OutputDebugString(buffer);
                }
            }

            if (FAILED(D3D12CreateDevice(temp_adapter4.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(temp_device.ReleaseAndGetAddressOf())))) {
                OutputDebugString("Failed to create d3d12 device\n");
                DEBUG_BREAK;
            }

            
            d3d12_device = temp_device.Get();
            temp_device.Reset();
        }

        // If no adapters were found, use WARP
        if (d3d12_device == nullptr){
            Microsoft::WRL::ComPtr<IDXGIAdapter1> warp_adapter;
            ThrowIfFailed(dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(warp_adapter.GetAddressOf())));
            ThrowIfFailed(D3D12CreateDevice(warp_adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(temp_device.GetAddressOf())));

            d3d12_device = temp_device.Get();
            temp_device.Reset();
        }

    #if defined(_DEBUG)
        
        // Configure info Queue
        ID3D12InfoQueue* d3d12_info_queue;
        ThrowIfFailed(d3d12_device->QueryInterface(&d3d12_info_queue));

        // Deny specific Categories, Severities, and IDs
        D3D12_MESSAGE_SEVERITY severities[] = {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        D3D12_INFO_QUEUE_FILTER info_queue_filter = {};
        info_queue_filter.DenyList.NumSeverities = _countof(severities);
        info_queue_filter.DenyList.pSeverityList = severities;
        d3d12_info_queue->PushStorageFilter(&info_queue_filter);
        d3d12_info_queue->Release();
        OutputDebugString("Debug Layer Enabled!\n");
    #endif

        d3d12_device->SetName(L"Main d_dx12 DirectX Device");

        // Check if tearing is supported
        is_tearing_supported = CheckTearingSupport();

        // Create Command Queues
        create_command_queues();

        // Set up swapchain
        create_display(hWnd, display_width, display_height);

        // Flush Command Queue 0
        direct_command_queue.flush();

        // Set up the Upload Buffer (allocate GPU memory, map memory, set ptrs)
        upload_buffer.init();

        // Set up the Dynamic Ring Buffer (allocate GPU memory, map memory, set ptrs)
        dynamic_buffer.init();
    }

    void d_dx12_shutdown(){

        direct_command_queue.d_dx12_release();
        copy_command_queue.d_dx12_release();
        display.d_dx12_release();
        upload_buffer.d_dx12_release();
        d3d12_device.Reset();

    }

    /*
    *   Texture!
    */
    void Texture::resize(u16 width, u16 height){
        this->d3d12_resource.Reset();

        switch(this->usage){
            case(Texture::USAGE::USAGE_DEPTH_STENCIL):
            {

                D3D12_CLEAR_VALUE optimizedClearValue = {};
                optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
                optimizedClearValue.DepthStencil = { 1.0f, 0 };

                D3D12_HEAP_PROPERTIES depth_buffer_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

                D3D12_RESOURCE_DESC  depth_buffer_resource_description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
                    1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

                ThrowIfFailed(d3d12_device->CreateCommittedResource(
                    &depth_buffer_heap_properties,
                    D3D12_HEAP_FLAG_NONE,
                    &depth_buffer_resource_description,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    &optimizedClearValue,
                    IID_PPV_ARGS(&this->d3d12_resource)
                ));

                if(this->name)
                    this->d3d12_resource->SetName(name);

                // Update the depth stencil view
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv = { };
                dsv.Format = DXGI_FORMAT_D32_FLOAT;
                dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsv.Texture2D.MipSlice = 0;
                dsv.Flags = D3D12_DSV_FLAG_NONE;

                d3d12_device->CreateDepthStencilView(this->d3d12_resource.Get(), &dsv, this->offline_descriptor_handle.cpu_descriptor_handle);

            }
            break;
            case(Texture::USAGE::USAGE_NONE):
            default:
                OutputDebugString("Error (Texture::resize): Invalid Texture Usage");
                DEBUG_BREAK;
        }

        this->state = D3D12_RESOURCE_STATE_COMMON;
    }

    void Texture::d_dx12_release(){
        if(d3d12_resource)
            d3d12_resource.Reset();
    }

    /*
    *   Buffer!
    */
    void Buffer::d_dx12_release(){
        d3d12_resource.Reset();
    }

    /*
     *   Shader!
     */

    void reflect_shader(Shader_Desc& desc, Shader* shader){


        #define MAX_FILE_SIZE _1MB
        HANDLE hFile; 
        u8*    ReadBuffer = (u8*)calloc(MAX_FILE_SIZE, sizeof(u8));
        OVERLAPPED overlapped_flag = {0};
        DWORD number_of_bytes_read;

        // TODO: D_ARRAY
        u16 shader_binding_array_index = 0;

        // Vertex Shader
        {
            hFile = CreateFileW(desc.vertex_shader,              // file to open
                                GENERIC_READ,          // open for reading
                                FILE_SHARE_READ,       // share for reading
                                NULL,                  // default security
                                OPEN_EXISTING,         // existing file only
                                FILE_ATTRIBUTE_NORMAL, // normal file
                                NULL);                 // no attr. template
                
            if (hFile == INVALID_HANDLE_VALUE) 
            { 
                OutputDebugString("Terminal failure: unable to open shader file for read.\n");
                DEBUG_BREAK;
                return; 
            }

            // Read one character less than the buffer size to save room for
            // the terminating NULL character. 

            if( FALSE == ReadFile(hFile, ReadBuffer, MAX_FILE_SIZE - 1, &number_of_bytes_read, &overlapped_flag) )
            {
                char* buffer = (char*)calloc(500, sizeof(char));
                OutputDebugString("Terminal failure: Unable to read from file.\n");
                sprintf(buffer, "Terminal failure: Unable to read from file.\n GetLastError=%08x\n", GetLastError());
                OutputDebugString(buffer);
                free(buffer);
                CloseHandle(hFile);
                return;
            }


            Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shader_reflection;
            HRESULT hr = D3DReflect(ReadBuffer, number_of_bytes_read, IID_PPV_ARGS(shader_reflection.GetAddressOf()));
            if(FAILED(hr)){
                DWORD error = GetLastError();
                OutputDebugString("Unable to reflect shader file");
                DEBUG_BREAK;
            }

            D3D12_SHADER_DESC shader_reflection_desc;
            if(shader_reflection){
                shader_reflection->GetDesc(&shader_reflection_desc);
            } else {
                OutputDebugString("Unable to reflect shader file");
                DEBUG_BREAK;
            }

            for (int i = 0; i < shader_reflection_desc.BoundResources; i++){

                //D3D12_SHADER_INPUT_BIND_DESC bind_desc;
                if(shader_binding_array_index < NUM_SHADER_BINDINGS){

                    D3D12_SHADER_INPUT_BIND_DESC shader_input_bind_desc;
                    shader_reflection->GetResourceBindingDesc(i, &(shader_input_bind_desc));
                    shader->binding_points[shader_input_bind_desc.Name].d3d12_binding_desc = shader_input_bind_desc;

                    if(shader->binding_points[shader_input_bind_desc.Name].shader_visibility != D3D12_SHADER_VISIBILITY_ALL){ 

                        shader->binding_points[shader_input_bind_desc.Name].shader_visibility = D3D12_SHADER_VISIBILITY_ALL;

                    } else {

                        shader->binding_points[shader_input_bind_desc.Name].shader_visibility = D3D12_SHADER_VISIBILITY_VERTEX;

                    }

                }

            }

            CloseHandle(hFile);
        }

        // Pixel Shader
        {
            hFile = CreateFileW(desc.pixel_shader,              // file to open
                                GENERIC_READ,          // open for reading
                                FILE_SHARE_READ,       // share for reading
                                NULL,                  // default security
                                OPEN_EXISTING,         // existing file only
                                FILE_ATTRIBUTE_NORMAL, // normal file
                                NULL);                 // no attr. template
                
            if (hFile == INVALID_HANDLE_VALUE) 
            { 
                OutputDebugString("Terminal failure: unable to open shader file for read.\n");
                DEBUG_BREAK;
                return; 
            }

            // Read one character less than the buffer size to save room for
            // the terminating NULL character. 

            if( FALSE == ReadFile(hFile, ReadBuffer, MAX_FILE_SIZE - 1, &number_of_bytes_read, &overlapped_flag) )
            {
                char* buffer = (char*)calloc(500, sizeof(char));
                OutputDebugString("Terminal failure: Unable to read from file.\n");
                sprintf(buffer, "Terminal failure: Unable to read from file.\n GetLastError=%08x\n", GetLastError());
                OutputDebugString(buffer);
                free(buffer);
                CloseHandle(hFile);
                return;
            }


            Microsoft::WRL::ComPtr<ID3D12ShaderReflection> shader_reflection;
            HRESULT hr = D3DReflect(ReadBuffer, number_of_bytes_read, IID_PPV_ARGS(shader_reflection.GetAddressOf()));
            if(FAILED(hr)){
                DWORD error = GetLastError();
                OutputDebugString("Unable to reflect shader file");
                DEBUG_BREAK;
            }

            D3D12_SHADER_DESC shader_reflection_desc;
            if(shader_reflection){
                shader_reflection->GetDesc(&shader_reflection_desc);
            } else {
                OutputDebugString("Unable to reflect shader file");
                DEBUG_BREAK;
            }

            for (int i = 0; i < shader_reflection_desc.BoundResources; i++){

                //D3D12_SHADER_INPUT_BIND_DESC bind_desc;
                if(shader_binding_array_index < NUM_SHADER_BINDINGS){

                    D3D12_SHADER_INPUT_BIND_DESC shader_input_bind_desc;
                    shader_reflection->GetResourceBindingDesc(i, &(shader_input_bind_desc));
                    shader->binding_points[shader_input_bind_desc.Name].d3d12_binding_desc = shader_input_bind_desc;
                    if(shader->binding_points[shader_input_bind_desc.Name].shader_visibility != D3D12_SHADER_VISIBILITY_ALL){ 
                        shader->binding_points[shader_input_bind_desc.Name].shader_visibility = D3D12_SHADER_VISIBILITY_ALL;
                    } else {

                        shader->binding_points[shader_input_bind_desc.Name].shader_visibility = D3D12_SHADER_VISIBILITY_PIXEL;

                    }

                }

            }

            CloseHandle(hFile);
        }

        free(ReadBuffer);

    }

    Shader* create_shader(Shader_Desc& desc){
        Shader* shader = new Shader;

        reflect_shader(desc, shader);

        /*
        *   Set up Root Signiture
        */

        // Create the root signature
        D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = { };
        featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
        if (FAILED(d3d12_device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
            featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
        }

        // Default Flags
        D3D12_ROOT_SIGNATURE_FLAGS root_signature_flags =
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
            D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


        //std::vector<CD3DX12_ROOT_PARAMETER1> root_parameters;
        CD3DX12_ROOT_PARAMETER1 root_parameters[64];
        u8 num_root_paramters = 0;

        CD3DX12_STATIC_SAMPLER_DESC static_samplers[64];
        u8 num_static_samplers = 0;
            
        for(int j = 0; j < desc.parameter_list.size(); j++){

            if(shader->binding_points.count(desc.parameter_list[j].name) == 0 ){

                char* buffer = (char*)calloc(500, sizeof(char));
                sprintf(buffer, "Error (create_shader): The shader code you specified doesn't contain a parameter: \'%s\'\n", desc.parameter_list[j].name.c_str());
                OutputDebugString(buffer);
                free(buffer);
                DEBUG_BREAK;
                continue;
            }
            
            Shader::Binding_Point * shader_binding_point = &(shader->binding_points[desc.parameter_list[j].name]);
            shader_binding_point->usage_type = desc.parameter_list[j].usage_type;

            switch(desc.parameter_list[j].usage_type){

                case(Shader_Desc::Parameter::Usage_Type::TYPE_INLINE_CONSTANT):

                    {

                    root_parameters[num_root_paramters].InitAsConstants(
                        desc.parameter_list[j].number_of_32bit_values,
                        shader_binding_point->d3d12_binding_desc.BindPoint,
                        shader_binding_point->d3d12_binding_desc.Space,
                        desc.parameter_list[j].shader_visibility
                    );
                    shader_binding_point->root_signature_index = num_root_paramters;
                    num_root_paramters++;

                    }

                break;

                case(Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_WRITE):
                case(Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ):
                case(Shader_Desc::Parameter::Usage_Type::TYPE_CONSTANT_BUFFER):

                {
                    Shader::Binding_Point * shader_binding_point = &(shader->binding_points[desc.parameter_list[j].name]);
                    D3D12_DESCRIPTOR_RANGE1* descriptor_range = (D3D12_DESCRIPTOR_RANGE1*)calloc(NUM_DESCRIPTOR_RANGES_IN_TABLE, sizeof(D3D12_DESCRIPTOR_RANGE1));

                    descriptor_range->BaseShaderRegister                = shader_binding_point->d3d12_binding_desc.BindPoint,
                    descriptor_range->RegisterSpace                     = shader_binding_point->d3d12_binding_desc.Space;
                    descriptor_range->OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
                    descriptor_range->Flags                             = D3D12_DESCRIPTOR_RANGE_FLAG_NONE;

                    descriptor_range->NumDescriptors                    = shader_binding_point->d3d12_binding_desc.BindCount;
                    // If the number of descriptors is 0, then we assume it's an unbound descriptor array
                    if(descriptor_range->NumDescriptors == 0){
                        descriptor_range->NumDescriptors = DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE;
                        descriptor_range->Flags |= D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
                    }


                    switch(desc.parameter_list[j].usage_type){
                        case(Shader_Desc::Parameter::Usage_Type::TYPE_CONSTANT_BUFFER):
                            descriptor_range->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
                            break;
                        case(Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_READ):
                            descriptor_range->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
                            break;
                        case(Shader_Desc::Parameter::Usage_Type::TYPE_TEXTURE_WRITE):
                            descriptor_range->RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
                            break;
                    }

                    root_parameters[num_root_paramters].InitAsDescriptorTable(1, descriptor_range, shader->binding_points[desc.parameter_list[j].name].shader_visibility);
                    shader_binding_point->root_signature_index = num_root_paramters;
                    num_root_paramters++;
                }

                break;

                case(Shader_Desc::Parameter::Usage_Type::TYPE_STATIC_SAMPLER):

                    CD3DX12_STATIC_SAMPLER_DESC * static_sampler_desc = &(static_samplers[num_static_samplers]);

                    static_sampler_desc->Filter = desc.parameter_list[j].static_sampler_desc.filter;
                    static_sampler_desc->AddressU = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                    static_sampler_desc->AddressV = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                    static_sampler_desc->AddressW = D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
                    static_sampler_desc->MipLODBias = 0;
                    static_sampler_desc->MaxAnisotropy = 0;
                    static_sampler_desc->ComparisonFunc = desc.parameter_list[j].static_sampler_desc.comparison_func;
                    static_sampler_desc->BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
                    static_sampler_desc->MinLOD = desc.parameter_list[j].static_sampler_desc.min_lod;
                    static_sampler_desc->MaxLOD = desc.parameter_list[j].static_sampler_desc.max_lod;
                    static_sampler_desc->ShaderRegister = shader_binding_point->d3d12_binding_desc.BindPoint;
                    static_sampler_desc->RegisterSpace = shader_binding_point->d3d12_binding_desc.Space;
                    static_sampler_desc->ShaderVisibility = shader_binding_point->shader_visibility;

                    num_static_samplers++;

                break;
            }

        }

        CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
        rootSignatureDescription.Init_1_1(num_root_paramters, root_parameters, num_static_samplers, static_samplers, root_signature_flags);

        // Serialize the root sig
        Microsoft::WRL::ComPtr<ID3DBlob> rootSigitureBlob;
        Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
        HRESULT hr  = D3DX12SerializeVersionedRootSignature(&rootSignatureDescription, featureData.HighestVersion, &rootSigitureBlob, &errorBlob);
        if (FAILED(hr)) {
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
            throw std::exception();
        }
        // Create root sig
        // This can be pre compiled
        ThrowIfFailed(d3d12_device->CreateRootSignature(0, rootSigitureBlob->GetBufferPointer(), rootSigitureBlob->GetBufferSize(), IID_PPV_ARGS(&(shader->d3d12_root_signature))));

        /*
        *   Set pipeline state object
        */



        struct PipelineStateStream {
            CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
            CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
            CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
            CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC blend_desc;
            CD3DX12_PIPELINE_STATE_STREAM_VS VS;
            CD3DX12_PIPELINE_STATE_STREAM_PS PS;
            CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
            CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
        } pipelineStateStream;

        // number of render targets and their format are defined
        D3D12_RT_FORMAT_ARRAY rtvFormats = { };
        rtvFormats.NumRenderTargets = 1;
        rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

        std::vector<D3D12_INPUT_ELEMENT_DESC> input_layout;
        for(int i = 0; i < desc.input_layout.size(); i++){

            D3D12_INPUT_ELEMENT_DESC input_element_desc;
            input_element_desc.SemanticName         = desc.input_layout[i].name.c_str();
            input_element_desc.SemanticIndex        = 0; // Good Default??
            input_element_desc.Format               = desc.input_layout[i].format;
            input_element_desc.InputSlot            = desc.input_layout[i].input_slot;
            input_element_desc.AlignedByteOffset    = desc.input_layout[i].offset; // Good Default?
            input_element_desc.InputSlotClass       = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;  // Good Default?
            input_element_desc.InstanceDataStepRate = 0;

            input_layout.push_back(input_element_desc);
        }

        Microsoft::WRL::ComPtr<ID3DBlob>                d3d12_vertex_shader_blob;
        Microsoft::WRL::ComPtr<ID3DBlob>                d3d12_pixel_shader_blob;

        ThrowIfFailed(D3DReadFileToBlob(desc.vertex_shader, &d3d12_vertex_shader_blob));
        ThrowIfFailed(D3DReadFileToBlob(desc.pixel_shader, &d3d12_pixel_shader_blob));

        // Create Blend Desc
        CD3DX12_BLEND_DESC alpha_blend_desc;
        alpha_blend_desc.AlphaToCoverageEnable = false;
        alpha_blend_desc.IndependentBlendEnable = false;

        // Learned from: https://wickedengine.net/2017/10/22/which-blend-state-for-me/
        D3D12_RENDER_TARGET_BLEND_DESC render_target_blend_desc;
        render_target_blend_desc.BlendEnable   = true;
        render_target_blend_desc.LogicOpEnable = false;
        render_target_blend_desc.SrcBlend  = D3D12_BLEND_SRC_ALPHA;
        render_target_blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
        render_target_blend_desc.BlendOp   = D3D12_BLEND_OP_ADD;
        render_target_blend_desc.SrcBlendAlpha  = D3D12_BLEND_ONE;
        render_target_blend_desc.DestBlendAlpha = D3D12_BLEND_ONE;
        render_target_blend_desc.BlendOpAlpha   = D3D12_BLEND_OP_ADD;
        render_target_blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            alpha_blend_desc.RenderTarget[ i ] = render_target_blend_desc;
        

        // Describe PSO
        pipelineStateStream.pRootSignature        = shader->d3d12_root_signature.Get();
        pipelineStateStream.InputLayout           = { input_layout.data(), (u16)input_layout.size() };
        pipelineStateStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pipelineStateStream.blend_desc            = alpha_blend_desc;
        pipelineStateStream.VS                    = CD3DX12_SHADER_BYTECODE(d3d12_vertex_shader_blob.Get());
        pipelineStateStream.PS                    = CD3DX12_SHADER_BYTECODE(d3d12_pixel_shader_blob.Get());
        pipelineStateStream.DSVFormat             = DXGI_FORMAT_D32_FLOAT;   // Hopefully a good default!
        pipelineStateStream.RTVFormats            = rtvFormats;


        // Create PSO
        D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
            sizeof(PipelineStateStream), &pipelineStateStream
        };

        ThrowIfFailed(d3d12_device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&(shader->d3d12_pipeline_state))));

        return shader;
    }

    void Shader::d_dx12_release(){
        d3d12_root_signature.Reset();
        d3d12_pipeline_state.Reset();
    }

    /*
     *   Command Queue!
     */

    // Creates a Command Queue
    void create_command_queues()
    {

        // Direct Command Queue (for rendering)
        Command_Queue *dcq = &(direct_command_queue);

        // Init fence value
        dcq->fence_value = 0;

        // Create Fence and Event
        ThrowIfFailed(d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(dcq->d3d12_fence.GetAddressOf())));

        // Create Direct Command Queue
        D3D12_COMMAND_QUEUE_DESC direct_queue_desc = {};
        direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        direct_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        direct_queue_desc.NodeMask = 0;
        ThrowIfFailed(d3d12_device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(dcq->d3d12_command_queue.GetAddressOf())));

        Command_Queue *ccq = &(copy_command_queue);

        // Init fence value
        ccq->fence_value = 0;

        // Create Fence and Event
        ThrowIfFailed(d3d12_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(ccq->d3d12_fence.GetAddressOf())));

        // Create Direct Command Queue
        D3D12_COMMAND_QUEUE_DESC copy_queue_desc = {};
        direct_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
        direct_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        direct_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        direct_queue_desc.NodeMask = 0;
        ThrowIfFailed(d3d12_device->CreateCommandQueue(&direct_queue_desc, IID_PPV_ARGS(ccq->d3d12_command_queue.GetAddressOf())));
    }

    void Command_Queue::d_dx12_release(){
        d3d12_command_queue.Reset();
        d3d12_fence.Reset();
    }

    void execute_command_list(Command_List* command_list){

        ID3D12CommandList* const command_lists[] = {
            command_list->d3d12_command_list.Get()
        };

        if(command_list->type == D3D12_COMMAND_LIST_TYPE_DIRECT){
            direct_command_queue.d3d12_command_queue->ExecuteCommandLists(1, command_lists);
        } else if(command_list->type == D3D12_COMMAND_LIST_TYPE_COPY){
            copy_command_queue.d3d12_command_queue->ExecuteCommandLists(1, command_lists);
        }
        // command_queues[current_backbuffer_index].signal(); // This signal would be stored with the command allocator for future use

    }

    /*
    *   Helper functions for fence and flushing queue
    */
    bool is_fence_complete(Microsoft::WRL::ComPtr<ID3D12Fence> d3d12_fence, u64 fence_value) {
        u64 completedValue = d3d12_fence->GetCompletedValue();
        if (completedValue >= fence_value) {
            return true;
        }
        else {
            return false;
        }
    }

    // Waits for a specified fence value
    void Command_Queue::wait_for_fence_value(u64 fence_value_to_wait_for)
    { 
        if (d3d12_fence->GetCompletedValue() < fence_value_to_wait_for) {
            HANDLE fence_event = CreateEvent(NULL, FALSE, FALSE, NULL);
            std::chrono::milliseconds duration = std::chrono::milliseconds::max();
            ThrowIfFailed(d3d12_fence->SetEventOnCompletion(fence_value_to_wait_for, fence_event));
            WaitForSingleObject(fence_event, static_cast<DWORD> (duration.count()));
        }
    }

    // Sets a fence value from the GPU side
    u64 Command_Queue::signal()
    {
        fence_value++;
        ThrowIfFailed(d3d12_command_queue->Signal(d3d12_fence.Get(), fence_value));

        return fence_value;
    }

    // Flushes all commands out of a command queue
    void Command_Queue::flush()
    {
        u64 fence_value_to_wait_for = signal();
        wait_for_fence_value(fence_value_to_wait_for);
    }

    void flush_gpu(){
        direct_command_queue.flush();
        copy_command_queue.flush();
    }

    /*
    *   Command List!
    */
    Command_List* create_command_list(Resource_Manager* resource_manager, D3D12_COMMAND_LIST_TYPE type){
        Command_List* command_list = new Command_List;

        command_list->resource_manager = resource_manager;

        command_list->type = type;
        ThrowIfFailed(d3d12_device->CreateCommandAllocator(type, IID_PPV_ARGS(&command_list->d3d12_command_allocator)));
        ThrowIfFailed(d3d12_device->CreateCommandList(0, type, command_list->d3d12_command_allocator.Get(), nullptr, IID_PPV_ARGS(&command_list->d3d12_command_list)));
        command_list->d3d12_command_list->Close();

        return command_list;
        
    }

    // Resets the command list for future use
    void Command_List::reset(){

        /*
        *   Reset command list and command allocator
        */
        d3d12_command_allocator->Reset();
        d3d12_command_list->Reset(d3d12_command_allocator.Get(), nullptr);
        this->current_bound_shader = 0;


        /*
        *   Reset online cbv_srv_uav descriptor heaps in resource manager
        */

        // Copy command lists do not support setting descriptor heaps
        if(type != D3D12_COMMAND_LIST_TYPE_COPY){

            // Need to reset the (current) online descriptor heap every time we render because we always add things to it!
            //resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].next_cpu_descriptor_handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart();
            //resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].next_gpu_descriptor_handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].d3d12_descriptor_heap->GetGPUDescriptorHandleForHeapStart();
            resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].reset();

            ID3D12DescriptorHeap* ppDescriptorHeap[] = {
                resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].d3d12_descriptor_heap.Get()
            };

            d3d12_command_list->SetDescriptorHeaps(1, ppDescriptorHeap);
        }

    }

    // Transitions a texture resource into the state provided
    void Command_List::transition_texture(Texture* texture, D3D12_RESOURCE_STATES new_state){
        
        // Transition RT out of present state to RT state
        CD3DX12_RESOURCE_BARRIER barrier_to_render_target = CD3DX12_RESOURCE_BARRIER::Transition(
            texture->d3d12_resource.Get(),
            texture->state,
            new_state
        );

        texture->state = new_state;

        // Inserts the barrier into the command list
        d3d12_command_list->ResourceBarrier(1, &barrier_to_render_target);

    }

    void Command_List::transition_buffer(Buffer* buffer, D3D12_RESOURCE_STATES new_state){

        if(buffer->state != new_state){

            // Transition RT out of present state to RT state
            CD3DX12_RESOURCE_BARRIER barrier_to_render_target = CD3DX12_RESOURCE_BARRIER::Transition(
                buffer->d3d12_resource.Get(),
                buffer->state,
                new_state
            );

            buffer->state = new_state;

            // Inserts the barrier into the command list
            d3d12_command_list->ResourceBarrier(1, &barrier_to_render_target);

        }

    }

    // Clears the render target with the specified color
    void Command_List::clear_render_target(Texture* rt, const float* clear_color){

        if(rt->usage != Texture::USAGE::USAGE_RENDER_TARGET){
            OutputDebugString("Error (clear_render_target): Have to pass in a render target texture here");
            DEBUG_BREAK;
        }

        d3d12_command_list->ClearRenderTargetView(rt->offline_descriptor_handle.cpu_descriptor_handle, clear_color, 0, nullptr);
    }


    // Clears the depth stencil with the specified depth
    void Command_List::clear_depth_stencil(Texture* ds, const float depth){

        if(ds->usage != Texture::USAGE::USAGE_DEPTH_STENCIL){
            OutputDebugString("Error (clear_render_target): Have to pass in a depth stencil texture here");
            DEBUG_BREAK;
        }

        d3d12_command_list->ClearDepthStencilView(ds->offline_descriptor_handle.cpu_descriptor_handle, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
    }

    void Command_List::close(){
        d3d12_command_list->Close();
    }

    void Command_List::d_dx12_release(){
        d3d12_command_list.Reset();
        d3d12_command_allocator.Reset();
    }

    /*
    *   Display!
    */

    void create_display(HWND& hWnd, u32 width, u32 height){

        display.win32_hwnd = hWnd;
        display.window_rect = {0, 0, (long)width, (long)height};

        display.display_width  = width;
        display.display_height = height;
        display.next_buffer    = 0;

        /*
        *   Swap Chain
        */

        // Gets the swap chain
        Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory2;
        ThrowIfFailed(CreateDXGIFactory2(0, IID_PPV_ARGS(&dxgiFactory2)));
        
        DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
        swapChainDesc.Width       = width;                  // DISPLAY SIZE
        swapChainDesc.Height      = height;
        swapChainDesc.Format      = DXGI_FORMAT_R8G8B8A8_UNORM;
        swapChainDesc.Stereo      = FALSE;
        swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        swapChainDesc.BufferCount = NUM_BACK_BUFFERS;               // Number of back buffers
        swapChainDesc.Scaling = DXGI_SCALING_NONE;
        swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        swapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
        // Needed. Documentation says this is used for bitblt transfer
        // For flip model, the Sample Desc struct needs to be set to {1, 0}
        swapChainDesc.SampleDesc.Count = 1;
        swapChainDesc.SampleDesc.Quality = 0;

        DXGI_SWAP_CHAIN_FULLSCREEN_DESC fullScreen_SC_Desc = {};
        fullScreen_SC_Desc.Windowed = TRUE;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> tempSwapChain;

        ThrowIfFailed(dxgiFactory2->CreateSwapChainForHwnd(
            direct_command_queue.d3d12_command_queue.Get(),
            hWnd,
            &swapChainDesc,
            &fullScreen_SC_Desc,
            nullptr,
            &tempSwapChain
            ));

        // Lets us control Alt+Enter instead of the window
        ThrowIfFailed(dxgiFactory2->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));
    
        // TODO: Do we really need to do this?
        ThrowIfFailed(tempSwapChain.As(&display.d3d12_swap_chain));

        // Gets the current back buffer index
        current_backbuffer_index = display.d3d12_swap_chain->GetCurrentBackBufferIndex();

        // These are currently being created with display width and display height.
        // Don't know if they should in the future - detach display size from render target size
        display.viewport     = CD3DX12_VIEWPORT(0.0, 0.0, static_cast<float>(width), static_cast<float>(height));
        display.scissor_rect = CD3DX12_RECT(0, 0, static_cast<LONG>(width), static_cast<LONG>(height));

    }   

    void Display::d_dx12_release(){
        d3d12_swap_chain.Reset();
    }

    /*
    *   Descriptor Heap!
    */

    // Initializes a Descriptor Heap with capacity + DEFUALT_UNBOUND_DESCRIPTOR_TABLE_SIZE amount of descriptors
    void Descriptor_Heap::init(D3D12_DESCRIPTOR_HEAP_TYPE type, u16 capacity, bool is_gpu_visible){
        
        // TODO: Should check the size here to ensure it's OK
        this->size     = 0;
        this->capacity = capacity;

        if(is_gpu_visible && (type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)){
            OutputDebugString("Error(Descriptor_Heap::init) This Descriptor Heap type cannot be GPU visible");
            DEBUG_BREAK;
        }
        
        if(is_gpu_visible && capacity <= DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE){
            OutputDebugString("Error(Descriptor_Heap::init) This Descriptor Heap will not be big enough to fit the texture descriptors");
            DEBUG_BREAK;
        }

        this->is_gpu_visible = is_gpu_visible;
        this->type           = type;

        // Create the heap
        D3D12_DESCRIPTOR_HEAP_DESC Descriptor_Heap_Desc = { };
        Descriptor_Heap_Desc.Type = type;
        Descriptor_Heap_Desc.NumDescriptors = this->capacity + DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE;
        if(is_gpu_visible) Descriptor_Heap_Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(d3d12_device->CreateDescriptorHeap(&Descriptor_Heap_Desc, IID_PPV_ARGS(&d3d12_descriptor_heap)));

        // Descriptor size
        descriptor_size = d3d12_device->GetDescriptorHandleIncrementSize(type);

        // Descriptor Heap start within d3d12_descriptor_heap
        next_cpu_descriptor_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

        // GPU Visible Descriptor Heap:
        // [{------------ Texture Descriptors ---------------}{-------------- Rest of Descriptors ------------------}]

        if(is_gpu_visible){

            // Need to offset to make room for unbound texture table
            next_cpu_texture_descriptor_handle = next_cpu_descriptor_handle;
            next_cpu_descriptor_handle.Offset(descriptor_size * DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE);

            next_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetGPUDescriptorHandleForHeapStart());
            // Need to offset to make room for unbound texture table
            next_gpu_texture_descriptor_handle = next_gpu_descriptor_handle;
            next_gpu_descriptor_handle.Offset(descriptor_size * DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE);
        }

    }

    void Descriptor_Heap::d_dx12_release(){
        d3d12_descriptor_heap.Reset();
    }

    // Returns the next handle in the heap
    Descriptor_Handle Descriptor_Heap::get_next_handle(){

        // TODO: Check size/index before doing this? Don't go off the end, ring buffer?
        if(this-> size < this->capacity){

            if(d3d12_descriptor_heap == NULL){
                OutputDebugString("Error (Descriptor_Heap::get_next_handle): The d3d12 Descriptor Heap hasn't been created yet");
                DEBUG_BREAK;
            }

            Descriptor_Handle handle = {
                next_cpu_descriptor_handle,
                next_gpu_descriptor_handle,
                this->type
            };

            this->size += 1;

            next_cpu_descriptor_handle.Offset(descriptor_size);
            if(is_gpu_visible) next_gpu_descriptor_handle.Offset(descriptor_size);

            return handle;

        } else {
            OutputDebugString("Error (get_next_handle): No more handles left!");
            DEBUG_BREAK;
            return { };
        }

    }

    // Returns a handle in the heap based on index
    Descriptor_Handle Descriptor_Heap::get_handle_by_index(u16 index){

        if(index >= 0 && index < this->capacity){

            if(d3d12_descriptor_heap == NULL){
                OutputDebugString("Error (Descriptor_Heap::get_next_handle): The d3d12 Descriptor Heap hasn't been created yet");
                DEBUG_BREAK;
            }

            CD3DX12_CPU_DESCRIPTOR_HANDLE cpu_descriptor_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
            CD3DX12_GPU_DESCRIPTOR_HANDLE gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetGPUDescriptorHandleForHeapStart());

            cpu_descriptor_handle.Offset(descriptor_size * index);
            gpu_descriptor_handle.Offset(descriptor_size * index);
            
            Descriptor_Handle handle = {
                cpu_descriptor_handle,
                gpu_descriptor_handle,
                this->type
            };

            return handle;

        } else {
            OutputDebugString("Error (get_next_handle): Invalid Index!");
            DEBUG_BREAK;
            return { };
        }

    }

    // Returns the next handle in the heap
    // TODO: 
    // Not the best naming for this, should probably have one function that has "Spaces" or something to key off of. This could be
    // confusing
    Descriptor_Handle Descriptor_Heap::get_next_texture_handle(){

        if(!this->is_gpu_visible){
            OutputDebugString("Error (Descriptor_Heap::get_next_handle): This Descriptor Heap doesn't support texture descriptors for unbound texture arrays");
            DEBUG_BREAK;
        }

        // TODO: Check size/index before doing this? Don't go off the end, ring buffer?
        if( this->texture_table_size < DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE){

            if(d3d12_descriptor_heap == NULL){
                OutputDebugString("Error (Descriptor_Heap::get_next_handle): The d3d12 Descriptor Heap hasn't been created yet");
                DEBUG_BREAK;
            }

            Descriptor_Handle handle = {
                next_cpu_texture_descriptor_handle,
                next_gpu_texture_descriptor_handle,
                this->type
            };

            this->texture_table_size += 1;

            next_cpu_texture_descriptor_handle.Offset(descriptor_size);
            if(is_gpu_visible) next_gpu_texture_descriptor_handle.Offset(descriptor_size);

            return handle;

        } else {
            OutputDebugString("Error (get_next_handle): No more handles left!");
            DEBUG_BREAK;
            return { };
        }


    }

    void Descriptor_Heap::reset(){

        // Descriptor Heap start within d3d12_descriptor_heap
        next_cpu_descriptor_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetCPUDescriptorHandleForHeapStart());

        // GPU Visible Descriptor Heap:
        // [{------------ Texture Descriptors ---------------}{-------------- Rest of Descriptors ------------------}]

        if(is_gpu_visible){

            // Need to offset to make room for unbound texture table
            next_cpu_texture_descriptor_handle = next_cpu_descriptor_handle;
            next_cpu_descriptor_handle.Offset(descriptor_size * DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE);

            next_gpu_descriptor_handle = CD3DX12_GPU_DESCRIPTOR_HANDLE(d3d12_descriptor_heap->GetGPUDescriptorHandleForHeapStart());
            // Need to offset to make room for unbound texture table
            next_gpu_texture_descriptor_handle = next_gpu_descriptor_handle;
            next_gpu_descriptor_handle.Offset(descriptor_size * DEFAULT_UNBOUND_DESCRIPTOR_TABLE_SIZE);
        }

        size = 0;
        texture_table_size = 0;

    }

    /*
    *   Resource Manager!
    */

    void Resource_Manager::init(){
        
        // Create descriptor heaps (For DSV and RTV)
        rtv_descriptor_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16);
        dsv_descriptor_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 16);
        offline_cbv_srv_uav_descriptor_heap.init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 300);
        for(int i = 0; i < NUM_BACK_BUFFERS; i++){

            online_cbv_srv_uav_descriptor_heap[i].init(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 300, true);

        }

    }

    // TODO: Move render target "creation out of here". Make a "get_render_target(idx)" function
    Texture* Resource_Manager::create_texture(wchar_t* name, Texture_Desc& desc){
        Texture* texture = new Texture;
        texture->usage = desc.usage;
        texture->name  = desc.name;

        switch(desc.usage){
            case(Texture::USAGE::USAGE_RENDER_TARGET):
            {
                // Descriptor Heap start within d3d12_RTV_descriptor_heap
                if(desc.rtv_connect_to_next_swapchain_buffer){

                    // Get descriptor from Descriptor Heap
                    Descriptor_Handle descriptor_handle = rtv_descriptor_heap.get_next_handle();
                    texture->offline_descriptor_handle = descriptor_handle;

                    // Get Buffer from swap chain
                    ThrowIfFailed(display.d3d12_swap_chain->GetBuffer(display.next_buffer, IID_PPV_ARGS((&texture->d3d12_resource))));
                    display.next_buffer++;
                    
                    texture->d3d12_resource.Get()->SetName(name);
                    d3d12_device->CreateRenderTargetView(texture->d3d12_resource.Get(), nullptr, descriptor_handle.cpu_descriptor_handle);

                    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc;
                    ThrowIfFailed(display.d3d12_swap_chain->GetDesc1(&swap_chain_desc));

                    texture->width = swap_chain_desc.Width;
                    texture->height = swap_chain_desc.Height;

                }

            }
            break;
            case(Texture::USAGE::USAGE_DEPTH_STENCIL):
            {

                D3D12_CLEAR_VALUE optimizedClearValue = {};
                optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
                optimizedClearValue.DepthStencil = { 1.0f, 0 };

                D3D12_HEAP_PROPERTIES depth_buffer_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

                D3D12_RESOURCE_DESC  depth_buffer_resource_description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, desc.width, desc.height,
                    1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

                ThrowIfFailed(d3d12_device->CreateCommittedResource(
                    &depth_buffer_heap_properties,
                    D3D12_HEAP_FLAG_NONE,
                    &depth_buffer_resource_description,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    &optimizedClearValue,
                    IID_PPV_ARGS(&texture->d3d12_resource)
                ));

                texture->d3d12_resource->SetName(name);

                // Update the depth stencil view
                D3D12_DEPTH_STENCIL_VIEW_DESC dsv = { };
                dsv.Format = DXGI_FORMAT_D32_FLOAT;
                dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsv.Texture2D.MipSlice = 0;
                dsv.Flags = D3D12_DSV_FLAG_NONE;

                Descriptor_Handle descriptor_handle = dsv_descriptor_heap.get_next_handle();
                texture->offline_descriptor_handle = descriptor_handle;
                d3d12_device->CreateDepthStencilView(texture->d3d12_resource.Get(), &dsv, descriptor_handle.cpu_descriptor_handle);

            }
            break;
            case(Texture::USAGE::USAGE_SAMPLED):
            {
            
                // Get descriptor from Descriptor Heap
                Descriptor_Handle descriptor_handle = offline_cbv_srv_uav_descriptor_heap.get_next_handle();
                texture->offline_descriptor_handle = descriptor_handle;
                texture->state = D3D12_RESOURCE_STATE_COMMON;
                texture->width = desc.width;
                texture->height = desc.height;
                texture->format = desc.format;
                texture->pixel_size = desc.pixel_size;

                // Empty to start, need to load a file into a resource and connect it to this texture
                // -> LoadTextureFromFile();

            }
            break;
            case(Texture::USAGE::USAGE_NONE):
            default:
                OutputDebugString("Error (create_texture): Invalid Texture Usage");
                DEBUG_BREAK;
        }

        texture->state = D3D12_RESOURCE_STATE_COMMON;

        return texture;

    }

    Buffer* Resource_Manager::create_buffer(wchar_t* name, Buffer_Desc& desc){
        Buffer* buffer = new Buffer;
        buffer->name   = name;
        buffer->usage  = desc.usage;
        buffer->number_of_elements = desc.number_of_elements;
        buffer->size_of_each_element = desc.size_of_each_element;
        u32 total_size = desc.number_of_elements * desc.size_of_each_element;

        buffer->state = D3D12_RESOURCE_STATE_COPY_DEST;

        switch(buffer->usage){
            case(Buffer::USAGE::USAGE_VERTEX_BUFFER):
            {

                // Create the resource in the buffer
                D3D12_HEAP_PROPERTIES heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(total_size);

                d3d12_device->CreateCommittedResource(
                    &heap_prop,
                    D3D12_HEAP_FLAG_NONE,
                    &resource_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(buffer->d3d12_resource.GetAddressOf())
                );

                buffer->vertex_buffer_view.SizeInBytes = desc.number_of_elements * desc.size_of_each_element;
                buffer->vertex_buffer_view.StrideInBytes = desc.size_of_each_element;
                buffer->vertex_buffer_view.BufferLocation = buffer->d3d12_resource->GetGPUVirtualAddress();
            }
            break;

            // TODO...
            case(Buffer::USAGE::USAGE_INDEX_BUFFER):
            {
                // Create the resource in the buffer
                D3D12_HEAP_PROPERTIES heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(total_size);

                d3d12_device->CreateCommittedResource(
                    &heap_prop,
                    D3D12_HEAP_FLAG_NONE,
                    &resource_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(buffer->d3d12_resource.GetAddressOf())
                );

                buffer->index_buffer_view.SizeInBytes = desc.number_of_elements * desc.size_of_each_element;
                buffer->index_buffer_view.BufferLocation = buffer->d3d12_resource->GetGPUVirtualAddress();
                buffer->index_buffer_view.Format = DXGI_FORMAT_R16_UINT;

            }
            break;

            // TODO...
            case(Buffer::USAGE::USAGE_CONSTANT_BUFFER):
            {
                // Should I get a descriptor handle here? It's offline so it should be fine?
                buffer->offline_descriptor_handle = this->offline_cbv_srv_uav_descriptor_heap.get_next_handle();

                // Not sure where to find this, just showed up in an error...
                // Guess we need to align CB size to 256
                // Old NVidia requirement?
                u16 alignment = 256;
                u32 remainder = total_size % alignment;
                u32 aligned_total_size = total_size + (alignment - remainder);

                // Create the resource in the buffer
                D3D12_HEAP_PROPERTIES heap_prop = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
                D3D12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(aligned_total_size);

                d3d12_device->CreateCommittedResource(
                    &heap_prop,
                    D3D12_HEAP_FLAG_NONE,
                    &resource_desc,
                    D3D12_RESOURCE_STATE_COPY_DEST,
                    nullptr,
                    IID_PPV_ARGS(buffer->d3d12_resource.GetAddressOf())
                );

                // Create CBV
                // TODO: 
                // Dont know if this should be created here, or if the CommittedResource should be either,
                // Shouldn't we make this durring a load when the user describes their data, then we create a
                // CommittedResource large enough for their data??
                D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
                cbv_desc.BufferLocation = buffer->d3d12_resource->GetGPUVirtualAddress();
                cbv_desc.SizeInBytes = aligned_total_size;

                d3d12_device->CreateConstantBufferView(&cbv_desc, buffer->offline_descriptor_handle.cpu_descriptor_handle);
            }
            break;
        }

        return buffer;

    }

    Descriptor_Handle Resource_Manager::load_dyanamic_frame_data(void* input_data_ptr, u64 size, u64 alignment){

        // Allocate from dyanamic buffer and copy data over
        Dynamic_Buffer::Allocation allocation = dynamic_buffer.allocate(size, alignment);
        memcpy(allocation.cpu_addr, input_data_ptr, size);

        // Aquire a handle from the online descriptor heap
        Descriptor_Handle handle = this->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

        // Initialize Descriptor
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc;
        cbv_desc.BufferLocation = allocation.gpu_addr;
        // Constant buffer requirement
        cbv_desc.SizeInBytes = AlignPow2Up(allocation.aligned_size, 256); 
        d3d12_device->CreateConstantBufferView(&cbv_desc, handle.cpu_descriptor_handle);

        return handle;
    }

    void Resource_Manager::d_dx12_release(){
        rtv_descriptor_heap.d_dx12_release();
        dsv_descriptor_heap.d_dx12_release();
        offline_cbv_srv_uav_descriptor_heap.d_dx12_release();
        for(int i = 0; i < NUM_BACK_BUFFERS; i++){
            online_cbv_srv_uav_descriptor_heap[i].d_dx12_release();
        }
    }

    /*
     *  Upload Buffer
     *  
     *  This only works for uploading all resources at application start up time.
     *  The heap doesn't delete or keep track of anything in it, so it's only ment
     *  for one time use at the beginning. Upgrading this would mean keeping track of resources
     *  in the heap and refrence counts to those resources. The subresources could be released
     *  / main resource refrence decremented at the end of the frame that is uploading. 
     */

    void Upload_Buffer::init(){

        capacity = _64MB * 2 * 2 * 2 * 2;
        size     = 0;
        offset   = 0;

        D3D12_HEAP_PROPERTIES heap_properties = {};
        heap_properties.Type                  = D3D12_HEAP_TYPE_UPLOAD;
        heap_properties.CPUPageProperty       = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        heap_properties.CreationNodeMask      = 0;
        heap_properties.VisibleNodeMask       = 0;
        heap_properties.MemoryPoolPreference  = D3D12_MEMORY_POOL_UNKNOWN;

        D3D12_HEAP_FLAGS heap_flags = D3D12_HEAP_FLAG_NONE;

        CD3DX12_RESOURCE_DESC resource_desc = CD3DX12_RESOURCE_DESC::Buffer(capacity);
        
        ThrowIfFailed(d3d12_device->CreateCommittedResource(
            &heap_properties,
            heap_flags,
            &((D3D12_RESOURCE_DESC)resource_desc),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(d3d12_resource.GetAddressOf())
        ));

        d3d12_resource->SetName(L"Upload Heap");

        start_gpu   = d3d12_resource->GetGPUVirtualAddress();
        current_gpu = start_gpu;

        d3d12_resource->Map(0, &CD3DX12_RANGE(0, capacity), (void**)&start_cpu);
        current_cpu = start_cpu;

    }

    Upload_Buffer::Allocation Upload_Buffer::allocate(u64 size_of_data, u64 alignment){

        u64 mask = alignment - 1;
        u64 allocation_size   = (size_of_data + mask) & (~mask);
        offset = (offset + mask) & (~mask);

        if(allocation_size + offset > capacity){
            OutputDebugString("Error (Upload_Buffer): Attempt to allocate past buffer end");
            DEBUG_BREAK;
        }

        Allocation allocation;
        allocation.cpu_addr = start_cpu + offset;
        allocation.gpu_addr = start_gpu + offset;
        allocation.resource_offset = offset;
        allocation.d3d12_resource = d3d12_resource;

        offset += allocation_size;

        /*
        current_cpu += allocation_size;
        current_gpu += allocation_size;
        size        += allocation_size;
        */

        return allocation;

    }

    void Upload_Buffer::d_dx12_release(){
        d3d12_resource.Reset();
    }

    void Command_List::load_decoded_texture_from_memory(Texture* texture, Span<u8> data, bool create_mipchain){

        if(texture->usage != Texture::USAGE::USAGE_SAMPLED){
            OutputDebugString("Error (load_texture_from_memory): Invalid Texture Usage");
            return;
        }

        if(data.ptr){

            HRESULT hr;

            u64 row_pitch = texture->width * texture->pixel_size;
            u64 slice_pitch = texture->height * row_pitch;

            // Create a DirectxTex texture for our image
            Image base_texture;
            base_texture.format     = texture->format;
            base_texture.height     = texture->height;
            base_texture.width      = texture->width;
            base_texture.rowPitch   = row_pitch;
            base_texture.slicePitch = slice_pitch;
            base_texture.pixels     = data.ptr;

            // Create a DirectXTex Scratch image for our image
            ScratchImage base_scratch_image;
            if(create_mipchain){
                hr = GenerateMipMaps(base_texture, TEX_FILTER_DEFAULT, 0, base_scratch_image);
                if(FAILED(hr)){
                    OutputDebugString("Error (load_decoded_texture_from_memory): Error when creating mip_chain");
                    DEBUG_BREAK;
                }
            } else {
                //base_scratch_image.InitializeFromImage(base_texture);
                hr = GenerateMipMaps(base_texture, TEX_FILTER_DEFAULT, 2, base_scratch_image);
                if(FAILED(hr)){
                    OutputDebugString("Error (load_decoded_texture_from_memory): Error when creating mip_chain");
                    DEBUG_BREAK;
                }
            }

            hr = CreateTexture(d3d12_device.Get(), base_scratch_image.GetMetadata(), texture->d3d12_resource.GetAddressOf());
            if(FAILED(hr)){
                OutputDebugString("Error (load_decoded_texture_from_memory): Error when creating texture resource");
                DEBUG_BREAK;
            }

            // The above "CreateTexture" function creates the texture with this state
            texture->state = D3D12_RESOURCE_STATE_COPY_DEST;

            texture->d3d12_resource->SetName(L"Texture");

            std::vector<D3D12_SUBRESOURCE_DATA> subresources;
            hr = PrepareUpload(d3d12_device.Get(), base_scratch_image.GetImages(), 
                base_scratch_image.GetImageCount(), base_scratch_image.GetMetadata(), subresources);
            if(FAILED(hr)){
                OutputDebugString("Error (load_decoded_texture_from_memory): Error preparing for texture upload");
                DEBUG_BREAK;
            }

            const u64 upload_buffer_size = GetRequiredIntermediateSize(texture->d3d12_resource.Get(), 0, subresources.size());

            Upload_Buffer::Allocation upload_allocation = upload_buffer.allocate(upload_buffer_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            UpdateSubresources(this->d3d12_command_list.Get(),
                texture->d3d12_resource.Get(), upload_allocation.d3d12_resource.Get(),
                upload_allocation.resource_offset, 0, static_cast<unsigned int>(subresources.size()),
                subresources.data());
            
            // Remap after Update Subresources unmaps
            upload_buffer.d3d12_resource->Map(0, &CD3DX12_RANGE(0, upload_buffer.capacity), (void**)&(upload_buffer.start_cpu));

            this->transition_texture(texture, D3D12_RESOURCE_STATE_COMMON);

            /*
            // Problems because it Maps and Unmaps
            UpdateSubresources(
                this->d3d12_command_list.Get(),
                texture->d3d12_resource.Get(),
                upload_allocation.d3d12_resource.Get(), 
                upload_allocation.resource_offset,
                0,
                subresources.size(),
                subresources.data()
            );
            */

            #if 0
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
            UINT row_count;
            UINT64 row_size;
            UINT64 size;
            d3d12_device->GetCopyableFootprints(&texture->d3d12_resource->GetDesc(), 0, 1, 0,
                                            &footprint, &row_count, &row_size, &size);

            for(u64 i = 0; i < row_count; i++){

                memcpy(upload_allocation.cpu_addr + row_size * i, data.ptr + texture->width * texture->pixel_size * i, texture->width * texture->pixel_size);

            }

            // Describe the destination location of the texture
            CD3DX12_TEXTURE_COPY_LOCATION copy_dest(texture->d3d12_resource.Get(), 0);

            // Describes a subresource within a parent
            D3D12_SUBRESOURCE_FOOTPRINT texture_footprint = {};
            texture_footprint.Format = footprint.Footprint.Format;
            texture_footprint.Width = footprint.Footprint.Width;
            texture_footprint.Height = footprint.Footprint.Height;
            texture_footprint.Depth = 1;
            //
            // !!!!
            // TODO: texture_footprint.RowPitch breaks if the image width isn't a multiple of 256. How do we fix this??
            // !!!!
            //
            texture_footprint.RowPitch = footprint.Footprint.RowPitch;

            // Describes a PLACED subresource within a parent. Note: Offset
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_pitched_desc = {};
            placed_pitched_desc.Offset = upload_allocation.resource_offset;
            placed_pitched_desc.Footprint = texture_footprint;

            // Describes the copy source of the texture.
            // This would be the upload allocation we made in the Upload Buffer,
            // plus an offset into the upload buffer resource.
            CD3DX12_TEXTURE_COPY_LOCATION copy_src(upload_allocation.d3d12_resource.Get(), placed_pitched_desc);

            // Copies the texture from the upload buffer into the commited resource created in LoadWICTextureFromFile
            d3d12_command_list->CopyTextureRegion(
                &copy_dest,
                0, 0, 0,
                &copy_src,
                nullptr
            );
            #endif
            
            TexMetadata final_tex_metadata = base_scratch_image.GetMetadata();
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = final_tex_metadata.format;
            // Only works for 2d textures currently
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = final_tex_metadata.mipLevels;

            d3d12_device->CreateShaderResourceView(texture->d3d12_resource.Get(), &srv_desc, texture->offline_descriptor_handle.cpu_descriptor_handle);

#if 0

            D3D12_HEAP_PROPERTIES heap_properties = {};
            heap_properties.Type = D3D12_HEAP_TYPE_DEFAULT;
            heap_properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            heap_properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC resourceDesc = {};
            resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
            resourceDesc.Alignment = 0;
            resourceDesc.Width = texture->width;
            resourceDesc.Height = texture->height;
            resourceDesc.DepthOrArraySize = 1;
            resourceDesc.MipLevels = 1;
            //resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            resourceDesc.Format = texture->format;
            resourceDesc.SampleDesc = {1, 0};
            resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            ThrowIfFailed(d3d12_device->CreateCommittedResource(
                &heap_properties, D3D12_HEAP_FLAG_NONE, &resourceDesc,
                D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&texture->d3d12_resource)));

            
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
            UINT row_count;
            UINT64 row_size;
            UINT64 size;
            d3d12_device->GetCopyableFootprints(&texture->d3d12_resource->GetDesc(), 0, 1, 0,
                                            &footprint, &row_count, &row_size, &size);

            // Row Pitch:   The row pitch, or width, or physical size, in bytes, of the subresource data
            // Row Pitch = width * pixel size in bytes
            // Slice Pitch: The depth pitch, or width, or physical size, in bytes, of the subresource data
            // Slice Pitch = height * Row Pitch
            // TODO: Break here to inspect Slice and Row pitch

            u64 row_pitch = texture->width * texture->pixel_size;
            u64 slice_pitch = texture->height * row_pitch;

            size_t allocation_size = slice_pitch;
            // TODO: Is this the correct align value???
            Upload_Buffer::Allocation upload_allocation = upload_buffer.allocate(size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            for(u64 i = 0; i < row_count; i++){

                memcpy(upload_allocation.cpu_addr + row_size * i, data.ptr + texture->width * texture->pixel_size * i, texture->width * texture->pixel_size);

            }

            // Describe the destination location of the texture
            CD3DX12_TEXTURE_COPY_LOCATION copy_dest(texture->d3d12_resource.Get(), 0);

            // Describes a subresource within a parent
            D3D12_SUBRESOURCE_FOOTPRINT texture_footprint = {};
            texture_footprint.Format = footprint.Footprint.Format;
            texture_footprint.Width = footprint.Footprint.Width;
            texture_footprint.Height = footprint.Footprint.Height;
            texture_footprint.Depth = 1;
            //
            // !!!!
            // TODO: texture_footprint.RowPitch breaks if the image width isn't a multiple of 256. How do we fix this??
            // !!!!
            //
            texture_footprint.RowPitch = footprint.Footprint.RowPitch;

            // Describes a PLACED subresource within a parent. Note: Offset
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_pitched_desc = {};
            placed_pitched_desc.Offset = upload_allocation.resource_offset;
            placed_pitched_desc.Footprint = texture_footprint;

            // Describes the copy source of the texture.
            // This would be the upload allocation we made in the Upload Buffer,
            // plus an offset into the upload buffer resource.
            CD3DX12_TEXTURE_COPY_LOCATION copy_src(upload_allocation.d3d12_resource.Get(), placed_pitched_desc);
            //CD3DX12_TEXTURE_COPY_LOCATION copy_src(upload_allocation.d3d12_resource.Get(), footprint);

            // Copies the texture from the upload buffer into the commited resource created in LoadWICTextureFromFile
            d3d12_command_list->CopyTextureRegion(
                &copy_dest,
                0, 0, 0,
                &copy_src,
                nullptr
            );
           
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = texture->format;
            // Only works for 2d, 1 MIP level textures currently
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;

            d3d12_device->CreateShaderResourceView(texture->d3d12_resource.Get(), &srv_desc, texture->offline_descriptor_handle.cpu_descriptor_handle);
#endif
        }

        return;
    }

    
    void Dynamic_Buffer::init(){

        HRESULT hr = d3d12_device->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_UPLOAD ),    
            D3D12_HEAP_FLAG_NONE, 
            &CD3DX12_RESOURCE_DESC::Buffer( DYNAMIC_BUFFER_SIZE ), 
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,  
            IID_PPV_ARGS( &d3d12_resource )
        );    

        if(FAILED(hr)){
            OutputDebugString("Failed to init Dynamic Buffer\n");
            DEBUG_BREAK;
            return;
        }

        // No CPU reads will be done from the resource.
        CD3DX12_RANGE readRange(0, 0);
        d3d12_resource->Map( 0, &readRange, &((void*)absolute_beginning_ptr)); 
        inuse_beginning_ptr = absolute_beginning_ptr;
        inuse_end_ptr = inuse_beginning_ptr;
        absolute_ending_ptr = absolute_beginning_ptr + DYNAMIC_BUFFER_SIZE;

    }

    Dynamic_Buffer::Allocation Dynamic_Buffer::allocate(u64 size, u64 alignment){

        u64 aligned_size = AlignPow2Up(size, alignment);
        u8* return_ptr = 0;

        u64 free_space = 0;

        if (inuse_beginning_ptr <= inuse_end_ptr){

            u64 used_space = inuse_end_ptr - inuse_beginning_ptr;
            free_space = DYNAMIC_BUFFER_SIZE - used_space;

        } else {

            free_space = inuse_beginning_ptr - inuse_end_ptr;

        }

        // If our requested allocation fits in the free space
        if( free_space >= aligned_size ){

            if(inuse_end_ptr + aligned_size <= absolute_ending_ptr){

                // If the allocation doesn't run past the end of the buffer
                return_ptr = inuse_end_ptr;
                return_ptr = (u8*)AlignPow2Up((u_ptr)return_ptr, alignment);
                inuse_end_ptr += aligned_size;

            } else {

                // If the allocation runs past the end of the buffer, need to check if we can wrap around
                if(inuse_beginning_ptr - absolute_beginning_ptr <= aligned_size){

                    return_ptr = absolute_beginning_ptr + aligned_size; 
                    return_ptr = (u8*)AlignPow2Up((u_ptr)return_ptr, alignment);
                    inuse_end_ptr = return_ptr + aligned_size;

                } else {

                    OutputDebugString("Error (Dyamic_Buffer::allocate): Out of space");
                    DEBUG_BREAK;
                    return {0};

                }
            }
        }

        Allocation allocation;
        allocation.cpu_addr = return_ptr;
        allocation.resource_offset = return_ptr - absolute_beginning_ptr;
        allocation.gpu_addr = d3d12_resource->GetGPUVirtualAddress() + allocation.resource_offset;
        allocation.d3d12_resource = d3d12_resource;
        allocation.aligned_size = aligned_size;

        return allocation;
    }

    void Dynamic_Buffer::reset_frame(u8 frame){

        if(frame >= sizeof(frame_ending_ptrs)){
            OutputDebugString("Error (save_frame_ptr): Invalid Frame");
            DEBUG_BREAK;
            return;
        }

        // TODO: Any checks needed?

        inuse_beginning_ptr = frame_ending_ptrs[frame];
    }

    void Dynamic_Buffer::save_frame_ptr(u8 frame){
        if(frame >= sizeof(frame_ending_ptrs)){
            OutputDebugString("Error (save_frame_ptr): Invalid Frame");
            DEBUG_BREAK;
            return;
        }

        frame_ending_ptrs[frame] = inuse_end_ptr;
    }

    void Command_List::load_texture_from_file(Texture* texture, const wchar_t* filename){

        if(texture->usage != Texture::USAGE::USAGE_SAMPLED){
            OutputDebugString("Error (load_texture_from_file): Invalid Texture Usage");
            return;
        }

        std::unique_ptr<u8[]> decoded_file_data;
        
        // Describes subresource data
        D3D12_SUBRESOURCE_DATA subresource_data;

        #if 0 // Linking problems..
        HRESULT hr = DirectX::LoadWICTextureFromFile(
            *(d3d12_device.GetAddressOf()),
            filename,
            &(texture->d3d12_resource),
            decoded_file_data,
            subresource_data
        );
        if(FAILED(hr)){
            OutputDebugString("Failed Texture Upload!");
            DEBUG_BREAK;
        }
        #endif

        if(decoded_file_data.get()){

            // Row Pitch:   The row pitch, or width, or physical size, in bytes, of the subresource data
            // Row Pitch = width * pixel size in bytes
            // Slice Pitch: The depth pitch, or width, or physical size, in bytes, of the subresource data
            // Slice Pitch = height * Row Pitch
            // TODO: Break here to inspect Slice and Row pitch
            size_t allocation_size = subresource_data.SlicePitch;
            Upload_Buffer::Allocation upload_allocation = upload_buffer.allocate(allocation_size, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

            memcpy(upload_allocation.cpu_addr, decoded_file_data.get(), allocation_size);

            // Describe the destination location of the texture
            CD3DX12_TEXTURE_COPY_LOCATION copy_dest(texture->d3d12_resource.Get(), 0);

            // Get a description of the texture resource, filled out by LoadWICTextureFromFile
            D3D12_RESOURCE_DESC desc = texture->d3d12_resource->GetDesc();

            // Describes a subresource within a parent
            D3D12_SUBRESOURCE_FOOTPRINT texture_footprint = {};
            texture_footprint.Format = desc.Format;
            texture_footprint.Width = desc.Width;
            texture_footprint.Height = desc.Height;
            texture_footprint.Depth = desc.DepthOrArraySize;
            //
            // !!!!
            // TODO: texture_footprint.RowPitch breaks if the image width isn't a multiple of 256. How do we fix this??
            // !!!!
            //
            texture_footprint.RowPitch = subresource_data.RowPitch;

            // Describes a PLACED subresource within a parent. Note: Offset
            D3D12_PLACED_SUBRESOURCE_FOOTPRINT placed_pitched_desc = {};
            placed_pitched_desc.Offset = upload_allocation.resource_offset;
            placed_pitched_desc.Footprint = texture_footprint;

            // Describes the copy source of the texture.
            // This would be the upload allocation we made in the Upload Buffer,
            // plus an offset into the upload buffer resource.
            CD3DX12_TEXTURE_COPY_LOCATION copy_src(upload_allocation.d3d12_resource.Get(), placed_pitched_desc);

            // Copies the texture from the upload buffer into the commited resource created in LoadWICTextureFromFile
            d3d12_command_list->CopyTextureRegion(
                &copy_dest,
                0, 0, 0,
                &copy_src,
                nullptr
            );
           
            D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
            srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            srv_desc.Format = desc.Format;
            // Only works for 2d, 1 MIP level textures currently
            srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            srv_desc.Texture2D.MipLevels = 1;

            d3d12_device->CreateShaderResourceView(texture->d3d12_resource.Get(), &srv_desc, texture->offline_descriptor_handle.cpu_descriptor_handle);

        }

        return;
    }

    void Command_List::load_buffer(Buffer* buffer, u8* data, u64 size, u64 alignment){

        if(size > (buffer->number_of_elements * buffer->size_of_each_element)){
            OutputDebugString("Error (load_buffer): Trying to copy more data than is available");
            DEBUG_BREAK;
        }

        // Allocate data from the upload buffer
        Upload_Buffer::Allocation upload_allocation = upload_buffer.allocate(size, alignment);
        // Copy our data into that buffer
        memcpy(upload_allocation.cpu_addr, data, size);

        // Transition buffer to copy destination state
        if(buffer->state != D3D12_RESOURCE_STATE_COPY_DEST){
            this->transition_buffer(buffer, D3D12_RESOURCE_STATE_COPY_DEST);
        }

        // Copy the data from the upload buffer resource into the Buffer buffer resource
        d3d12_command_list->CopyBufferRegion(
            buffer->d3d12_resource.Get(),                // Dest Resource
            0,                                           // Dest Resource Offset
            upload_allocation.d3d12_resource.Get(),      // Src Resource
            upload_allocation.resource_offset,           // Src Resource Offset
            size                                         // Copy size
        );

        return;
    }

    void Command_List::bind_vertex_buffer(Buffer* buffer, u32 slot){

        if(buffer->usage == Buffer::USAGE::USAGE_VERTEX_BUFFER){

            if(buffer->state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER){
                this->transition_buffer(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            d3d12_command_list->IASetVertexBuffers(slot, 1, &buffer->vertex_buffer_view); 

            // TODO: is this the best place for this?
            d3d12_command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        } else {
            OutputDebugString("Error: bind_vertex_buffer requires a buffer with usage: USAGE_VERTEX_BUFFER");
            DEBUG_BREAK;
        }

    }

    void Command_List::bind_index_buffer(Buffer* buffer){

        if(buffer->usage == Buffer::USAGE::USAGE_INDEX_BUFFER){

            if(buffer->state != D3D12_RESOURCE_STATE_INDEX_BUFFER){
                this->transition_buffer(buffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);
            }

            d3d12_command_list->IASetIndexBuffer(&buffer->index_buffer_view); 

        } else {
            OutputDebugString("Error: bind_vertex_buffer requires a buffer with usage: USAGE_VERTEX_BUFFER");
            DEBUG_BREAK;
        }

    }

    void Command_List::bind_handle(Descriptor_Handle handle, std::string binding_point){

        d3d12_command_list->SetGraphicsRootDescriptorTable(current_bound_shader->binding_points[binding_point].root_signature_index, handle.gpu_descriptor_handle);
        return;

    }

    void Command_List::bind_buffer(Buffer* buffer, Resource_Manager* resource_manager, std::string binding_point){

        if(buffer->usage != Buffer::USAGE::USAGE_CONSTANT_BUFFER){
            if(resource_manager == NULL){
                OutputDebugString("Error (Command_List::bind_buffer): no valid resource_manager");
                DEBUG_BREAK;
            }

            if(buffer->state != D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER){
                this->transition_buffer(buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
            }

            // OOF thats a long line, descriptive though..
            buffer->online_descriptor_handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

            // Need to copy offline descriptor to online descriptor
            d3d12_device->CopyDescriptorsSimple(1, buffer->online_descriptor_handle.cpu_descriptor_handle, buffer->offline_descriptor_handle.cpu_descriptor_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            d3d12_command_list->SetGraphicsRootDescriptorTable(current_bound_shader->binding_points[binding_point].root_signature_index, buffer->online_descriptor_handle.gpu_descriptor_handle);

        } else {
            OutputDebugString("Error: bind_buffer currently requires a buffer with usage: USAGE_CONSTANT_BUFFER");
            DEBUG_BREAK;
        }
    }

    u8 Command_List::bind_texture(Texture* texture, Resource_Manager* resource_manager, std::string binding_point){
        
        switch(texture->usage){
            case(Texture::USAGE::USAGE_SAMPLED):
            {

                if(resource_manager == NULL){
                    OutputDebugString("Error (Command_List::bind_texture): no valid resource_manager");
                    DEBUG_BREAK;
                }

                if(texture->state != D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE){
                    this->transition_texture(texture, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
                }

                // OOF thats a long line, descriptive though..
                texture->online_descriptor_handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_texture_handle();

                // Copy offline descriptor to online descriptor
                d3d12_device->CopyDescriptorsSimple(1, texture->online_descriptor_handle.cpu_descriptor_handle, texture->offline_descriptor_handle.cpu_descriptor_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

                // Set descriptor in Root Signature
                // No longer needed! Textures now bound to root signature with bind_online_descriptor_heap_texture_table
                //d3d12_command_list->SetGraphicsRootDescriptorTable(current_bound_shader->binding_points[binding_point].root_signature_index, texture->online_descriptor_handle.gpu_descriptor_handle);
            }
            break;

            default:

            OutputDebugString("Error (Command_List::bind_texture): invalid Texture Usage");
            DEBUG_BREAK;
            break;

        }

        // Returns the index of the most recently added texture descriptor
        return resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].texture_table_size - 1;

    }

    // Bind an array of textures starting at the beginning of the online_cbv_srv_uav_descriptor_heap
    void Command_List::bind_online_descriptor_heap_texture_table(Resource_Manager* resource_manager, std::string binding_point){
        
        if(resource_manager == NULL){
            OutputDebugString("Error (Command_List::bind_texture): no valid resource_manager");
            DEBUG_BREAK;
        }

        // If the shader contains this binding point
        if(current_bound_shader->binding_points.count(binding_point)){

            // Get the first handle in the online heap, this is the first handle of the texture table
            Descriptor_Handle handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_handle_by_index(0);

            // Set descriptor in Root Signature
            d3d12_command_list->SetGraphicsRootDescriptorTable(current_bound_shader->binding_points[binding_point].root_signature_index, handle.gpu_descriptor_handle);

        }

        return;
    }

    Descriptor_Handle Command_List::bind_descriptor_handles_to_online_descriptor_heap(Descriptor_Handle handle, size_t count){
        if(handle.type != D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV){
            OutputDebugString("Error (bind_descriptor_handle_to_online_descriptor_heap) Currently no online heap of the type provided! ");
            DEBUG_BREAK;
        }

        Descriptor_Handle online_descriptor_handle = resource_manager->online_cbv_srv_uav_descriptor_heap[current_backbuffer_index].get_next_handle();

        d3d12_device->CopyDescriptorsSimple(count, online_descriptor_handle.cpu_descriptor_handle, handle.cpu_descriptor_handle, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        return online_descriptor_handle;

    }

    void Command_List::bind_constant_arguments(void* data, u16 num_32bit_values_to_set, std::string parameter_name){
        // TODO...
        if(this->type == D3D12_COMMAND_LIST_TYPE_DIRECT){

            // If the shader contains this binding point
            if(&this->current_bound_shader->binding_points.count(parameter_name)){

                Shader::Binding_Point* binding_point = &this->current_bound_shader->binding_points[parameter_name];
                u32 root_signature_index = binding_point->root_signature_index;

                d3d12_command_list->SetGraphicsRoot32BitConstants(root_signature_index, num_32bit_values_to_set, data, 0);

            }

        } else if(this->type == D3D12_COMMAND_LIST_TYPE_COMPUTE){

            // If the shader contains this binding point
            if(&this->current_bound_shader->binding_points.count(parameter_name)){

                Shader::Binding_Point* binding_point = &this->current_bound_shader->binding_points[parameter_name];
                u32 root_signature_index = binding_point->root_signature_index;

                d3d12_command_list->SetComputeRoot32BitConstants(root_signature_index, num_32bit_values_to_set, data, 0);
            
            }
        } else {

            OutputDebugString("Error (set_inline_constants): Cannot set inline constants for this command list type");

        }
    }

    void Command_List::set_shader(Shader* shader){
        this->current_bound_shader = shader;
        d3d12_command_list->SetGraphicsRootSignature(shader->d3d12_root_signature.Get());
        d3d12_command_list->SetPipelineState(shader->d3d12_pipeline_state.Get());
    }

    void Command_List::set_render_targets(u8 num_render_targets, Texture* rt, Texture* ds){

        if(num_render_targets > 0){
            // If you have multiple render targets, they must be sequential in the descriptor heap
            d3d12_command_list->OMSetRenderTargets(num_render_targets, &rt->offline_descriptor_handle.cpu_descriptor_handle, FALSE, &ds->offline_descriptor_handle.cpu_descriptor_handle);
        } else {
            d3d12_command_list->OMSetRenderTargets(0, NULL, FALSE, &ds->offline_descriptor_handle.cpu_descriptor_handle);
        }

    }

    void Command_List::set_viewport(float top_left_x, float top_left_y, float width, float height){

        d3d12_command_list->RSSetViewports(1, &CD3DX12_VIEWPORT(top_left_x, top_left_y, width, height));
    }
    void Command_List::set_viewport(D3D12_VIEWPORT viewport){

        d3d12_command_list->RSSetViewports(1, &viewport);
    }
    void Command_List::set_scissor_rect(float left, float top, float right, float bottom){

        d3d12_command_list->RSSetScissorRects(1, &CD3DX12_RECT(left, top, right, bottom));
    }
    void Command_List::set_scissor_rect(D3D12_RECT scissor_rect){

        d3d12_command_list->RSSetScissorRects(1, &scissor_rect);
    }

    void Command_List::draw(u32 number_of_indicies){
        d3d12_command_list->DrawIndexedInstanced(number_of_indicies, 1, 0, 0, 0); 
    }

    void Command_List::draw_indexed(u32 index_count, u32 index_offset, s32 vertex_offset){
        d3d12_command_list->DrawIndexedInstanced(index_count, 1, index_offset, vertex_offset, 0); 
    }

    void present(bool using_v_sync){

        UINT syncInterval = using_v_sync ? 1 : 0;
        UINT presentFlags = is_tearing_supported && !using_v_sync ? DXGI_PRESENT_ALLOW_TEARING : 0;

        dynamic_buffer.save_frame_ptr(current_backbuffer_index);
        ThrowIfFailed(display.d3d12_swap_chain->Present(syncInterval, presentFlags));

        frame_fence_values[current_backbuffer_index] = direct_command_queue.signal();

        //current_backbuffer_index = display.d3d12_swap_chain->GetCurrentBackBufferIndex();
        current_backbuffer_index = (current_backbuffer_index + 1) % 2;

        // Waits for the new current value to make sure the resources are ready to write to
        direct_command_queue.wait_for_fence_value(frame_fence_values[current_backbuffer_index]);
        dynamic_buffer.reset_frame(current_backbuffer_index);
        

    }

    // Checks whether tearing is supported
    bool CheckTearingSupport(){

        BOOL allowTearing = false;
        Microsoft::WRL::ComPtr<IDXGIFactory4> factory4;

        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(factory4.GetAddressOf())))) {

            Microsoft::WRL::ComPtr<IDXGIFactory5> factory5;

            if (SUCCEEDED(factory4.As(&factory5))) {

                HRESULT hr = factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));

                if (SUCCEEDED(hr) && allowTearing) {
                    allowTearing = true;
                }
            }
        }

        return allowTearing;

    }

    void toggle_fullscreen(Span<Texture*> rts_to_resize){

        if(rts_to_resize.nitems != NUM_BACK_BUFFERS){
            OutputDebugString("Error (toggle_fullscreen): The number of Render targets sent to this function needs to equal the number of backbuffers the swapchain contains (currently 2)");
            DEBUG_BREAK;
        }

        for(int i = 0; i < NUM_BACK_BUFFERS; i++){
            if(rts_to_resize.ptr[i]->usage != Texture::USAGE::USAGE_RENDER_TARGET){
                OutputDebugString("Error (toggle_fullscreen): rts_to_resize needs to contain Textures with usage: USAGE_RENDER_TARGET");
                DEBUG_BREAK;
            }
        }

        flush_gpu();

        if(display.fullscreen_mode == true){

            // Restore window style
            SetWindowLong(display.win32_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

            SetWindowPos(
                display.win32_hwnd,
                HWND_NOTOPMOST,
                display.window_rect.left,
                display.window_rect.top,
                display.window_rect.right - display.window_rect.left,
                display.window_rect.bottom - display.window_rect.top,
                SWP_FRAMECHANGED | SWP_NOACTIVATE
            );

            for(int i = 0; i < NUM_BACK_BUFFERS; i++){
                rts_to_resize.ptr[i]->d_dx12_release();
            }

            ThrowIfFailed(display.d3d12_swap_chain->ResizeBuffers(
                NUM_BACK_BUFFERS,
                display.window_rect.right - display.window_rect.left,
                display.window_rect.bottom - display.window_rect.top,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));

            for(int i = 0; i < NUM_BACK_BUFFERS; i++){
                ThrowIfFailed(display.d3d12_swap_chain->GetBuffer(i, IID_PPV_ARGS(&(rts_to_resize.ptr[i])->d3d12_resource)));
                d3d12_device->CreateRenderTargetView(rts_to_resize.ptr[i]->d3d12_resource.Get(), nullptr, rts_to_resize.ptr[i]->offline_descriptor_handle.cpu_descriptor_handle);
                rts_to_resize.ptr[i]->width  = display.window_rect.right - display.window_rect.left;
                rts_to_resize.ptr[i]->height = display.window_rect.bottom - display.window_rect.top;
                rts_to_resize.ptr[i]->d3d12_resource->SetName(rts_to_resize.ptr[i]->name);
            }

            ShowWindow(display.win32_hwnd, SW_NORMAL);

        } else {

            // Save window rect to restore later
            GetWindowRect(display.win32_hwnd, &display.window_rect);

            // Remove window borders so client area fills the whole screen
            SetWindowLong(display.win32_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME));

            RECT fullscreen_rect;

            #if 0
            if(display.d3d12_swap_chain){
                Microsoft::WRL::ComPtr<IDXGIOutput> output;
                ThrowIfFailed(display.d3d12_swap_chain->GetContainingOutput(&output));
                DXGI_OUTPUT_DESC desc;
                ThrowIfFailed(output->GetDesc(&desc));
                fullscreen_rect = desc.DesktopCoordinates;
            }
            #endif

            // Get the settings of the primary display
            DEVMODE devMode = {};
            devMode.dmSize = sizeof(DEVMODE);
            EnumDisplaySettings(nullptr, ENUM_CURRENT_SETTINGS, &devMode);

            fullscreen_rect = {
                devMode.dmPosition.x,
                devMode.dmPosition.y,
                devMode.dmPosition.x + static_cast<LONG>(devMode.dmPelsWidth),
                devMode.dmPosition.y + static_cast<LONG>(devMode.dmPelsHeight)
            };

            SetWindowPos(
                display.win32_hwnd,
                HWND_TOPMOST,
                fullscreen_rect.left,
                fullscreen_rect.top,
                fullscreen_rect.right,
                fullscreen_rect.bottom,
                SWP_FRAMECHANGED | SWP_NOACTIVATE
            );

            for(int i = 0; i < NUM_BACK_BUFFERS; i++){
                rts_to_resize.ptr[i]->d_dx12_release();
            }

            ThrowIfFailed(display.d3d12_swap_chain->ResizeBuffers(
                NUM_BACK_BUFFERS,
                fullscreen_rect.right - fullscreen_rect.left,
                fullscreen_rect.bottom - fullscreen_rect.top,
                DXGI_FORMAT_R8G8B8A8_UNORM,
                CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
            ));

            for(int i = 0; i < NUM_BACK_BUFFERS; i++){
                ThrowIfFailed(display.d3d12_swap_chain->GetBuffer(i, IID_PPV_ARGS(&(rts_to_resize.ptr[i])->d3d12_resource)));
                d3d12_device->CreateRenderTargetView(rts_to_resize.ptr[i]->d3d12_resource.Get(), nullptr, rts_to_resize.ptr[i]->offline_descriptor_handle.cpu_descriptor_handle);
                rts_to_resize.ptr[i]->width  = fullscreen_rect.right - fullscreen_rect.left;
                rts_to_resize.ptr[i]->height = fullscreen_rect.bottom - fullscreen_rect.top;
                rts_to_resize.ptr[i]->d3d12_resource->SetName(rts_to_resize.ptr[i]->name);
            }

            ShowWindow(display.win32_hwnd, SW_MAXIMIZE);

        }
        
        current_backbuffer_index = display.d3d12_swap_chain->GetCurrentBackBufferIndex();
        display.fullscreen_mode = !display.fullscreen_mode;

    }


#if 0    

    // Resizes the depth buffer
    void resize_depth_buffer(Command_Queue* command_queue, u16 width, u16 height){

        // Flush any GPU commands that might be referencing the depth buffer
        command_queue->Flush(); 

        // These cant be zero
        width = width > 1u ? width : 1u;
        height = height > 1u ? height : 1u;

        /*
        *
        * Resize screen dependent resources...
        *
        * Create a depth buffer:
        */
        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil = { 1.0f, 0 };

        D3D12_HEAP_PROPERTIES depth_buffer_heap_properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

        D3D12_RESOURCE_DESC  depth_buffer_resource_description = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
            1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

        ThrowIfFailed(d3d12_device->CreateCommittedResource(
            &depth_buffer_heap_properties,
            D3D12_HEAP_FLAG_NONE,
            &depth_buffer_resource_description,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimizedClearValue,
            IID_PPV_ARGS(&d3d12_depth_buffer)
        ));

        d3d12_depth_buffer->SetName(L"Main Depth Buffer");

        // Update the depth stencil view
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = { };
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        dsv.Flags = D3D12_DSV_FLAG_NONE;

        d3d12_device->CreateDepthStencilView(d3d12_depth_buffer.Get(), &dsv,
            d3d12_DSV_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
    }

    // Updates descriptors for our render targets
    void UpdateRTVs(){

        // Descriptor Heap start within d3d12_RTV_descriptor_heap
        CD3DX12_CPU_DESCRIPTOR_HANDLE RTVHandle(d3d12_RTV_descriptor_heap->GetCPUDescriptorHandleForHeapStart());
        // Descriptor offset
        u32 RTVDescriptorSize = d3d12_device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        // Loop through back buffers and update their respective descriptors in the d3d12_RTV_descriptor_heap
        for (UINT i = 0; i < NUM_BACK_BUFFERS; i++) {
            ThrowIfFailed(d3d12_swap_chain->GetBuffer(i, IID_PPV_ARGS((d3d12_display_buffers[i]).GetAddressOf())));
            d3d12_display_buffers[i].Get()->SetName(L"Render Target Buffer");
            d3d12_device->CreateRenderTargetView(d3d12_display_buffers[i].Get(), nullptr, RTVHandle);
            RTVHandle.Offset(RTVDescriptorSize);
        }

    }
#endif

}