#ifndef D_ARRAY
#define D_ARRAY

#include "d_types.h"
#include "d_memory.h"

namespace d_std {

    template<typename T>
    struct d_array {
        T* ptr     = 0;
        //u64 size     = 0;
        u64 capacity = 0;

        // Keep reference to arena this array is allocated in
        Memory_Arena *arena = 0;

        // Allocate memory for array. Initialize capacity.
        void make_array(Memory_Arena *arena, u64 nitems);

        // Reset size of array to 0. Keep memory.
        void reset();

        // Release memory of array back to arena.
        void release();

        #if 0
        // Push Value onto end of array
        void push_back(T item);
        
        // Pop item off the end of the array. Return the item.
        T pop_off();
        #endif

        // Set item in array
        void set(u64 index, T item);

        // Set item in array
        T get(u64 index);

        // Uses [] operator to act like a std::vector
        T& operator[] (u64 i);

        T operator[] (u64 i) const;
    };

    // Allocate memory for array. Initialize capacity.
    template <typename T> 
    void d_array<T>::make_array(Memory_Arena *arena, u64 nitems){

        this->ptr = arena->allocate_array<T>(nitems);
        if(this->ptr){
            capacity = nitems;
            //size = 0;
            this->arena = arena;
        }

    }

    // Reset size of array to 0. Keep memory.
    template <typename T> 
    void d_array<T>::reset(){
        //size = 0;
    }

    // Release memory of array back to arena.
    template <typename T> 
    void d_array<T>::release(){
        this->arena->deallocate_array<T>(this->ptr);
        this->arena = 0;
        this->capacity = 0;
        //this->size = 0;
    }

    #if 0
    // Push Value onto end of array
    template <typename T> 
    void d_array<T>::push_back(T item){

        #ifdef _DEBUG
        if(size >= capacity){
            os_debug_print("(d_array::push_back) Warning: push_back called past end of the allocated array!\n");
        }
        #endif

        *(this->ptr + size) = item;
        size++;
        return;
    }
    
    // Pop item off the end of the array. Return the item.
    template <typename T> 
    T d_array<T>::pop_off(){
        T item = *(this->array + size);
        *(this->array + size) = 0;
        size--;
        return item;
    }
    #endif

    // Set item in array
    template <typename T> 
    void d_array<T>::set(u64 index, T item){

        #ifdef _DEBUG
        if(index >= capacity){
            os_debug_print("(d_array::set) Warning: set called past end of the allocated array!\n");
        }
        #endif

        *(this->ptr + index) = item;

        return;
    }

    // Set item in array
    template <typename T> 
    T d_array<T>::get(u64 index){

        #ifdef _DEBUG
        if(index >= capacity){
            os_debug_print("(d_array::get) Warning: get called past end of the allocated array!\n");
        }
        #endif

        return *(this->ptr + index);
    }

    // Uses [] operator to act like a std::vector, for non-const values, return a reference, so my_array[3] = 5 works
    template <typename T> 
    T& d_array<T>::operator[] (u64 index){

        #ifdef _DEBUG
        if(index >= capacity){
            os_debug_print("(d_array::[]) Warning: [] called past end of the allocated array!\n");
        }
        #endif

        return *(ptr + index);
    }

    // Uses [] operator to act like a std::vector, for const values, return a value.
    template <typename T> 
    T d_array<T>::operator[] (u64 index) const {

        #ifdef _DEBUG
        if(index >= capacity){
            os_debug_print("(d_array::[]) Warning: [] called past end of the allocated array!\n");
        }
        #endif

        return *(ptr + index);
    }

};

#endif // D_ARRAY