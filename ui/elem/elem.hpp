//
// Created by rei on 4/9/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include "fonts/fonts.hpp"

#include "elem_animations/elem_animations.hpp"
#include "standalone_elem/standalone_elem.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"

#include <functional>
#include <string>
#include <type_traits>
#include <vector>

namespace alice
{
    struct basic_elem_info_t
    {
        bool disabled;
        float disable_mod;
    };

    struct visible_info_t
    {
        float target { 0.f };
        float alpha { 0.f };
        float current_h { 0.f };
        float natural_h { 0.f };
        bool initialized { false };
    };

    struct select_button_info_t
    {
        ImVec4 background;
        ImVec4 outline;
        ImVec4 text;
    };

    struct item_builder
    {
        constexpr static int k_elem_spacing = 10;

        std::string name;
        ImRect rect;
        basic_elem_info_t* basic_info;

        item_builder add_tooltip( const std::string& text, bool force = false );
        item_builder add_tooltip( const std::string& title, const std::vector< std::pair< ImColor, std::string > >& lines, bool force = false );

        item_builder add_toggle( bool* v );

        item_builder add_dropdown( int* v, std::vector< std::string > const& entries );

        item_builder add_color( ImVec4* v );
        item_builder add_color( std::pair< ImVec4*, std::optional< ImVec4* > > v );

        template < typename T >
        item_builder add_slider( T* v, T v_min, T v_max, char const* format = nullptr, bool has_input = true );

        void finish( ) {}
    };

    class c_ui_elements
    {
        constexpr static int k_elem_height = 38;

        static std::optional< bool > is_disabled;

        static font_t font;
        static std::vector< int > group_item_stack;

        struct visible_stack_entry_t
        {
            visible_info_t* info;
            bool visible;
            ImVec2 start_cursor;
            float start_max_y;
        };
        static std::vector< visible_stack_entry_t > visible_stack;

        static void next_item_separator( );

    public:
        static void prepare( );

        static void push_disabled( bool disabled = true );
        static void pop_disabled( );

        static void push_visible( std::string const& id, bool visible );
        static void pop_visible( );
        static void visible_if( std::string const& id, bool visible, std::function< void( ) > const& body );

        static void begin_group( std::string const& name, ImVec2 const& size, bool absolute = false, ImGuiWindowFlags flags = 0 );
        static void end_group( );

        static bool select_button( std::string const& name, bool* v, ImVec2 const& size_arg = { 0, 0 }, std::function< void( ) > const& rmb_callback = { } );

        static item_builder start( std::string const& label );
    };

} // namespace alice