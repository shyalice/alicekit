//
// Created by rei on 4/14/2026.
//

#pragma once

#include "imgui_internal.h"

#include <string>
#include <vector>

namespace alice
{
    struct tooltip_info_t
    {
        bool hovered;
        ImVec2 position = { 0, 0 };
        float mod = 0.f;
        float hover_time = 0.f;
    };

    class c_tooltips
    {
        static void render_frame( const ImRect& rect, float alpha = 1.f );
        static bool window_backend( tooltip_info_t* info, const std::string& title, const ImVec2& pos );

    public:
        static void standard( tooltip_info_t* info, const std::string& title, const std::vector< std::pair< ImColor, std::string > >& lines );
        static void simple( tooltip_info_t* info, const std::string& text );
    };
} // namespace alice