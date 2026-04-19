//
// Created by rei on 4/11/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <unordered_map>

namespace alice
{
    struct smooth_scroll_entry_t
    {
        float pos_x { 0.f };
        float pos_y { 0.f };
        float vel_x { 0.f };
        float vel_y { 0.f };
        float target_x { 0.f };
        float target_y { 0.f };
        float last_applied_x { 0.f };
        float last_applied_y { 0.f };
        int last_frame { 0 };
        bool initialized { false };
    };

    class c_smooth_scroll
    {
        static constexpr float k_smooth_time = 0.16f;
        static constexpr float k_rubber_band = 55.f;
        static constexpr float k_wheel_overshoot_gain = 7.f;
        static constexpr float k_max_velocity = 6000.f;
        static constexpr float k_max_dt = 1.f / 30.f;
        static constexpr float k_external_drift_eps = 2.5f;

        std::unordered_map< ImGuiID, smooth_scroll_entry_t > states;
        std::unordered_map< ImGuiID, float > step_overrides;

        static void init_state( smooth_scroll_entry_t& s, ImGuiWindow const* window );
        static float rubber_factor( float over );
        static void apply_wheel( float& pos, float& vel, float& target, float delta, float max );
        static void smooth_damp( float& pos, float& vel, float target, float dt );

    public:
        static constexpr float k_overscroll_limit = 80.f;

        static c_smooth_scroll& instance( );

        void on_wheel_y( ImGuiWindow const* window, float delta );
        void on_wheel_x( ImGuiWindow const* window, float delta );
        void tick( );
        void apply_to_window( ImGuiWindow* window );

        void push_step( float step );
        void pop_step( );
        [[nodiscard]] float query_step( ImGuiWindow const* window, float default_step ) const;
    };
} // namespace alice
