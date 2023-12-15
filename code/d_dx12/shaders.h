#pragma once

#include "../pch.h"

const enum Binding_Point_Index : u32 {
    SAMPLER_1, 
    TEXTURE_2D_TABLE,
    PER_FRAME_DATA,
    OUTPUT_DIMENSIONS,
    MATERIAL_DATA,
    GBUFFER_INDICES,
    SSAO_SAMPLE,
    SSAO_TEXTURE_INDEX,
    SHADOW_TEXTURE_INDEX,
    MODEL_MATRIX,
    LIGHT_MATRIX,
    BINDING_POINT_INDEX_COUNT,
};

struct Binding_Point_String_Map {
    const char * string;
    u32 index;
};

constexpr const Binding_Point_String_Map binding_point_map[] = {
    {"sampler_1", SAMPLER_1},
    {"texture_2d_table", TEXTURE_2D_TABLE},
    {"per_frame_data", PER_FRAME_DATA},
    {"output_dimensions", OUTPUT_DIMENSIONS},
    {"material_data", MATERIAL_DATA},
    {"gbuffer_indices", GBUFFER_INDICES},
    {"ssao_sample", SSAO_SAMPLE},
    {"ssao_texture_index", SSAO_TEXTURE_INDEX},
    {"shadow_texture_index", SHADOW_TEXTURE_INDEX},
    {"model_matrix", MODEL_MATRIX},
    {"light_matrix", LIGHT_MATRIX},
};

// Got from: https://stackoverflow.com/questions/27490858/how-can-you-compare-two-character-strings-statically-at-compile-time
constexpr bool const_string_compare(char const a[], char const b[]) {
    return *a == *b && (*a == '\0' || const_string_compare(a + 1, b + 1));
}

constexpr u32 binding_point_string_lookup(const char string[]){
    for (Binding_Point_String_Map binding_point_string_map : binding_point_map) {
        if(const_string_compare(binding_point_string_map.string, string) == true){
            return binding_point_string_map.index;
        }
    }

    //DEBUG_ERROR("Cant find the binding point string! Make sure you've included the binding point into 'binding_point_map'.");
    return (u32)0 - (u32)1; // If we cant find the string, return the max u32 number
}
