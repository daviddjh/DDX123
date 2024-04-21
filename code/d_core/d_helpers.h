#ifndef _D_HELPERS
#define _D_HELPERS

#include "d_context.h"

#ifdef OS_WINDOWS
#ifndef PrintD
#define PrintD(s) OutputDebugString(s)
#endif // Print(s)
#endif // OS_WINDOWS

#define AlignPow2To0 (x, p) ((x + ((x >> sizeof(x)-1) & p - 1)) & -p)
#define AlignPow2Down(x, p) (x & -p) 
#define AlignPow2Up(x, p) ((x + p - 1) & -p) 

#define d_min(a, b) (a < b ? a : b)
#define d_max(a, b) (a > b ? a : b)

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))  // Doesn't work for pointers, must be array type 

#if COMPILER_MSVC 
#define abs_val_int32(x) { int y = x >> 31; x = (x ^ y) - y; }
#endif // COMPILER_MSVC 

#define stringerize(x) _stringerize(x)
#define _stringerize(x) #x

#ifndef FORCE_INLINE
    #if COMPILER_MSVC
        #define FORCE_INLINE __forceinline
    #endif // COMPILER_MSVC

    #ifndef FORCE_INLINE
        #define FORCE_INLINE
    #endif // FORCE_INLINE
#endif // FORCE_INLINE

#endif // _D_HELPERS