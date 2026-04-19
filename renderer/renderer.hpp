//
// Created by rei on 4/8/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

namespace alice
{
    struct renderer_t
    {
        static void path_squircle_corner( ImDrawList* dl, ImVec2 center, float rx, float ry, float sx, float sy, float exponent, int segs, bool reverse );
        static void build_squircle_path( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding, ImDrawFlags flags, float exponent, float arm_ratio, int segs );

        static void add_rect( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max,
                              ImU32 col, float thickness, float rounding,
                              ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                              float exponent = 5.f,
                              float arm_ratio = 1.f,
                              int segs_per_corner = 0 );

        static void add_rect_filled( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max,
                                     ImU32 col, float rounding,
                                     ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                                     float exponent = 5.f,
                                     float arm_ratio = 1.f,
                                     int segs_per_corner = 0 );

        // ghetto but works as expected so idc
        static void add_shadow( ImDrawList* dl,
                                ImVec2 p_min, ImVec2 p_max,
                                ImU32 col, float rounding,
                                float radius,
                                ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                                float exponent = 5.f,
                                float arm_ratio = 1.f,
                                int steps = 12 );
    };
} // namespace alice