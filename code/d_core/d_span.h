#pragma once

namespace d_std {

    template <typename T>
    struct Span {
        T* ptr = nullptr;
        size_t nitems = 0;

        void alloc(size_t nitems, size_t size); 
        void alloc(size_t nitems); 
        void d_free();
    };

    template <typename T>
    inline void Span<T>::alloc(size_t nitems){

        if(ptr != nullptr){
            free(ptr);
            ptr = nullptr;
        }

       this->nitems = nitems; 
       ptr = static_cast<T*>(calloc(nitems, sizeof(T)));
    }

    template <typename T>
    inline void Span<T>::alloc(size_t nitems, size_t size){

        if(ptr != nullptr){
            free(ptr);
            ptr = nullptr;
        }

       this->nitems = nitems; 
       ptr = static_cast<T*>(calloc(nitems, size));
    }

    template <typename T>
    inline void Span<T>::d_free(){
        if(ptr != nullptr){
            free(ptr);
            ptr = nullptr;
        }
    }

};