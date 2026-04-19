//
// Created by rei on 4/8/2026.
//

#pragma once

#include "imgui.h"

#include <string>

namespace alice
{
    std::string truncate_text( ImFont* font, float font_size, std::string const& text, float max_width );

    struct color_t
    {
        ImVec4 value;

        constexpr color_t( ) {}
        constexpr color_t( float r, float g, float b, float a = 1.0f )
            : value( r, g, b, a )
        {
        }
        constexpr color_t( ImVec4 const& col )
            : value( col )
        {
        }
        constexpr color_t( ImColor const& col )
            : value( col.Value )
        {
        }
        constexpr color_t( int r, int g, int b, int a = 255 )
            : value( ( float ) r * ( 1.0f / 255.0f ), ( float ) g * ( 1.0f / 255.0f ), ( float ) b * ( 1.0f / 255.0f ), ( float ) a * ( 1.0f / 255.0f ) )
        {
        }
        constexpr color_t( ImU32 rgba )
            : value( ( float ) ( ( rgba >> IM_COL32_R_SHIFT ) & 0xFF ) * ( 1.0f / 255.0f ), ( float ) ( ( rgba >> IM_COL32_G_SHIFT ) & 0xFF ) * ( 1.0f / 255.0f ), ( float ) ( ( rgba >> IM_COL32_B_SHIFT ) & 0xFF ) * ( 1.0f / 255.0f ), ( float ) ( ( rgba >> IM_COL32_A_SHIFT ) & 0xFF ) * ( 1.0f / 255.0f ) )
        {
        }

        color_t dim( float dim ) const
        {
            return { value.x * dim, value.y * dim, value.z * dim, value.w };
        }

        ImColor modulate( float alpha = ImGui::GetStyle( ).Alpha ) const
        {
            return { value.x, value.y, value.z, value.w * alpha };
        }

        ImColor u32( ) const
        {
            return value;
        }
    };
} // namespace alice