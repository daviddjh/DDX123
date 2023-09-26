#pragma once

#include "../pch.h"
#include "string.h"

enum Binding_Point_Index : u32 {
    LIGHT_MATRIX   = 1,
    SHADOW_MAP     = 2,
    PER_FRAME_DATA = 3,
    VIEW_PROJECTION_MATRIX = 4,
    CAMERA_POSITION_BUFFER = 5,
    ALBEDO_GBUFFER = 6,
    POSITION_GBUFFER = 7,
    NORMAL_GBUFFER = 8,
    ROUGHNESS_AND_METALLIC_GBUFFER = 9,
    ALBEDO_INDEX = 10,
    POSITION_INDEX = 11,
    NORMAL_INDEX = 12,
    ROUGHNESS_METALLIC_INDEX = 13,
    OUTPUT_DIMENSIONS = 14,
    TEXTURE_2D_TABLE = 15,
    BINDING_POINT_INDEX_COUNT,
};

struct Binding_Point_String_Map {
    const char * string;
    u32 index;
};

const Binding_Point_String_Map binding_point_map[] = {
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
};

constexpr u32 binding_point_string_lookup(const char* string){
    for (Binding_Point_String_Map binding_point_string_map : binding_point_map) {
        if(strcmp(binding_point_string_map.string, string) == 0){
            return binding_point_string_map.index;
        }
    }

    DEBUG_LOG("Cant find the binding point string! Make sure you've included the binding point into 'binding_point_map'.");
    return (u32)0 - (u32)1; // If we cant find the string, return the max u32 number
}
