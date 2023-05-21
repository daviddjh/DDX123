#include "d_string.h"
#include "d_memory.h"

namespace d_std {

    d_string string_from_lit_string(Memory_Arena *arena, char* lit_string){

        d_string string;
        u32 size = 0;
        char* p = lit_string;
        while (*p != '\0'){
            size++;
            p++;
        }
        string.size = size;
        string.string = arena->allocate_array<char>(string.size);

        for(int i = 0; i < string.size; i++){
            string.string[i] = lit_string[i];
        }

        return string;
    }

    d_string format_lit_string(Memory_Arena *arena, char* lit_string, ...){

        // va_list
        u_ptr va_args;

        // va_start
        va_args = (u_ptr)(&lit_string + 1);

        return _format_lit_string(arena, lit_string, va_args);

    }

    d_string _format_lit_string(Memory_Arena *arena, char* lit_string, u_ptr va_args){

        char* it = lit_string;

        u32 capacity = 50;
        d_string return_string;
        return_string.string = arena->allocate_array<char>(capacity);
        return_string.size = 0;


        while( *it != '\0'){

            while( *it != '%' && *it != '\0'){
                return_string.string[return_string.size] = *it;
                return_string.size++;
                it++;
                if(return_string.size == capacity){
                    arena->allocate_array<char>(return_string.size);
                    capacity += return_string.size;
                }
            }

            if ( *it == '%' ) {
                if ( *(it+1) != '\0' ){
                    switch(*(it+1)){
                        case('u'):
                        {
                            u64 number = *(u64*)va_args;
                            va_args += sizeof(u64);

                            char* placement_ptr = return_string.string + return_string.size - 1;

                            // Guarantee one spot. What is "number" is 0?
                            u64 temp = number; 
                            placement_ptr++;
                            return_string.size++;
                            if(return_string.size == capacity){
                                arena->allocate_array<char>(return_string.size);
                                capacity += return_string.size;
                            }
                            temp /= 10;

                            // Find the reset of the space this number will take up in the string
                            for(temp; temp>0; temp /= 10){
                                placement_ptr++;
                                return_string.size++;
                                if(return_string.size == capacity){
                                    arena->allocate_array<char>(return_string.size);
                                    capacity += return_string.size;
                                }
                            }

                            // Insert the numbers from smallest (right) to largest (left)
                            temp = number; 
                            *placement_ptr = (temp % 10) + '0';
                            temp /= 10;
                            placement_ptr--;
                            for(temp; temp>0; temp /= 10, placement_ptr--){
                                *placement_ptr = (temp % 10) + '0';
                            }

                            it++;
                        }
                        break;
                    }
                }
                it++;
                if(return_string.size == capacity){
                    arena->allocate_array<char>(return_string.size);
                    capacity += return_string.size;
                }
            }

        }

        // Release memory from 
        if(return_string.size < capacity){
            arena->deallocate_array(return_string.string + return_string.size);
        }

        return return_string;

    }
}