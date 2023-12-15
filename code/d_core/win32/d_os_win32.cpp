#ifndef _D_OS_WIN32
#define _D_OS_WIN32

#include "../d_os.h"
#include "../d_string.h"
#include "../d_memory.h"

// Windows.h
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "windows.h"

namespace d_std {

    u_ptr
    os_reserve_memory (u64 size){

        u_ptr  memory = (u_ptr)VirtualAlloc(nullptr, size, MEM_RESERVE, PAGE_READWRITE);
        return memory;

    }

    void
    os_commit_memory (u_ptr memory, u64 size){

        VirtualAlloc((LPVOID)memory, size, MEM_COMMIT, PAGE_READWRITE);
        return;

    }

    void
    os_decommit_memory(u_ptr memory, u64 size){

        VirtualFree((LPVOID)memory, size, MEM_DECOMMIT);

    }

    void
    os_release_memory (u_ptr memory){

        VirtualFree((LPVOID)memory, 0, MEM_RELEASE);

    }

    void 
    os_debug_print(const char * string)
    {
        OutputDebugStringA(string);
    }
    
    void 
    os_debug_print(d_string string)
    {

        char c_str[501]; 

        if(string.size > 500){

            OutputDebugStringA("(os_debug_print) Error: string size is more than 500 characters. Printing first 500.\n");

            for(int i = 0; i < 500; i++){
                c_str[i] = string.string[i];
            }

            c_str[500] = '\0';

            OutputDebugStringA(c_str);
            

        } else {

            for(int i = 0; i < string.size; i++){
                c_str[i] = string.string[i];
            }

            c_str[string.size] = '\0';

            OutputDebugStringA(c_str);

        }

        return;

    }

    void __cdecl os_debug_printf(Memory_Arena *arena, char* lit_string, ...){

        // va_list
        u_ptr va_args;

        // va_start
        va_args = (u_ptr)(&lit_string + 1);

        d_string string_to_print = _format_lit_string(arena, lit_string, va_args);

        os_debug_print(string_to_print);

        return;

    }

}

#endif // _D_OS_WIN32