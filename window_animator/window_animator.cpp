//
// Created by rei on 4/8/2026.
//

#include "window_animator.hpp"

#include "imgui_internal.h"

using alice::c_window_animator;

float c_window_animator::ease_out_cubic( float t )
{
    float const tm = 1.0f - t;
    return 1.0f - tm * tm * tm;
}

c_window_animator::c_window_animator( float duration, float step )
{
    m_duration = duration;
    m_step = step;
}

void c_window_animator::open( )
{
    m_open = true;
}

void c_window_animator::close( )
{
    m_open = false;
}

void c_window_animator::toggle( )
{
    m_open = !m_open;
}

void c_window_animator::update( float dt )
{
    if ( m_open )
        m_time = ImMin( m_time + dt, m_duration );
    else
        m_time = ImMax( m_time - dt, 0.0f );
}

bool c_window_animator::visible( ) const
{
    return m_time > 0.0f;
}

float c_window_animator::alpha( ) const
{
    return m_time / m_duration;
}

float c_window_animator::scale( ) const
{
    return ( 1.0f - m_step ) + m_step * ease_out_cubic( m_time / m_duration );
}

void c_window_animator::capture( ImDrawList* draw_list )
{
    m_snapshots.push_back( { draw_list, draw_list->VtxBuffer.Size } );
}

void c_window_animator::begin( )
{
    m_snapshots.clear( );
    ImGui::PushStyleVar( ImGuiStyleVar_Alpha, ImGui::GetStyle( ).Alpha * alpha( ) );
}

void c_window_animator::set_pivot( ImVec2 pivot )
{
    m_pivot = pivot;
}

void c_window_animator::end( ImVec2 window_pos, ImVec2 window_size )
{
    ImGui::PopStyleVar( );

    float const s = scale( );
    ImVec2 const center = {
        window_pos.x + window_size.x * m_pivot.x,
        window_pos.y + window_size.y * m_pivot.y };

    for ( auto& [ draw_list, vtx_start ] : m_snapshots )
    {
        auto* vtx = draw_list->VtxBuffer.Data;
        int const vtx_end = draw_list->VtxBuffer.Size;
        for ( int i = vtx_start; i < vtx_end; i++ )
        {
            vtx[ i ].pos.x = center.x + ( vtx[ i ].pos.x - center.x ) * s;
            vtx[ i ].pos.y = center.y + ( vtx[ i ].pos.y - center.y ) * s;
        }

        for ( auto& cmd : draw_list->CmdBuffer )
        {
            ImVec4& cr = cmd.ClipRect;
            cr.x = center.x + ( cr.x - center.x ) * s;
            cr.y = center.y + ( cr.y - center.y ) * s;
            cr.z = center.x + ( cr.z - center.x ) * s;
            cr.w = center.y + ( cr.w - center.y ) * s;
        }
    }
}