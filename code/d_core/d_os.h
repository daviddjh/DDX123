#ifndef _D_OS
#define _D_OS

#include "d_types.h"
#include "d_string.h"

#define d_reserve(s)     os_reserve_memory(s)
#define d_commit(m, s)   os_commit_memory(m, s)
#define d_decommit(m, s) os_decommit_memory(m, s)
#define d_release(s)     os_release_memory(s)

namespace d_std {

    // Memory
    u_ptr os_reserve_memory (u64 size);
    void  os_commit_memory  (u_ptr memory, u64 size);
    void  os_decommit_memory(u_ptr memory, u64 size);
    void  os_release_memory (u_ptr memory);

    // Print
    void  os_debug_print (const char*);
    void  os_debug_printf(Memory_Arena *arena, char* lit_string, ...);
    void  os_debug_printf(char* lit_string, ...);
    void  os_debug_print (d_string);

}

#endif // _D_OS