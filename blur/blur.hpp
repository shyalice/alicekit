#pragma once

#include "imgui.h"

namespace alice
{
    class c_blur
    {
    public:
        virtual ~c_blur( ) = default;

        virtual void resize( int w, int h ) = 0;

        virtual bool init_duplication( ) = 0;
        virtual void capture_desktop( ) = 0;

        virtual void prepare_frame( float radius = 12.f, int passes = 2 ) = 0;
        virtual void begin( ImDrawList* dl, float radius = 12.f, int passes = 2 ) = 0;

        virtual void apply( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                            float radius = 12.f, float rounding = 0.f, int passes = 2, float alpha = 1.f ) = 0;

        virtual void apply_squircle( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                                     float radius = 12.f, float rounding = 0.f,
                                     float exponent = 5.f, float arm_ratio = 1.f,
                                     int passes = 2, float alpha = 1.f ) = 0;

        [[nodiscard]] virtual ImTextureID get_texture( ) const = 0;

        virtual void uv_for( ImVec2 p_min, ImVec2 p_max, ImVec2& uv_min, ImVec2& uv_max ) const = 0;
        virtual void add_image( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col = IM_COL32_WHITE ) const = 0;
        virtual void add_image_rounded( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                         ImDrawFlags flags = ImDrawFlags_RoundCornersAll, ImU32 col = IM_COL32_WHITE ) const = 0;
        virtual void add_image_squircle( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                          ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                                          float exponent = 5.f, float arm_ratio = 1.f,
                                          int segs_per_corner = 0, ImU32 col = IM_COL32_WHITE ) const = 0;
    };
} // namespace alice
