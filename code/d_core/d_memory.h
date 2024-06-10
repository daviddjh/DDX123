#ifndef _D_MEMORY
#define _D_MEMORY

#include "d_types.h"
#include "d_os.h"
#include "string.h" // For memcpy

#ifdef DEBUG
#include "crtdbg.h"
#else
#include "crtdefs.h"
#endif

// Would need to import d_os.h first to get os_*_memory definitions. Is that needed?

#ifndef d_reserve
#error "d_reserve not defined!"
#include "stdlib.h"
#define d_reserve(s) malloc(s)
#endif

#ifndef d_commit
#define d_commit(...)
#endif

#ifndef d_decommit
#define d_decommit(...)
#endif

#ifndef d_release
#include "stdlib.h"
#define d_release(s) free(s)
#endif

#define DEFAULT_ARENA_RESERVE_SIZE _64MB * 10
#define DEFAULT_ARENA_COMMIT_SIZE  _64MB

namespace d_std {

    /*
        Because the Memory_Arena struct is stored at the beginning of the arena,
        the Memory_Arena needs to be passed around by pointer. The Memory_Arena itself
        doesn't contain a pointer to the arena, so cannot be passed by value to a new stack
        variable. This would result in messing with memory on the stack.
    */
    /*
        Want more than one type of memory arena / allocator.
        Ex: Linear, Heap, Genaral Free list, etc..
        May need to make this struct a "general" type arena, with a "type" value.
        The general "allocate" and "Deallocate" functions will key off of this "type" value
        to choose the correct allocation and deallocation functions.

        This is a trade off compared to Inheritance and Virtual function tables
    */
    enum Memory_Arena_Type {
        Memory_Arena_Type_Linear,
        Memory_Arena_Type_Count
    };

    struct Memory_Arena {

        u64 capacity;
        u64 position;
        u64 commit_position;
        Memory_Arena_Type type = Memory_Arena_Type_Linear;

        //////////////////////////////////////////////////////
        // General functions for all kinds of allocators
        //////////////////////////////////////////////////////

        // Return committed memory to OS
        void  release();
        // De allocate every allocation in the arena
        void  reset();
        u_ptr allocate(u64 size);
        void  deallocate(u_ptr address);

        //////////////////////////////////////////////////////
        // Memory Arena Type Linear
        //////////////////////////////////////////////////////

        u_ptr push(u64 size);
        void  pop_to(u64 position);
        void  pop_all();

        template<typename t>
        inline t* allocate_array(u64 nitems){

            return (t*)allocate(nitems * sizeof(t));

        }

        template<typename t>
        inline void deallocate_array(t* array){


            switch (this->type)
            {
            case Memory_Arena_Type_Linear:

                this->deallocate((u_ptr)array);
                break;
            
            }

        }

    };

    Memory_Arena* make_arena();
    Memory_Arena* make_arena(u64 reserve_size);

}



#endif // _D_MEMORY