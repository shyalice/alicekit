//
// Created by rei on 4/13/2026.
//

#include "notifications.hpp"

#include "alice_kit.hpp"
#include "fonts/fonts.hpp"
#include "fonts/lucide_defines.hpp"
#include "renderer/renderer.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"

#include "imgui_internal.h"

#include <algorithm>
#include <cmath>

using alice::c_notifications;
using alice::notification_t;
using alice::notification_type;

std::vector< notification_t > c_notifications::entries;
uint32_t c_notifications::next_id = 1;

float c_notifications::smooth( float current, float target, float speed, float dt )
{
    float const diff = target - current;
    if ( fabsf( diff ) < 0.001f )
        return target;
    return current + diff * ImMin( speed * dt, 1.f );
}

ImVec4 c_notifications::color_for_type( notification_type type )
{
    auto const& t = c_themes::palette( );
    switch ( type )
    {
    case notification_type::loading: return t.accent;
    case notification_type::success: return { 0.35f, 0.82f, 0.45f, 1.f };
    case notification_type::failed:  return t.danger;
    case notification_type::tip:     return t.text_muted;
    }
    return t.text_muted;
}

char const* c_notifications::icon_for_type( notification_type type )
{
    switch ( type )
    {
    case notification_type::loading: return ICON_LUCIDE_LOADER_CIRCLE;
    case notification_type::success: return ICON_LUCIDE_CIRCLE_CHECK;
    case notification_type::failed:  return ICON_LUCIDE_CIRCLE_X;
    case notification_type::tip:     return ICON_LUCIDE_INFO;
    }
    return ICON_LUCIDE_INFO;
}

float c_notifications::compute_height( notification_t const& n )
{
    auto& tf = core->fonts->get( "bold" );
    auto& df = core->fonts->get( "primary" );

    float h = k_pad;

    if ( tf.ptr )
        h += tf.ptr->CalcTextSizeA( tf.size, FLT_MAX, -1, n.title.c_str( ) ).y;
    else
        h += 14.f;

    if ( !n.description.empty( ) && df.ptr )
        h += 3.f + df.ptr->CalcTextSizeA( df.size, FLT_MAX, k_desc_max, n.description.c_str( ) ).y;

    h += k_pad;

    return ImMax( h, k_icon_area + k_pad * 2.f );
}

uint32_t c_notifications::push( notification_type type, std::string title, std::string description,
                                float duration )
{
    uint32_t const id = next_id++;
    entries.push_back( { .id = id, .type = type, .prev_type = type,
                         .title = std::move( title ), .description = std::move( description ),
                         .max_lifetime = duration } );
    return id;
}

uint32_t c_notifications::push_persistent( notification_type type, std::string title, std::string description )
{
    uint32_t const id = next_id++;
    entries.push_back( { .id = id, .type = type, .prev_type = type,
                         .title = std::move( title ), .description = std::move( description ),
                         .max_lifetime = 0.f, .persistent = true } );
    return id;
}

void c_notifications::resolve( uint32_t id, notification_type new_type,
                               std::string new_title, std::string new_description,
                               float dismiss_after )
{
    for ( auto& n : entries )
    {
        if ( n.id != id ) continue;
        n.prev_type = n.type;
        n.type = new_type;
        n.type_blend = 0.f;
        if ( !new_title.empty( ) ) n.title = std::move( new_title );
        if ( !new_description.empty( ) ) n.description = std::move( new_description );
        n.persistent = false;
        n.lifetime = 0.f;
        n.max_lifetime = dismiss_after;
        break;
    }
}

void c_notifications::dismiss( uint32_t id )
{
    for ( auto& n : entries )
        if ( n.id == id ) { n.dismissed = true; break; }
}

void c_notifications::clear( )
{
    for ( auto& n : entries )
        n.dismissed = true;
}

void c_notifications::render( )
{
    auto const& io = ImGui::GetIO( );
    float const dt = io.DeltaTime;
    float const base_x = io.DisplaySize.x - k_margin;
    float cursor_y = k_margin;

    auto* dl = ImGui::GetForegroundDrawList( );
    auto const& pal = c_themes::palette( );
    auto& title_font = core->fonts->get( "bold" );
    auto& desc_font = core->fonts->get( "primary" );
    auto& icon_font = core->fonts->get( "primary" );

    for ( auto& n : entries )
    {
        float const target = n.dismissed ? 0.f : 1.f;
        n.anim = smooth( n.anim, target, 6.f, dt );
        n.type_blend = smooth( n.type_blend, 1.f, 6.f, dt );

        if ( n.type == notification_type::loading )
            n.sweep_t = fmodf( n.sweep_t + 1.4f * dt, 1.f );

        if ( !n.persistent && !n.dismissed )
        {
            n.lifetime += dt;
            if ( n.lifetime >= n.max_lifetime )
                n.dismissed = true;
        }
    }

    for ( auto& n : entries )
    {
        if ( n.anim < 0.003f && n.dismissed )
            continue;

        float const h = compute_height( n );
        float const a = n.anim;
        float const off_x = ( k_width + k_margin + 10.f ) * ( 1.f - a );

        ImVec2 const mn = { base_x - k_width + off_x, cursor_y };
        ImVec2 const mx = { base_x + off_x, cursor_y + h };

        ImVec4 const tc = color_for_type( n.type );
        ImVec4 const pc = color_for_type( n.prev_type );
        ImVec4 const ac = ImLerp( pc, tc, n.type_blend );

        renderer_t::add_shadow( dl, mn, mx, color_t( pal.shadow ).modulate( a ), k_rounding, 16, ImDrawFlags_RoundCornersAll, 5, 3.f, 20 );
        core->blur->apply( dl, mn, mx, 14.f, k_rounding, 2, a );

        ImU32 const bg = color_t( with_alpha( pal.notification_bg, pal.notification_bg.w * a ) ).modulate( );
        ImU32 const border = color_t( with_alpha( pal.notification_border, pal.notification_border.w * a ) ).modulate( );

        dl->AddRectFilled( mn, mx, bg, k_rounding );
        dl->AddRect( mn, mx, border, k_rounding );

        if ( n.type == notification_type::loading )
        {
            float const w = mx.x - mn.x;
            float const perimeter = 2.f * ( w + h );
            float const head = n.sweep_t * perimeter;
            float const trail = perimeter * 0.28f;

            constexpr int k_segs = 80;
            float const seg = perimeter / ( float ) k_segs;

            auto point_on_rect = [ & ]( float d ) -> ImVec2 {
                d = fmodf( d, perimeter );
                if ( d < w ) return { mn.x + d, mn.y };
                d -= w;
                if ( d < h ) return { mx.x, mn.y + d };
                d -= h;
                if ( d < w ) return { mx.x - d, mx.y };
                d -= w;
                return { mn.x, mx.y - d };
            };

            for ( int i = 0; i < k_segs; i++ )
            {
                float const d = ( float ) i * seg;
                float const dist = fmodf( head - d + perimeter, perimeter );
                if ( dist > trail ) continue;

                float const intensity = 1.f - dist / trail;
                float const ia = intensity * intensity * a;

                ImVec2 const p0 = point_on_rect( d );
                ImVec2 const p1 = point_on_rect( d + seg );

                dl->AddLine( p0, p1, color_t( with_alpha( ac, 0.8f * ia ) ).modulate( ), 2.f );
            }
        }

        float const icon_y = mn.y + ( h - k_icon_area ) * 0.5f;
        ImVec2 const ib_mn = { mn.x + k_pad, icon_y };
        ImVec2 const ib_mx = { ib_mn.x + k_icon_area, icon_y + k_icon_area };

        dl->AddRectFilled( ib_mn, ib_mx, color_t( with_alpha( ac, 0.1f * a ) ).modulate( ), 6.f );

        if ( icon_font.ptr )
        {
            char const* ic = icon_for_type( n.type );
            ImVec2 const isz = icon_font.ptr->CalcTextSizeA( icon_font.size, FLT_MAX, -1, ic );
            ImVec2 const ipos = {
                ib_mn.x + ( k_icon_area - isz.x ) * 0.5f,
                ib_mn.y + ( k_icon_area - isz.y ) * 0.5f
            };
            ImU32 const icol = color_t( with_alpha( ac, a ) ).modulate( );

            if ( n.type == notification_type::loading )
            {
                float const angle = fmodf( ( float ) ImGui::GetTime( ) * 3.f, IM_PI * 2.f );
                ImVec2 const center = { ib_mn.x + k_icon_area * 0.5f, ib_mn.y + k_icon_area * 0.5f };

                int const vs = dl->VtxBuffer.Size;
                dl->AddText( icon_font.ptr, icon_font.size, ipos, icol, ic );
                int const ve = dl->VtxBuffer.Size;

                float const cs = cosf( angle ), sn = sinf( angle );
                for ( int v = vs; v < ve; v++ )
                {
                    float const dx = dl->VtxBuffer[ v ].pos.x - center.x;
                    float const dy = dl->VtxBuffer[ v ].pos.y - center.y;
                    dl->VtxBuffer[ v ].pos.x = center.x + dx * cs - dy * sn;
                    dl->VtxBuffer[ v ].pos.y = center.y + dx * sn + dy * cs;
                }
            }
            else
            {
                dl->AddText( icon_font.ptr, icon_font.size, ipos, icol, ic );
            }
        }

        float const tx = mn.x + k_pad + k_icon_area + k_text_gap;
        float ty = mn.y + k_pad;

        if ( title_font.ptr )
        {
            dl->AddText( title_font.ptr, title_font.size, { tx, ty },
                         color_t( with_alpha( pal.text_bright, a ) ).modulate( ),
                         n.title.c_str( ) );
            ty += title_font.ptr->CalcTextSizeA( title_font.size, FLT_MAX, -1, n.title.c_str( ) ).y + 3.f;
        }

        if ( !n.description.empty( ) && desc_font.ptr )
        {
            dl->AddText( desc_font.ptr, desc_font.size, { tx, ty },
                         color_t( with_alpha( pal.text_secondary, a ) ).modulate( ),
                         n.description.c_str( ), nullptr, k_desc_max );
        }

        cursor_y += ( h + k_gap ) * ( n.dismissed ? a : 1.f );
    }

    std::erase_if( entries, []( notification_t const& n ) {
        return n.dismissed && n.anim < 0.003f;
    } );
}
