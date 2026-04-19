//
// Created by rei on 4/12/2026.
//

#pragma once

#include "themes.hpp"
#include "custom_scrollbar/custom_scrollbar.hpp"

namespace alice
{
    class c_theme_editor
    {
        static theme_palette_t draft;
        static theme_palette_t backup;
        static int editing_idx;
        static bool pending_open;
        static bool cancelling;

        static char name_buf[ 128 ];

        static c_custom_scrollbar scrollbar;
        static float sb_inset;

    public:
        static void open( int idx );
        static void cancel( );
        static void save( );
        static bool is_open( );
        static int editing( );
        static std::string generate_name( );
        static void render( );
    };
} // namespace alice
