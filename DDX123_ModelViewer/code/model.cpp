#include "model.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace tg = tinygltf;
using namespace d_dx12;
using namespace DirectX;

tg::TinyGLTF model_loader;

#define BUFFER_OFFSET(i) ((char *)0 + (i))

/*
*   Load a gltf model
*/

#if 0
bool load_image_function(tg::Image *tg_image, const int image_idx, std::string *err, std::string *warn,
    int req_width, int req_height, const unsigned char *bytes, int size, void *user_data){

    HRESULT hr;
    
    // Image name:
    // tg_image.uri.c_str();

    // Load to ScratchImage
    // LoadFromWICMemory
    ScratchImage* image = new ScratchImage();
    TexMetadata  image_metadata;
    hr = LoadFromWICMemory(bytes, size, WIC_FLAGS_ALL_FRAMES, &image_metadata, *image);
    if(FAILED(hr)){
        OutputDebugString("Error (load_image_function): Failed to load image.");
    }

    // GenerateMipChain
    // GenerateMipMaps()

    // Copy ScratchImage to image
    tg_image->image.resize(sizeof(ScratchImage));
    memcpy(tg_image->image.data(), (u8*)image, sizeof(ScratchImage));

    return true;
}
#endif

/*
*   Loads a mesh from the tg_model
*   Stores Indicies and Verticies (Primitive attributes)
*   Currently only supports POSITION, TEXCOORD_0, and COLOR_0
*   Loads to CPU memory only!
*/
void load_mesh(D_Model& d_model, tg::Model& tg_model, tg::Mesh& mesh){

    // Allocate and loop through array of meshes
    d_model.meshes.alloc(tg_model.meshes.size());
    for(u64 mesh_index = 0; mesh_index < d_model.meshes.nitems; mesh_index++){

        D_Mesh* mesh = d_model.meshes.ptr + mesh_index;
        tg::Mesh tg_mesh = tg_model.meshes[mesh_index];

        // Allocate and loop through array of primative groups
        mesh->primitive_groups.alloc(tg_mesh.primitives.size());
        for(u64 primative_group_index = 0; primative_group_index < mesh->primitive_groups.nitems; primative_group_index++){

            D_Primitive_Group* primative_group = mesh->primitive_groups.ptr + primative_group_index;
            tg::Primitive primitive = tg_mesh.primitives[primative_group_index];

            // Fill primative group

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
                primative_group->indicies.alloc(index_accessor.count);

                // copy over indicies
                for(u64 i = 0; i < index_accessor.count; i++){
                    // WARNING: u16* would need to change with different sizes / byte_strides of indicies
                    //primative_group->indicies.ptr[i] = ((u16*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                    primative_group->indicies.ptr[i] = ((u16*)(&buffer.data.at(0) + index_accessor.byteOffset))[i];
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
                        primative_group->verticies.alloc(accessor.count, sizeof(Vertex_Position_Normal_Tangent_Color_Texturecoord));
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

                    int byte_stride = accessor.ByteStride(buffer_view);
                    int size = 1;
                    if(accessor.type != TINYGLTF_TYPE_SCALAR){
                        size = accessor.type;
                    }
                    
                    if(attribute.first.compare("POSITION") == 0){
                        for(int i = 0; i < primative_group->verticies.nitems; i++){
                            primative_group->verticies.ptr[i].position = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                            primative_group->verticies.ptr[i].position.z = -primative_group->verticies.ptr[i].position.z;
                        }
                    } else if(attribute.first.compare("NORMAL") == 0){
                        for(int i = 0; i < primative_group->verticies.nitems; i++){
                            primative_group->verticies.ptr[i].normal = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                            primative_group->verticies.ptr[i].normal.z = -primative_group->verticies.ptr[i].normal.z;
                        }
                    } else if(attribute.first.compare("TANGENT") == 0){
                        for(int i = 0; i < primative_group->verticies.nitems; i++){
                            primative_group->verticies.ptr[i].tangent = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                            primative_group->verticies.ptr[i].tangent.z = -primative_group->verticies.ptr[i].tangent.z;
                        }
                    } else if(attribute.first.compare("TEXCOORD_0") == 0){
                        for(int i = 0; i < primative_group->verticies.nitems; i++){
                            primative_group->verticies.ptr[i].texture_coordinates = ((DirectX::XMFLOAT2*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                            // Dx12 UV is different than OpenGL / GLTF
                            //primative_group->verticies.ptr[i].texture_coordinates.y = 1 - primative_group->verticies.ptr[i].texture_coordinates.y;
                        }
                    } else if(attribute.first.compare("COLOR_0") == 0){
                        for(int i = 0; i < primative_group->verticies.nitems; i++){
                            primative_group->verticies.ptr[i].color = ((DirectX::XMFLOAT3*)(&buffer.data.at(0) + buffer_view.byteOffset))[i];
                        }
                    }

                    #if 0
                    char* out_buffer = (char*)calloc(500, sizeof(char));
                    sprintf(out_buffer, "attribute.first: %s size: %u, count: %u, accessor.componentType: %u, accessor.normalized: %u, byteStride: %u, byteOffset: %u\n", attribute.first.c_str(), size, buffer_view.byteLength / byte_stride, accessor.componentType, accessor.normalized, byte_stride, BUFFER_OFFSET(accessor.byteOffset));
                    OutputDebugString(out_buffer);
                    free(out_buffer);
                    #endif

                }
            }

            #if 0
            for (int vertex_index = 0; vertex_index < primative_group->verticies.nitems; vertex_index++) {
                DirectX::XMVECTOR tangent = DirectX::XMVectorZero();
                DirectX::XMVECTOR bitangent = DirectX::XMVectorZero();
                u32 triangles_included = 0;

                for (u32 index = 0; index < primative_group->indicies.nitems; index += 3) {

                    u32 index0 = primative_group->indicies.ptr[index];
                    u32 index1 = primative_group->indicies.ptr[index + 1];
                    u32 index2 = primative_group->indicies.ptr[index + 2];

                    // Only calc if a index matches our vertex
                    if (index0 == vertex_index || index1 == vertex_index || index2 == vertex_index) {
                        DirectX::XMVECTOR vertex0_pos = DirectX::XMLoadFloat3(&primative_group->verticies.ptr[index0].position);
                        DirectX::XMVECTOR vertex1_pos = DirectX::XMLoadFloat3(&primative_group->verticies.ptr[index1].position);
                        DirectX::XMVECTOR vertex2_pos = DirectX::XMLoadFloat3(&primative_group->verticies.ptr[index2].position);

                        DirectX::XMFLOAT2 vertex0_uv = primative_group->verticies.ptr[index0].texture_coordinates;
                        DirectX::XMFLOAT2 vertex1_uv = primative_group->verticies.ptr[index1].texture_coordinates;
                        DirectX::XMFLOAT2 vertex2_uv = primative_group->verticies.ptr[index2].texture_coordinates;

                        DirectX::XMVECTOR delta_pos0;
                        delta_pos0 = DirectX::XMVectorSubtract(vertex1_pos, vertex0_pos);

                        DirectX::XMVECTOR delta_pos1;
                        delta_pos1 = DirectX::XMVectorSubtract(vertex2_pos, vertex0_pos);

                        DirectX::XMFLOAT2 delta_uv0;
                        delta_uv0.x = vertex1_uv.x - vertex0_uv.x;
                        delta_uv0.y = vertex1_uv.y - vertex0_uv.y;

                        DirectX::XMFLOAT2 delta_uv1;
                        delta_uv1.x = vertex2_uv.x - vertex0_uv.x;
                        delta_uv1.y = vertex2_uv.y - vertex0_uv.y;

                        float f = 1.0 / (delta_uv0.x * delta_uv1.y - delta_uv0.y * delta_uv1.x);
                        tangent += (delta_uv1.y * delta_pos0 - delta_pos1 * delta_uv0.y) * f;
                        bitangent += (delta_pos1 * delta_uv0.x - delta_pos0 * delta_uv1.x) * f;
                        triangles_included += 1;
                    }
                }

                if (triangles_included > 0) {
                    tangent /= triangles_included;
                    bitangent /= triangles_included;
                    tangent = DirectX::XMVector3Normalize(tangent);
                    bitangent = DirectX::XMVector3Normalize(bitangent);
                }

                XMStoreFloat3(&primative_group->verticies.ptr[vertex_index].tangent, tangent);
                XMStoreFloat3(&primative_group->verticies.ptr[vertex_index].bitangent, bitangent);

            }
            #endif

            primative_group->material_index = primitive.material;
        }
    }
}

// Load every mesh in each node, then load the node's children nodes
void load_model_nodes(D_Model& d_model, tg::Model& tg_model, tg::Node& node){

    if(node.mesh > -1 && node.mesh < tg_model.meshes.size()){
        load_mesh(d_model, tg_model, tg_model.meshes[node.mesh]);
    }

    for(u64 i = 0; i < node.children.size(); i++){
        load_model_nodes(d_model, tg_model, tg_model.nodes[node.children[i]]);
    }

}

// Load the materials from tg_model to d_model
// Currently only supports baseColorTextures
void load_materials(D_Model& d_model, tg::Model& tg_model){

    // If there are textures available:
    if(tg_model.textures.size() > 0){

        // Allocate the correct number of materials
        d_model.materials.alloc(tg_model.materials.size());

        // Load each material from tg_model
        for(u16 j = 0; j < tg_model.materials.size(); j++){

            /////////////////////
            // Albedo Texture
            /////////////////////

            // Here, we are only storing, and using, the base color texture
            u32 texture_index = tg_model.materials[j].pbrMetallicRoughness.baseColorTexture.index;
            tg::Texture &tex = tg_model.textures[texture_index];

            // Get reference to corresponding material in d_model
            D_Material &material = d_model.materials.ptr[j];

            // If we have a baseColorTexture
            if(tex.source >= 0){

                // Load the image corresponding to the texture
                tg::Image &image = tg_model.images[tex.source];

                material.albedo_texture.texture_desc.width = image.width;
                material.albedo_texture.texture_desc.height = image.height;

                DXGI_FORMAT image_format = DXGI_FORMAT_UNKNOWN;
                
                // Store the image_format
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

                // Store a description of the texture
                material.albedo_texture.texture_desc.format     = image_format;
                material.albedo_texture.texture_desc.usage      = Texture::USAGE::USAGE_SAMPLED;
                material.albedo_texture.texture_desc.pixel_size = image.component * image.bits / 8;

                // Allocate enough room for the raw image data
                material.albedo_texture.cpu_texture_data.alloc(image.image.size());
                // Copy the raw image data over to d_model material j
                memcpy(material.albedo_texture.cpu_texture_data.ptr, &image.image.at(0), image.image.size());

                
            }

            /////////////////////
            // Normal Map
            /////////////////////

            // Here, we are only storing, and using, the normal texture
            texture_index = tg_model.materials[j].normalTexture.index;

            // If the texture intex is valid (isn't when we don't have a normal texture)
            if(texture_index != UINT_MAX){

                tex = tg_model.textures[texture_index];

                // Get reference to corresponding material in d_model
                material = d_model.materials.ptr[j];
                material.material_flags |= MATERIAL_FLAG_NORMAL_TEXTURE;

                // Load the image corresponding to the texture
                tg::Image &image = tg_model.images[tex.source];

                material.normal_texture.texture_desc.width = image.width;
                material.normal_texture.texture_desc.height = image.height;

                DXGI_FORMAT image_format = DXGI_FORMAT_UNKNOWN;
                
                // Store the image_format
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

                // Store a description of the texture
                material.normal_texture.texture_desc.format     = image_format;
                material.normal_texture.texture_desc.usage      = Texture::USAGE::USAGE_SAMPLED;
                material.normal_texture.texture_desc.pixel_size = image.component * image.bits / 8;

                // Allocate enough room for the raw image data
                material.normal_texture.cpu_texture_data.alloc(image.image.size());
                // Copy the raw image data over to d_model material j
                memcpy(material.normal_texture.cpu_texture_data.ptr, &image.image.at(0), image.image.size());
                        
            }
        }
    }
}

/*
    Input: Empty D_Model, filename of gltf file
    Output: D_Model with values from specified gltf file
*/
void load_gltf_model(D_Model& d_model, const char* filename){

    tg::Model tg_model;
    std::string err;
    std::string warn;

    bool ret = model_loader.LoadASCIIFromFile(&tg_model, &err, &warn, filename);

    if (!warn.empty()) {
        OutputDebugString(warn.c_str());
    }

    if (!err.empty()) {
        OutputDebugString(err.c_str());
        DEBUG_BREAK;
    }

    if (!ret) {
        OutputDebugString("Failed to parse glTF\n");
        DEBUG_BREAK;
        return;
    }

    const tg::Scene &scene = tg_model.scenes[tg_model.defaultScene];    

    // Load each node into the d_model structure
    for(u64 i = 0; i < scene.nodes.size(); i++){
        load_model_nodes(d_model, tg_model, tg_model.nodes[scene.nodes[i]]);
    }
    
    // Separetly load the materials, primative groups keep track of what material they use
    load_materials(d_model, tg_model);

}