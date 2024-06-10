#include "d_types.h"
#include "d_memory.h"
#include "d_helpers.h"
#include "d_string.h"

#ifndef _D_HASH
#define _D_HASH

namespace d_std {

    static inline u32 murmur3_scramble_32(u32 key_4_bytes){
        key_4_bytes *= 0xcc9e2d51;
        key_4_bytes = (key_4_bytes << 15) | (key_4_bytes >> 17);
        key_4_bytes *= 0x1b873593;
        return key_4_bytes;
    }

    u32 murmur3_32(void* key, u32 len, u32 seed = 0){
        u32 h = seed;
        u32 k;
        u8* key_u8 = (u8*)key;

        // For each 4 bytes in key_u8
        for(size_t i = len >> 2; i != 0; i--){
            // Copy 4 bytes to k
            memcpy(&k, key_u8, sizeof(u32));
            // Move key_u8 to next 4 bytes
            key_u8 += sizeof(u32);
            h ^= murmur3_scramble_32(k);
            h = (h << 13) | (h >> 19);
            h = h * 5 + 0xe6546b64;
        }

        // Deal with the remaining bytes
        k = 0;
        for(size_t i = len & 3; i != 0; i--){
            k <<= 8;
            k |= key_u8[i - 1];
        }

        h ^= murmur3_scramble_32(k);
        h ^= len;
        h ^= h >> 16;
        h *= 0x85ebca6b;
        h ^= h >> 13;
        h *= 0xc2b2ae35;
        h ^= h >> 16;
        return h;
    }

    template<typename Value, u32 size>
    struct Static_String_Hash_Table {

        Value value_array[size] = {0};
        const u32 value_array_size = size;

        Value get_value(d_std::d_string key);
        void add_value(d_std::d_string key, Value value);
        void count(d_std::d_string key);
    };

    template<typename Value, u32 size>
    Value Static_String_Hash_Table<Value, size>::get_value(d_std::d_string key){
        u32 value_index_32 = murmur3_32(key.string, key.size);
        u32 value_index = value_index_32 % this->value_array_size;

        ASSERT(value_index < size);
        return value_array[value_index];
    }

    template<typename Value, u32 size>
    void Static_String_Hash_Table<Value, size>::add_value(d_std::d_string key, Value value){
        u32 value_index_32 = murmur3_32(key.string, key.size);
        u32 value_index = value_index_32 % this->value_array_size;

        ASSERT(value_index < size);
        value_array[value_index] = value;
        return;
    }

    template<typename Value, u32 size>
    void Static_String_Hash_Table<Value, size>::count(d_std::d_string key){
        ASSERT("Not Finished");
        return;
    }

}

#endif // _D_HASH