#include "d_memory.h"
#include "d_helpers.h"

namespace d_std {

    Memory_Arena* make_arena(u64 reserve_size) {
        Memory_Arena* arena = nullptr;

        if(reserve_size > sizeof(Memory_Arena)){
            u_ptr memory = d_reserve(reserve_size);
            d_commit(memory, sizeof(Memory_Arena));
            arena = (Memory_Arena*) memory;

            arena->capacity = reserve_size;
            arena->position = sizeof(Memory_Arena);
            arena->commit_position = sizeof(Memory_Arena);
        }

        return arena;
    }

    Memory_Arena* make_arena() {
        Memory_Arena* arena = make_arena(DEFAULT_ARENA_RESERVE_SIZE);
        return arena;
    }

    void Memory_Arena::release() {
        d_release((u_ptr)this);
        return;
    }

    void Memory_Arena::reset() {

        switch(this->type){
            case(Memory_Arena_Type_Linear):
                this->pop_all();
                break;
        }

        return;
    }

    u_ptr Memory_Arena::allocate(u64 size) {

        u_ptr result = 0;
        switch(this->type){
            case(Memory_Arena_Type_Linear):
                result = this->push(size);
                break;
        }

        return result;
    }

    void Memory_Arena::deallocate(u_ptr address) {

        switch(this->type){
            case(Memory_Arena_Type_Linear):
                this->pop_to(address - (u_ptr)this);
                break;
        }

        return;
    }

    u_ptr Memory_Arena::push(u64 size) {

        u_ptr memory = 0;
        if(size + this->position < capacity){
            if(size + this->position <= commit_position){
                memory = (u_ptr)this + this->position;
                this->position += size;
            } else {

                memory = (u_ptr)this + this->position;
                this->position += size;

                u64 aligned_position = AlignPow2Up(this->position, DEFAULT_ARENA_COMMIT_SIZE);
                u64 next_commit_position = d_min(aligned_position, this->capacity);
                u64 commit_size = next_commit_position - this->commit_position;

                d_commit((u_ptr)this + this->commit_position, commit_size);
                this->commit_position += commit_size;

            }
        }

        return memory;
    }

    void Memory_Arena::pop_to(u64 position){

        if(position < this->position){

            this->position = position;

            u64 aligned_position = AlignPow2Up(this->position, DEFAULT_ARENA_COMMIT_SIZE);
            u64 next_commit_position = d_min(aligned_position, this->capacity);

            if(next_commit_position < this->commit_position){

                u64 decommission_size = this->commit_position - next_commit_position;
                d_decommit((u_ptr)(this + next_commit_position), decommission_size);
                this->commit_position = next_commit_position;

            }

        }

    }

    void Memory_Arena::pop_all(){

        // Clear the arena, pop to the begining ( which is &arena + sizeof(Arena) )
        pop_to(sizeof(Memory_Arena));

    }

}

