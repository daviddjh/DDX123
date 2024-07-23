//////////////////////////////////
// COMMON DEFINES
//////////////////////////////////

#define CommonSpace space0
#define VertexSpace space1
#define PixelSpace  space2
#define ComputeSpace  space2
#define Tex2DSpace  space100
#define Tex2DUAVSpace  space101
#define Tex2DUintSpace  space102

#define MATERIAL_FLAG_NONE                     0x0
#define MATERIAL_FLAG_NORMAL_TEXTURE           0x1
#define MATERIAL_FLAG_ROUGHNESSMETALIC_TEXTURE 0x2

static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618379067154;

//////////////////////////////////
// COMMON Data Structures
//////////////////////////////////

#include "../code/constant_buffers.h"

//////////////////////////////////
// COMMON BINDINGS
//////////////////////////////////

// Our texture sampler and texture table
// TODO: More Samplers
# define GET_RESOURCE(index) ResourceDescriptorHeap[index]
SamplerState                   sampler_1              : register(s0, CommonSpace);
Texture2D                      texture_2d_table[]     : register(t0, Tex2DSpace);
Texture2D<uint>                texture_2d_uint_table[]: register(t0, Tex2DUintSpace);
RWTexture2D<float4>            texture_2d_uav_table[] : register(u0, Tex2DUAVSpace);
ConstantBuffer<Per_Frame_Data> per_frame_data         : register(b10, CommonSpace);
