#ifndef _D_STRING
#define _D_STRING

#include "d_types.h"

namespace d_std {

    struct Memory_Arena;

    struct d_string {
        char* string;
        u32   size;

        const char* c_str(Memory_Arena* arena);
    };

    d_string string_from_lit_string(Memory_Arena *arena, char* lit_string);
    d_string format_lit_string(Memory_Arena *arena, char* lit_string, ...);
    d_string _format_lit_string(Memory_Arena *arena, char* lit_string, u_ptr va_args);
    void _format_lit_string(char* return_string, u16 return_string_max_size, char* lit_string, u_ptr va_args);

}

#endif // _D_STRING