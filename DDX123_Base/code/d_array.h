// #include "d_mem.h"
#define DEFAULT_CAPACITY 32

template <typename T>
struct Array {

    T*      ptr;
    size_t size;
    size_t capacity;

    Array();
    Array(size_t size);
    Array(const T t[]);
    T& operator[](size_t);

    void resize(size_t size);
    void push_back(T);
    T    pop();
    void remove(size_t size);

};