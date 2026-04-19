//
// Created by rei on 4/10/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "popups/popups.hpp"
#include "window_animator/window_animator.hpp"

#include <string>

namespace alice
{
    struct selectable_info_t
    {
        ImVec4 background;
        ImVec4 mark;
        ImVec4 text;
    };

    struct button_info_t
    {
        ImVec4 background;
        ImVec4 text;
    };

    struct expander_info_t
    {
        ImVec4 background;
        ImVec4 text;
        ImVec4 arrow;
        c_popup popup { 150, "##expander_popup" };
        expander_info_t* parent = nullptr;
        bool initialized = false;
    };

    class c_popup_elem
    {
        constexpr static int k_elem_height = 28;
        static expander_info_t* current_expander;

    public:
        c_popup_elem( );
        ~c_popup_elem( );

        static int get_elem_height( );

        static void begin_group( c_window_animator* animator, std::string const& id );
        static void end_group( );

        static void text( std::string const& label, float pad_top = 2.f, float pad_bottom = 8.f );

        static bool button( std::optional< std::string > const& icon, std::string const& label );
        static bool selectable( std::optional< std::string > const& icon, std::string const& label, bool active );
        static bool toggle( std::optional< std::string > const& icon, std::string const& label, bool* v );

        static bool begin_expander( std::optional< std::string > const& icon, std::string const& label, float x = 5, float width = 150 );
        static void end_expander( );
    };
} // namespace alice