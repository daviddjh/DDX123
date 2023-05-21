#ifndef _D_TYPES
#define _D_TYPES

#include "d_context.h" // OS_WINDOWS_64
#include "limits.h"

typedef           __int8  s8;
typedef  unsigned __int8  u8;
typedef           __int16 s16;
typedef  unsigned __int16 u16;
typedef           __int32 s32;
typedef  unsigned __int32 u32;
typedef           __int64 s64;
typedef  unsigned __int64 u64;
#ifdef OS_WINDOWS_64
typedef  unsigned __int64 u_ptr;
#else
typedef  unsigned __int32 u_ptr;
#endif

typedef float  f32;
typedef double f64;

#define _1KB   1 * 1024
#define _2KB   2 * 1024
#define _4KB   4 * 1024
#define _8KB   8 * 1024
#define _16KB 16 * 1024
#define _32KB 32 * 1024
#define _64KB 64 * 1024

#define _1MB   1 * 1024 * 1024
#define _2MB   2 * 1024 * 1024
#define _4MB   4 * 1024 * 1024
#define _8MB   8 * 1024 * 1024
#define _16MB 16 * 1024 * 1024
#define _32MB 32 * 1024 * 1024
#define _64MB 64 * 1024 * 1024

#define _1GB   1 * 1024 * 1024 * 1024
#define _2GB   2 * 1024 * 1024 * 1024
#define _4GB   4 * 1024 * 1024 * 1024
#define _8GB   8 * 1024 * 1024 * 1024
#define _16GB 16 * 1024 * 1024 * 1024
#define _32GB 32 * 1024 * 1024 * 1024
#define _64GB 64 * 1024 * 1024 * 1024

#define KB(x) x     * 1024LL 
#define MB(x) KB(x) * 1024LL 
#define GB(x) MB(x) * 1024LL 

#endif // _D_TYPES