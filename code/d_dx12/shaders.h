#pragma once

#include "../pch.h"

const enum Binding_Point_Index : u32 {
    LIGHT_MATRIX,
    SHADOW_MAP,
    PER_FRAME_DATA,
    VIEW_PROJECTION_MATRIX,
    CAMERA_POSITION_BUFFER,
    ALBEDO_GBUFFER,
    POSITION_GBUFFER,
    NORMAL_GBUFFER,
    ROUGHNESS_AND_METALLIC_GBUFFER,
    ALBEDO_INDEX,
    POSITION_INDEX,
    NORMAL_INDEX,
    ROUGHNESS_METALLIC_INDEX,
    OUTPUT_DIMENSIONS,
    TEXTURE_2D_TABLE,
    MODEL_MATRIX,
    MATERIAL_FLAGS,
    SAMPLER_1, 
    DEPTH_BUFFER_INDEX,
    BINDING_POINT_INDEX_COUNT,
};

struct Binding_Point_String_Map {
    const char * string;
    u32 index;
};

constexpr const Binding_Point_String_Map binding_point_map[] = {
    {"light_matrix", LIGHT_MATRIX},
    {"Shadow_Map", SHADOW_MAP},
    {"per_frame_data", PER_FRAME_DATA},
    {"view_projection_matrix", VIEW_PROJECTION_MATRIX},
    {"camera_position_buffer", CAMERA_POSITION_BUFFER},
    {"Albedo Gbuffer", ALBEDO_GBUFFER},
    {"Position Gbuffer", POSITION_GBUFFER},
    {"Normal Gbuffer", NORMAL_GBUFFER},
    {"Roughness and Metallic Gbuffer", ROUGHNESS_AND_METALLIC_GBUFFER},
    {"albedo_index", ALBEDO_INDEX},
    {"position_index", POSITION_INDEX},
    {"normal_index", NORMAL_INDEX},
    {"roughness_metallic_index", ROUGHNESS_METALLIC_INDEX},
    {"output_dimensions", OUTPUT_DIMENSIONS},
    {"texture_2d_table", TEXTURE_2D_TABLE},
    {"model_matrix", MODEL_MATRIX},
    {"material_flags", MATERIAL_FLAGS},
    {"sampler_1", SAMPLER_1},
    {"depth_buffer_index", DEPTH_BUFFER_INDEX},
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

    DEBUG_ERROR("Cant find the binding point string! Make sure you've included the binding point into 'binding_point_map'.");
    return (u32)0 - (u32)1; // If we cant find the string, return the max u32 number
}
