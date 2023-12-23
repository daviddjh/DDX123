//////////////////////////////////
// COMMON DEFINES
//////////////////////////////////

#define CommonSpace space0
#define VertexSpace space1
#define PixelSpace  space2
#define ComputeSpace  space2
#define Tex2DSpace  space100

#define MATERIAL_FLAG_NONE                     0x0
#define MATERIAL_FLAG_NORMAL_TEXTURE           0x1
#define MATERIAL_FLAG_ROUGHNESSMETALIC_TEXTURE 0x2

static const float PI = 3.14159265359;

//////////////////////////////////
// COMMON Data Structures
//////////////////////////////////

#include "../code/constant_buffers.h"

//////////////////////////////////
// COMMON BINDINGS
//////////////////////////////////

// Our texture sampler and texture table
// TODO: More Samplers
SamplerState                   sampler_1          : register(s0, CommonSpace);
Texture2D                      texture_2d_table[] : register(t0, Tex2DSpace);
ConstantBuffer<Per_Frame_Data> per_frame_data     : register(b0, CommonSpace);
