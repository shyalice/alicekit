#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "window_animator/window_animator.hpp"

#include <string>
#include <unordered_map>

namespace alice
{
    struct window_state_t
    {
        c_window_animator animator { 0.12f, 0.08f };
        ImVec2 position { };
        ImVec2 spawn_position { };
        bool open = false;
        bool dragging = false;
        ImVec2 drag_offset { };
        bool focus_pending = false;
    };

    class c_windows
    {
    public:
        void open( std::string const& id, ImVec2 const& spawn_pos, ImVec2 const& pivot = { 0.f, 0.f } );
        void close( std::string const& id );
        void toggle( std::string const& id, ImVec2 const& spawn_pos, ImVec2 const& pivot = { 0.f, 0.f } );

        [[nodiscard]] bool is_open( std::string const& id ) const;

        bool begin( std::string const& id, std::string const& title, ImVec2 const& size );
        void end( );

        void update( float dt );

        c_window_animator* current_animator( );

    private:
        std::unordered_map< std::string, window_state_t > states;
        std::string current_id;
        ImVec2 current_size { };

        static constexpr float k_title_h = 30.f;
        static constexpr float k_rounding = 10.f;
        static constexpr float k_pad = 12.f;
        static constexpr float k_close_radius = 8.f;
    };

} // namespace alice
