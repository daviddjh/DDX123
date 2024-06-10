// cl.exe /Zi .\main.cpp ..\d_memory.cpp ..\win32\d_os_win32.cpp

#include "../d_core.cpp"

unsigned long __stdcall main(){

    // Helpers
    u64 a;
    a = AlignPow2Up(7, 8);
    a = AlignPow2Up(9, 8);
    a = AlignPow2Down(7, 8);
    a = AlignPow2Down(9, 8);
    a = d_max(7, 8);
    a = d_min(7, 8);
    
    // Memory
    d_std::Memory_Arena* arena = d_std::make_arena();
    u8* memory = (u8*)arena->allocate(128);
    memory[0] = 1;
    memory[2] = 1;
    memory[4] = 1;
    memory[127] = 1;

    u8* memory2 = (u8*)arena->allocate(MB(64));

    memory2[0] = 1;
    memory2[2] = 1;
    memory2[4] = 1;
    memory2[MB(64) - 1] = 1;

    //arena->arena_pop_to(memory2 - (u8*)arena);
    arena->deallocate((u_ptr)memory2);

    memory2 = (u8*)arena->allocate(MB(64));

    memory2[0] = 1;
    memory2[2] = 1;
    memory2[4] = 1;
    memory2[MB(64) - 1] = 1;

    arena->deallocate((u_ptr)memory);

    arena->release();

    struct Person{
        u8 age;
        u8 height;
        u8 weight;
    };

    arena = d_std::make_arena(MB(512));

    Person* people = arena->allocate_array<Person>(100);

    for(int i = 0; i < 100; i++){
        people[i].age    = i;
        people[i].weight = i + i;
        people[i].height = i * 2;
    }

    arena->deallocate_array(people);

    people = arena->allocate_array<Person>(100);

    for(int i = 0; i < 100; i++){
        people[i].age    = i;
        people[i].weight = i + i;
        people[i].height = i * 2;
    }

    Person* other_people = arena->allocate_array<Person>(100);

    for(int i = 0; i < 100; i++){
        other_people[i].age    = i;
        other_people[i].weight = i + i;
        other_people[i].height = i * 2;
    }

    arena->reset();

    u32 pos = 2;
    abs_val_int32(pos);

    u32 neg = -2;
    abs_val_int32(neg);

    d_std::os_debug_print("Hey!\n");

    d_std::d_string my_string = d_std::string_from_lit_string(arena, "Hello World!!\n");
    d_std::os_debug_print(my_string);

    d_std::os_debug_print(d_std::string_from_lit_string(arena, "Bye Now!\n"));
    d_std::os_debug_print(d_std::string_from_lit_string(arena, "Long string! \n"
    "haha                                                                    \n"
    "this                                                                    \n"
    "string                                                                    \n"
    "is                                                                      \n"
    "very                                                                     \n"
    "very                                                                    \n"
    "long                                                                    \n"
    "haha                                                                    \n"
    "this                                                                    \n"
    "string                                                                    \n"
    "is                                                                      \n"
    "very                                                                     \n"
    "very                                                                    \n"
    "long                                                                    \n"
    "haha                                                                    \n"
    "this                                                                    \n"
    "string                                                                    \n"
    "is                                                                      \n"
    "very                                                                     \n"
    "very                                                                    \n"
    "long                                                                    \n"
    ));

    d_std::os_debug_print("\n");

    d_std::os_debug_print(d_std::format_lit_string(arena, "My Numbers: %u, %u\n", 2, 1152));

    d_std::os_debug_printf("My Other Numbers: %u, %u\n", 55, 100);
    
    d_std::os_debug_printf(arena, "My float: %f\n", -100.200);

    d_std::os_debug_printf("My second and third floats: %f, %f\n", -15.1, 1003.2);

    d_std::os_debug_printf(arena, "My string: %s\n", "I'm Yours");
    
    d_std::os_debug_printf(arena, "My string other string: %s\n", "Not anymore lol");

    char best_playlist_ever[30] = "Backyard BBQ";

    d_std::os_debug_printf(arena, "2023 Summer I'm singing: %s\n", best_playlist_ever);

    char other_best_playlist_ever[30] = "Party Cove";

    d_std::os_debug_printf("2024 Summer I'm singing: %s\n", other_best_playlist_ever);

    d_std::d_string chicken_fried = d_std::string_from_lit_string(arena, "cold beer on a friday night");

    d_std::os_debug_printf(arena, "Chicken Fried: %$\n", chicken_fried);

    chicken_fried = d_std::string_from_lit_string(arena, "pair of jeans that fit just right");

    d_std::os_debug_printf("%$\n", chicken_fried);

    arena->reset();

    d_std::d_array<int> my_int_array; my_int_array.make_array(arena, 50);

    for (int i = 0; i < 50; i++){
        my_int_array[i] = 50 - i;
        d_std::os_debug_printf(arena, "Setting index %u of my int array. Value is now: %u\n", i, my_int_array[i]);
    }

    my_int_array.release();

    d_std::Memory_Arena *arena_2 = d_std::make_arena();
    my_int_array.make_array(arena_2, 100);

    
    for (int i = 0; i < 100; i++){
        my_int_array[i] = 100 - i;
        d_std::os_debug_printf(arena, "Setting index %u of my int array. Value is now: %u\n", i, my_int_array[i]);
    }

    u32 hash;

    hash = d_std::murmur3_32((const u8*)"Hello", 5);
    d_std::os_debug_printf(arena, "Hash of Hello: %u\n", hash);

    hash = d_std::murmur3_32((const u8*)"GoodBye", 7);
    d_std::os_debug_printf(arena, "Hash of Goodbye: %u\n", hash);

    hash = d_std::murmur3_32((const u8*)"Hello", 5);
    d_std::os_debug_printf(arena, "Hash of Hello: %u\n", hash);

    // Index past end of array
    my_int_array[101] = 4;

    my_int_array.release();
    arena_2->release();
    arena->release();

    DEBUG_BREAK;

    return 0;

}