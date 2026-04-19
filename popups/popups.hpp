//
// Created by rei on 4/10/2026.
//

#pragma once

#include "custom_scrollbar/custom_scrollbar.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "window_animator/window_animator.hpp"

#include <optional>
#include <string>
#include <vector>

namespace alice
{
    class c_popup
    {
        static inline int last_frame = -1;
        static inline std::vector< ImRect > rects_current;
        static inline std::vector< ImRect > rects_previous;

        static void advance_frame( );

        std::string title;
        float width = 100;
        bool opened = false;

        ImGuiWindow* window_ptr = nullptr;
        ImVec2 actual_position = { };
        ImVec2 actual_size = { };

        bool set_focus = false;
        ImVec2 window_padding = { 6, 5 };

        int opened_frame = -1;
        bool position_set_externally = false;
        ImVec2 position = { 0, 0 };
        std::optional< ImVec2 > size_constraints = std::nullopt;

        c_window_animator window_animator { 0.2f, 0.15f };

        c_custom_scrollbar scrollbar { };
        float last_content_height { 0.f };
        float animated_height { 0.f };
        ImVec2 animated_position { };
        bool layout_initialized = false;
        bool animate_layout = false;

    public:
        c_popup( ) = default;
        c_popup( float initial_width, std::string const& initial_title );

        bool begin( );
        void end( );

        void set_size_constraints( ImVec2 const& size );
        void set_position( ImVec2 const& pos );
        void set_window_padding( ImVec2 const& padding );
        void set_width( float value );
        void set_title( std::string const& new_title );
        void set_animate_layout( bool value );
        void hint_content_height( float height );

        bool open_allowed( ) const;
        bool is_open( ) const;
        void open( );
        void close( );

        c_window_animator* get_window_animator( );
    };
} // namespace alice
