#include "d_string.h"
#include "d_memory.h"

#define DSTR(arena, cstr) d_std::string_from_lit_string(arena, cstr)

namespace d_std {

    const char* d_string::c_str(Memory_Arena *arena){
        char* c_str = arena->allocate_array<char>(size + 1);

        for(int i = 0; i < size; i++){
            c_str[i] = string[i];
        }
        c_str[size] = '\0';

        return c_str;
    }

    d_string string_from_lit_string(Memory_Arena *arena, char* lit_string){

        d_string string;
        u32 size = 0;
        char* p = const_cast<char*>(lit_string);
        while (*p != '\0'){
            size++;
            p++;
        }
        string.size = size;
        string.string = arena->allocate_array<char>(string.size);

        for(int i = 0; i < string.size; i++){
            string.string[i] = lit_string[i];
        }
        string.string[string.size] = '\0';

        return string;
    }

    d_string __cdecl format_lit_string(Memory_Arena *arena, char* lit_string, ...){

        // va_list
        u_ptr va_args;

        // va_start
        va_args = (u_ptr)(&lit_string + 1);

        return _format_lit_string(arena, lit_string, va_args);

    }

    //////////////////////////////////////////////////////
    // Assumes we can change all the bytes in max size
    // Doesn't account for terminating char
    //////////////////////////////////////////////////////
    void __cdecl _format_lit_string(char* return_string, u16 return_string_max_size, const char* lit_string, u_ptr va_args){

        // it iterates through the original string
        const char* it = lit_string;

        u16 return_string_size = 0;
        u16 capacity = return_string_max_size;
        bool size_limit_reached = false;

        while( *it != '\0' && size_limit_reached == false){

            while( *it != '%' && *it != '\0'){
                return_string[return_string_size] = *it;
                return_string_size++;
                it++;
                if(return_string_size == capacity){
                    size_limit_reached = true;
                    break;
                }
            }
            if ( *it == '%' ) {
                if ( *(it+1) != '\0' ){
                    
                    // Switch based on argument type
                    switch(*(it+1)){

                        // Unsigned int
                        case('u'):
                        {
                            u64 number = *(u64*)va_args;
                            va_args += sizeof(u64);

                            char* placement_ptr = return_string + return_string_size - 1;

                            // Guarantee one spot. What if "number" is 0?
                            u64 temp = number; 
                            placement_ptr++;
                            return_string_size++;
                            if(return_string_size == capacity){
                                size_limit_reached = true;
                                break;
                            }
                            temp /= 10;

                            // Find the reset of the space this number will take up in the string
                            for(temp; temp>0; temp /= 10){
                                placement_ptr++;
                                return_string_size++;
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
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

                        // C string
                        case('s'):
                        {
                            char* c_str = *(char**)va_args;
                            va_args  += sizeof(u64);

                            char* placement_ptr = return_string + return_string_size;

                            char* input_string_iterator = (char*)c_str;

                            while(*input_string_iterator != '\0'){

                                // Set char in output string 
                                *placement_ptr = *input_string_iterator;

                                // Advance Iterators
                                input_string_iterator++;
                                placement_ptr++;
                                return_string_size++;

                                // Possibly allocate more memory
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
                                }
                            }

                            it++;
                        }
                        break;

                        // d_std::d_string
                        case('$'):
                        {
                            // Structs are added to the stack as the address of the stuct, and we have the address of that address
                            d_string d_str = **(d_string**)va_args;
                            va_args  += sizeof(u64);

                            char* placement_ptr = return_string + return_string_size;

                            //char* input_string_iterator = (char*)d_str.string;

                            for(int i = 0; i < d_str.size; i++){

                                // Set char in output string 
                                *placement_ptr = d_str.string[i];

                                // Advance Iterators
                                placement_ptr++;
                                return_string_size++;

                                // Possibly allocate more memory
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
                                }
                            }

                            it++;
                        }
                        break;

                        // d_std::d_string
                        case('f'):
                        {
                            // Structs are added to the stack as the address of the stuct, and we have the address of that address
                            double double_input = *(f64*)va_args;
                            va_args  += sizeof(u64);

                            if(return_string_size == capacity){
                                size_limit_reached = true;
                                break;
                            }

                            char* placement_ptr = return_string + return_string_size;

                            // Check and possibly add sign bit
                            if(double_input < 0){

                                // Add negative char in output string
                                *placement_ptr = '-';

                                // Advance Iterators
                                placement_ptr++;
                                return_string_size++;

                                // Possibly allocate more memory
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
                                }

                                double_input *= -1;

                            }

                            u64 integer_part = (u64)double_input;

                            // Guarantee one spot. What if "number" is 0?
                            u64 temp = integer_part; 
                            return_string_size++;
                            // Possibly allocate more memory
                            if(return_string_size == capacity){
                                size_limit_reached = true;
                                break;
                            }
                            temp /= 10;

                            // Find the reset of the space this number will take up in the string
                            for(temp; temp>0; temp /= 10){
                                placement_ptr++;
                                return_string_size++;
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
                                }
                            }

                            // Insert the numbers from smallest (right) to largest (left)
                            temp = integer_part; 
                            *placement_ptr = (temp % 10) + '0';
                            temp /= 10;
                            placement_ptr--;
                            for(temp; temp>0; temp /= 10, placement_ptr--){
                                *placement_ptr = (temp % 10) + '0';
                            }

                            placement_ptr     = return_string + return_string_size;

                            // Insert decimal place
                            *placement_ptr = '.';

                            // Advance Iterators
                            placement_ptr++;
                            return_string_size++;

                            // Possibly allocate more memory
                            if(return_string_size == capacity){
                                size_limit_reached = true;
                                break;
                            }

                            u64 decimal_part = (((double_input + 0.00001 )- (float)integer_part) * 1000.);

                            // Guarantee one spot. What if "number" is 0?
                            temp = decimal_part; 
                            return_string_size++;
                            temp /= 10;

                            // Find the reset of the space this number will take up in the string
                            for(temp; temp>0; temp /= 10){
                                placement_ptr++;
                                return_string_size++;
                                if(return_string_size == capacity){
                                    size_limit_reached = true;
                                    break;
                                }
                            }

                            // Insert the numbers from smallest (right) to largest (left)
                            temp = decimal_part; 
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
                if(return_string_size == capacity){
                    size_limit_reached = true;
                    break;
                }
            }

        }

        return;

    }

    d_string __cdecl _format_lit_string(Memory_Arena *arena, char* lit_string, u_ptr va_args){

        // it iterates through the original string
        char* it = lit_string;

        u32 capacity = 50;
        d_string return_string;

        // Current place in new string is kept track of with size
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
                    
                    // Switch based on argument type
                    switch(*(it+1)){

                        // Unsigned int
                        case('u'):
                        {
                            u64 number = *(u64*)va_args;
                            va_args += sizeof(u64);

                            char* placement_ptr = return_string.string + return_string.size - 1;

                            // Guarantee one spot. What if "number" is 0?
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

                        // C string
                        case('s'):
                        {
                            char* c_str = *(char**)va_args;
                            va_args  += sizeof(u64);

                            char* placement_ptr = return_string.string + return_string.size;

                            char* input_string_iterator = (char*)c_str;

                            while(*input_string_iterator != '\0'){

                                // Set char in output string 
                                *placement_ptr = *input_string_iterator;

                                // Advance Iterators
                                input_string_iterator++;
                                placement_ptr++;
                                return_string.size++;

                                // Possibly allocate more memory
                                if(return_string.size == capacity){
                                    arena->allocate_array<char>(return_string.size);
                                    capacity += return_string.size;
                                }
                            }

                            it++;
                        }
                        break;

                        // d_std::d_string
                        case('$'):
                        {
                            // Structs are added to the stack as the address of the stuct, and we have the address of that address
                            d_string d_str = **(d_string**)va_args;
                            va_args  += sizeof(u64);

                            char* placement_ptr = return_string.string + return_string.size;

                            //char* input_string_iterator = (char*)d_str.string;

                            for(int i = 0; i < d_str.size; i++){

                                // Set char in output string 
                                *placement_ptr = d_str.string[i];

                                // Advance Iterators
                                placement_ptr++;
                                return_string.size++;

                                // Possibly allocate more memory
                                if(return_string.size == capacity){
                                    arena->allocate_array<char>(return_string.size);
                                    capacity += return_string.size;
                                }
                            }

                            it++;
                        }
                        break;

                        // d_std::d_string
                        case('f'):
                        {
                            // Structs are added to the stack as the address of the stuct, and we have the address of that address
                            double double_input = *(f64*)va_args;
                            va_args  += sizeof(u64);

                            if(return_string.size == capacity){
                                arena->allocate_array<char>(return_string.size);
                                capacity += return_string.size;
                            }

                            char* placement_ptr     = return_string.string + return_string.size;

                            // Check and possibly add sign bit
                            if(double_input < 0){

                                // Add negative char in output string
                                *placement_ptr = '-';

                                // Advance Iterators
                                placement_ptr++;
                                return_string.size++;

                                // Possibly allocate more memory
                                if(return_string.size == capacity){
                                    arena->allocate_array<char>(return_string.size);
                                    capacity += return_string.size;
                                }

                                double_input *= -1;

                            }

                            u64 integer_part = (u64)double_input;

                            // Guarantee one spot. What if "number" is 0?
                            u64 temp = integer_part; 
                            return_string.size++;
                            // Possibly allocate more memory
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
                            temp = integer_part; 
                            *placement_ptr = (temp % 10) + '0';
                            temp /= 10;
                            placement_ptr--;
                            for(temp; temp>0; temp /= 10, placement_ptr--){
                                *placement_ptr = (temp % 10) + '0';
                            }

                            placement_ptr     = return_string.string + return_string.size;

                            // Insert decimal place
                            *placement_ptr = '.';

                            // Advance Iterators
                            placement_ptr++;
                            return_string.size++;

                            // Possibly allocate more memory
                            if(return_string.size == capacity){
                                arena->allocate_array<char>(return_string.size);
                                capacity += return_string.size;
                            }

                            u64 decimal_part = (((double_input + 0.00001 )- (float)integer_part) * 1000.);

                            // Guarantee one spot. What if "number" is 0?
                            temp = decimal_part; 
                            return_string.size++;
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
                            temp = decimal_part; 
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