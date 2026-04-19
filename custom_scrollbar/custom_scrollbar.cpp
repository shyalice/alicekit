//
// Created by rei on 4/11/2026.
//

#include "custom_scrollbar.hpp"

#include "themes/themes.hpp"
#include "util/util.hpp"

using alice::c_custom_scrollbar;
using ImGui::ButtonBehavior;
using ImGui::GetCurrentContext;
using ImGui::GetIO;
using ImGui::GetStyle;
using ImGui::IsMouseDown;
using ImGui::ItemAdd;
using ImGui::KeepAliveID;
using ImGui::SetScrollY;

void c_custom_scrollbar::render( ImGuiWindow* window, ImVec2 const& padding, bool inputs_allowed, float width )
{
    if ( !window )
        return;

    float const scroll_max = window->ScrollMax.y;
    float const scroll_y = window->Scroll.y;
    bool const has_overflow = scroll_max > 0.5f;

    float const dt = GetIO( ).DeltaTime;
    float const speed = 20.f * dt;

    auto const lerp_snap = []( float& v, float target, float t ) {
        v = ImLerp( v, target, t );
        if ( ImFabs( v - target ) < 0.001f )
            v = target;
    };

    lerp_snap( fade, has_overflow ? 1.f : 0.f, speed );

    if ( fade <= 0.001f )
    {
        if ( !IsMouseDown( 0 ) )
            dragging = false;
        return;
    }

    float const sb_w = width;
    float const track_x = window->Pos.x + window->Size.x - sb_w - padding.x;
    float const track_y0 = window->Pos.y + padding.y;
    float const track_y1 = window->Pos.y + window->Size.y - padding.y;
    float const track_h = track_y1 - track_y0;

    float const visible_ratio = ImClamp( window->Size.y / ImMax( window->Size.y + scroll_max, 1.f ), 0.12f, 1.f );
    float const thumb_h = ImMax( 20.f, track_h * visible_ratio );
    float const scroll_range = ImMax( scroll_max, 1.f );
    float const thumb_range = ImMax( track_h - thumb_h, 1.f );
    float const clamped_scroll = ImClamp( scroll_y, 0.f, scroll_max );
    float const thumb_y = track_y0 + thumb_range * ( clamped_scroll / scroll_range );

    ImRect const thumb_rect { { track_x, thumb_y }, { track_x + sb_w, thumb_y + thumb_h } };
    ImRect const track_rect { { track_x - 2, track_y0 }, { track_x + sb_w + 2, track_y1 } };

    ImVec2 const mouse = GetIO( ).MousePos;

    ImGuiID const track_id = window->GetID( "##custom_scrollbar_track" );
    KeepAliveID( track_id );

    bool track_hovered = false;
    bool track_held = false;
    if ( has_overflow && inputs_allowed )
    {
        auto& g = *GetCurrentContext( );
        ImRect const saved_clip = window->ClipRect;
        ImGuiWindow* const saved_hovered_window = g.HoveredWindow;
        bool const mouse_inside_track = track_rect.Contains( GetIO( ).MousePos );

        window->ClipRect.Add( track_rect );
        if ( mouse_inside_track )
            g.HoveredWindow = window;

        ItemAdd( track_rect, track_id, nullptr, ImGuiItemFlags_NoNav );
        ButtonBehavior( track_rect, track_id, &track_hovered, &track_held, ImGuiButtonFlags_PressedOnClick );

        window->ClipRect = saved_clip;
        g.HoveredWindow = saved_hovered_window;
    }

    bool const over_thumb = thumb_rect.Contains( mouse );

    if ( has_overflow && inputs_allowed )
    {
        if ( track_held && !dragging )
        {
            if ( over_thumb )
            {
                dragging = true;
                drag_grab = mouse.y - thumb_y;
            }
            else
            {
                float const target_thumb_y = ImClamp( mouse.y - thumb_h * 0.5f, track_y0, track_y0 + thumb_range );
                SetScrollY( window, ( target_thumb_y - track_y0 ) / thumb_range * scroll_max );
                dragging = true;
                drag_grab = thumb_h * 0.5f;
            }
        }
        if ( dragging && IsMouseDown( 0 ) )
        {
            float const new_thumb_y = ImClamp( mouse.y - drag_grab, track_y0, track_y0 + thumb_range );
            SetScrollY( window, ( new_thumb_y - track_y0 ) / thumb_range * scroll_max );
        }
    }
    if ( !IsMouseDown( 0 ) )
        dragging = false;

    bool const is_hovered = over_thumb || track_hovered || dragging;
    lerp_snap( hover, is_hovered ? 1.f : 0.f, speed );
    lerp_snap( active, dragging ? 1.f : 0.f, speed );

    float const alpha = GetStyle( ).Alpha * fade;
    auto* draw = window->DrawList;
    draw->PushClipRectFullScreen( );

    auto const& t = c_themes::palette( );

    draw->AddRectFilled( { track_x, track_y0 }, { track_x + sb_w, track_y1 }, color_t( t.control ).modulate( alpha * 0.6f ), sb_w * 0.5f );

    auto const base = t.control_hover;
    auto const hov = t.accent_subtle;
    auto const act = t.accent_surface;
    auto thumb_col = ImLerp( base, hov, hover );
    thumb_col = ImLerp( thumb_col, act, active );

    draw->AddRectFilled( thumb_rect.Min, thumb_rect.Max, color_t( thumb_col ).modulate( alpha ), sb_w * 0.5f );

    draw->PopClipRect( );
}
