//
// Created by rei on 4/11/2026.
//

#include "smooth_scroll.hpp"

using alice::c_smooth_scroll;
using alice::smooth_scroll_entry_t;
using ImGui::FindWindowByID;
using ImGui::GetCurrentWindow;
using ImGui::GetFrameCount;
using ImGui::GetIO;

void c_smooth_scroll::init_state( smooth_scroll_entry_t& s, ImGuiWindow const* window )
{
    if ( s.initialized )
        return;
    float const smax_x = window->ScrollMax.x;
    float const smax_y = window->ScrollMax.y;
    s.pos_x = ImClamp( window->Scroll.x, 0.f, smax_x );
    s.pos_y = ImClamp( window->Scroll.y, 0.f, smax_y );
    s.target_x = s.pos_x;
    s.target_y = s.pos_y;
    s.vel_x = 0.f;
    s.vel_y = 0.f;
    s.initialized = true;
}

float c_smooth_scroll::rubber_factor( float over )
{
    return 1.f / ( 1.f + over / k_rubber_band );
}

void c_smooth_scroll::apply_wheel( float& pos, float& vel, float& target, float delta, float max )
{
    bool const at_top = target <= 0.f;
    bool const at_bot = target >= max;
    bool const pushing_past = ( delta < 0.f && at_top ) || ( delta > 0.f && at_bot );

    if ( pushing_past )
    {
        float const over = ( pos < 0.f ) ? -pos : ( ( pos > max ) ? pos - max : 0.f );
        float const factor = rubber_factor( over );
        vel = ImClamp( vel + delta * k_wheel_overshoot_gain * factor, -k_max_velocity, k_max_velocity );
        target = ImClamp( target, 0.f, max );
    } else
    {
        target = ImClamp( target + delta, 0.f, max );
    }
}

void c_smooth_scroll::smooth_damp( float& pos, float& vel, float target, float dt )
{
    constexpr float omega = 2.f / k_smooth_time;
    float const x = omega * dt;
    float const exp_approx = 1.f / ( 1.f + x + 0.48f * x * x + 0.235f * x * x * x );
    float const change = pos - target;
    float const temp = ( vel + omega * change ) * dt;
    vel = ( vel - omega * temp ) * exp_approx;
    pos = target + ( change + temp ) * exp_approx;
}

c_smooth_scroll& c_smooth_scroll::instance( )
{
    static c_smooth_scroll inst;
    return inst;
}

void c_smooth_scroll::on_wheel_y( ImGuiWindow const* window, float delta )
{
    if ( !window )
        return;
    auto& s = states[ window->ID ];
    init_state( s, window );
    apply_wheel( s.pos_y, s.vel_y, s.target_y, delta, window->ScrollMax.y );
    s.last_frame = GetFrameCount( );
}

void c_smooth_scroll::on_wheel_x( ImGuiWindow const* window, float delta )
{
    if ( !window )
        return;
    auto& s = states[ window->ID ];
    init_state( s, window );
    apply_wheel( s.pos_x, s.vel_x, s.target_x, delta, window->ScrollMax.x );
    s.last_frame = GetFrameCount( );
}

void c_smooth_scroll::tick( )
{
    for ( auto it = states.begin( ); it != states.end( ); )
    {
        auto* window = FindWindowByID( it->first );
        if ( !window || !window->WasActive )
        {
            it = states.erase( it );
            continue;
        }
        ++it;
    }

    for ( auto it = step_overrides.begin( ); it != step_overrides.end( ); )
    {
        auto* window = FindWindowByID( it->first );
        if ( !window || !window->WasActive )
        {
            it = step_overrides.erase( it );
            continue;
        }
        ++it;
    }
}

void c_smooth_scroll::push_step( float step )
{
    auto* window = GetCurrentWindow( );
    if ( !window || step <= 0.f )
        return;
    step_overrides[ window->ID ] = step;
}

void c_smooth_scroll::pop_step( )
{
}

float c_smooth_scroll::query_step( ImGuiWindow const* window, float default_step ) const
{
    if ( !window )
        return default_step;
    auto const it = step_overrides.find( window->ID );
    return it != step_overrides.end( ) ? it->second : default_step;
}

void c_smooth_scroll::apply_to_window( ImGuiWindow* window )
{
    if ( !window )
        return;
    auto it = states.find( window->ID );
    if ( it == states.end( ) || !it->second.initialized )
        return;

    auto& s = it->second;
    float const dt = ImMin( GetIO( ).DeltaTime, k_max_dt );
    float const smax_y = window->ScrollMax.y;
    float const smax_x = window->ScrollMax.x;

    if ( ImFabs( window->Scroll.y - s.last_applied_y ) > k_external_drift_eps )
    {
        s.pos_y = window->Scroll.y;
        s.target_y = ImClamp( window->Scroll.y, 0.f, smax_y );
        s.vel_y = 0.f;
    }
    if ( ImFabs( window->Scroll.x - s.last_applied_x ) > k_external_drift_eps )
    {
        s.pos_x = window->Scroll.x;
        s.target_x = ImClamp( window->Scroll.x, 0.f, smax_x );
        s.vel_x = 0.f;
    }

    s.target_y = ImClamp( s.target_y, 0.f, smax_y );
    s.target_x = ImClamp( s.target_x, 0.f, smax_x );

    smooth_damp( s.pos_y, s.vel_y, s.target_y, dt );
    smooth_damp( s.pos_x, s.vel_x, s.target_x, dt );

    constexpr float hard_limit = c_smooth_scroll::k_overscroll_limit;
    s.pos_y = ImClamp( s.pos_y, -hard_limit, smax_y + hard_limit );
    s.pos_x = ImClamp( s.pos_x, -hard_limit, smax_x + hard_limit );

    window->Scroll.y = s.pos_y;
    window->Scroll.x = s.pos_x;
    s.last_applied_y = s.pos_y;
    s.last_applied_x = s.pos_x;
    s.last_frame = GetFrameCount( );
}
