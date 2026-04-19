#pragma once

#include "imgui.h"

namespace alice
{
    class c_grayscale
    {
    public:
        virtual ~c_grayscale( ) = default;

        virtual void resize( int w, int h ) = 0;

        virtual void apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                            float strength ) = 0;

        virtual void add_image( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
                                float strength = 1.f, ImU32 col = IM_COL32_WHITE ) = 0;

        virtual void add_image_rounded( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
                                        float rounding, ImDrawFlags flags,
                                        float strength = 1.f, ImU32 col = IM_COL32_WHITE ) = 0;
    };
} // namespace alice
