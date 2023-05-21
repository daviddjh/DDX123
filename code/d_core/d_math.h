namespace d_std {
        __forceinline size_t d_round_up(size_t size, size_t alignment){

                size_t mask = alignment - 1;
                return (size + mask) & (~mask);

        }
}