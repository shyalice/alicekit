//
// Created by rei on 4/10/2026.
//

#include "popups.hpp"

#include "alice_kit.hpp"
#include "imgui_internal.h"
#include "renderer/renderer.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"

using namespace ImGui;

using alice::c_popup;
using alice::c_window_animator;

void c_popup::advance_frame( )
{
    int const current_frame = GetFrameCount( );
    if ( current_frame == last_frame )
        return;
    rects_previous = std::move( rects_current );
    rects_current.clear( );
    last_frame = current_frame;
}

c_popup::c_popup( float initial_width, std::string const& initial_title )
    : title( initial_title )
    , width( initial_width )
{
}

bool c_popup::begin( )
{
    if ( opened && GetStyle( ).Alpha < 0.02f )
        opened = false;

    opened ? window_animator.open( ) : window_animator.close( );
    window_animator.update( GetIO( ).DeltaTime );

    advance_frame( );

    if ( !window_animator.visible( ) )
    {
        set_focus = true;
        layout_initialized = false;
        if ( !position_set_externally )
            position = GetMousePos( );

        return false;
    }

    float const max_h = size_constraints.has_value( ) ? size_constraints->y : FLT_MAX;
    float target_h = last_content_height + window_padding.y * 2;
    if ( last_content_height <= 0.f )
        target_h = size_constraints.has_value( ) ? size_constraints->y : 40.f;
    target_h = ImClamp( target_h, window_padding.y * 2 + 1.f, max_h );

    if ( animate_layout )
    {
        float const anim_speed = 20.f * GetIO( ).DeltaTime;

        if ( !layout_initialized )
        {
            animated_height = target_h;
            animated_position = position;
            layout_initialized = true;
        } else
        {
            animated_height = ImLerp( animated_height, target_h, anim_speed );
            if ( ImFabs( animated_height - target_h ) < 0.5f )
                animated_height = target_h;
            animated_position = ImLerp( animated_position, position, anim_speed );
            if ( ImFabs( animated_position.x - position.x ) < 0.5f && ImFabs( animated_position.y - position.y ) < 0.5f )
                animated_position = position;
        }

        SetNextWindowPos( ImFloor( animated_position ), ImGuiCond_Always );
        SetNextWindowSize( { width, ImFloor( animated_height ) } );
    } else
    {
        SetNextWindowPos( position, ImGuiCond_Always );
        SetNextWindowSize( { width, target_h } );
    }

    PushStyleVar( ImGuiStyleVar_WindowPadding, window_padding );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, ImVec2 { 0, 0 } );
    window_animator.begin( );

    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground;
    if ( !opened )
        flags |= ImGuiWindowFlags_NoInputs;

    Begin( title.c_str( ), nullptr, flags );
    auto* window = GetCurrentWindow( );
    if ( animate_layout )
    {
        window->Scroll.y = 0.f;
        window->ScrollMax.y = 0.f;
    }
    window_animator.capture( window->DrawList );

    {
        constexpr float k_scrollbar_reserve = 6.f + 5.f;
        float const sb_fade = scrollbar.get_fade( );
        window->ContentRegionRect.Max.x -= k_scrollbar_reserve * sb_fade;
        window->WorkRect.Max.x -= k_scrollbar_reserve * sb_fade;
    }

    if ( set_focus )
    {
        BringWindowToDisplayFront( window );
        set_focus = false;
    }

    auto const& t = c_themes::palette( );

    window->DrawList->PushClipRectFullScreen( );
    renderer_t::add_shadow( window->DrawList, window->Pos, window->Pos + window->Size, color_t( t.shadow ).modulate( GetStyle( ).Alpha ), 8, 16, ImDrawFlags_RoundCornersAll, 5, 3.f, 20 );
    {
        float const s_scale = window_animator.scale( );
        ImVec2 const blur_min = window->Pos;
        ImVec2 const blur_max = window->Pos + window->Size;
        ImVec2 const blur_center = ( blur_min + blur_max ) * 0.5f;
        ImVec2 const sm = blur_center + ( blur_min - blur_center ) * s_scale;
        ImVec2 const sx = blur_center + ( blur_max - blur_center ) * s_scale;
        core->blur->apply_squircle( window->DrawList, sm, sx, 18, 10, 5, 3.f, 2, GetStyle( ).Alpha );
    }
    renderer_t::add_rect_filled( window->DrawList, window->Pos, window->Pos + window->Size, color_t( t.popup_bg ).modulate( GetStyle( ).Alpha ), 10, ImDrawFlags_RoundCornersAll, 5, 3.f );
    renderer_t::add_rect( window->DrawList, window->Pos, window->Pos + window->Size, color_t( t.border ).modulate( GetStyle( ).Alpha ), 1, 10, ImDrawFlags_RoundCornersAll, 5, 3.f );
    window->DrawList->PopClipRect( );

    rects_current.emplace_back( window->Pos, window->Pos + window->Size );

    if ( opened && GetIO( ).MouseClicked[ 0 ] && opened_frame != GetFrameCount( ) )
    {
        ImVec2 const mouse = GetIO( ).MousePos;
        bool inside_any = false;

        for ( auto const& r : rects_previous )
        {
            if ( r.Contains( mouse ) )
            {
                inside_any = true;
                break;
            }
        }

        if ( !inside_any )
        {
            for ( auto const& r : rects_current )
            {
                if ( r.Contains( mouse ) )
                {
                    inside_any = true;
                    break;
                }
            }
        }

        if ( !inside_any )
            opened = false;
    }

    window_ptr = window;

    if ( window->SkipItems )
    {
        end( );
        return false;
    }

    return true;
}

void c_popup::end( )
{
    last_content_height = window_ptr->DC.CursorMaxPos.y - window_ptr->DC.CursorStartPos.y;

    scrollbar.render( window_ptr, window_padding, opened && size_constraints.has_value( ) );

    actual_size = window_ptr->Size;
    actual_position = window_ptr->Pos;

    window_animator.end( actual_position, actual_size );
    PopStyleVar( 2 );

    End( );
}

void c_popup::set_size_constraints( ImVec2 const& size )
{
    size_constraints = size;
}

void c_popup::set_position( ImVec2 const& pos )
{
    position_set_externally = true;
    position = pos;
}

void c_popup::set_window_padding( ImVec2 const& padding )
{
    window_padding = padding;
}

void c_popup::set_width( float value )
{
    width = value;
}

void c_popup::set_title( std::string const& new_title )
{
    title = new_title;
}

void c_popup::set_animate_layout( bool value )
{
    animate_layout = value;
}

void c_popup::hint_content_height( float height )
{
    if ( last_content_height <= 0.f )
        last_content_height = height;
}

bool c_popup::open_allowed( ) const
{
    return !window_animator.visible( ) && !opened;
}

bool c_popup::is_open( ) const
{
    return window_animator.visible( ) || opened;
}

void c_popup::open( )
{
    if ( !opened )
        opened_frame = GetFrameCount( );
    opened = true;
}

void c_popup::close( )
{
    opened = false;
}

c_window_animator* c_popup::get_window_animator( )
{
    return &window_animator;
}
