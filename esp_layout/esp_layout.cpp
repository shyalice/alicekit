//
// Created by rei on 4/8/2026.
//

#include "esp_layout.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace alice
{
    constexpr float k_lr_text_y_bias = 3.f;

    constexpr float k_anim_speed = 14.f;

    constexpr float k_snap_eps = 0.5f;

    float smooth_t( float dt, float speed = k_anim_speed )
    {
        return 1.f - std::exp( -speed * dt );
    }

    ImVec2 lerp( ImVec2 a, ImVec2 b, float t )
    {
        return { ImLerp( a.x, b.x, t ), ImLerp( a.y, b.y, t ) };
    }

    ImVec2 round_pos( ImVec2 v )
    {
        return { std::round( v.x ), std::round( v.y ) };
    }

    float perp_dist_from_box( esp_side side, ImRect const& bounds, ImVec2 p )
    {
        switch ( side )
        {
        case esp_side::top:
            return bounds.Min.y - p.y;
        case esp_side::bottom:
            return p.y - bounds.Max.y;
        case esp_side::left:
            return bounds.Min.x - p.x;
        case esp_side::right:
            return p.x - bounds.Max.x;
        }
        return 0.f;
    }

    c_esp_layout::c_esp_layout( )
    {
        for ( auto& p : m_padding )
        {
            p.box_to_bar = 2.f;
            p.between_bars = 1.f;
        }

        m_padding[ ( int ) esp_side::top ].box_to_text = 4.f;
        m_padding[ ( int ) esp_side::top ].bar_to_text = 2.f;
        m_padding[ ( int ) esp_side::top ].between_texts = 1.f;
        m_padding[ ( int ) esp_side::bottom ].box_to_text = 1.f;
        m_padding[ ( int ) esp_side::bottom ].bar_to_text = 0.f;
        m_padding[ ( int ) esp_side::bottom ].between_texts = 1.f;

        m_padding[ ( int ) esp_side::left ].box_to_text = 4.f;
        m_padding[ ( int ) esp_side::left ].bar_to_text = 3.f;
        m_padding[ ( int ) esp_side::left ].between_texts = 1.f;
        m_padding[ ( int ) esp_side::right ].box_to_text = 4.f;
        m_padding[ ( int ) esp_side::right ].bar_to_text = 3.f;
        m_padding[ ( int ) esp_side::right ].between_texts = 1.f;
    }

    void c_esp_layout::add_bar( int id, std::string name, esp_side side, int order,
                                esp_measure_fn measure, esp_render_fn render, esp_align align )
    {
        m_slots.push_back( esp_slot {
            .id = id,
            .name = std::move( name ),
            .side = side,
            .group = esp_group::bar,
            .order = order,
            .align = align,
            .measure = std::move( measure ),
            .render = std::move( render ) } );
    }

    void c_esp_layout::add_text( int id, std::string name, esp_side side, int order,
                                 esp_measure_fn measure, esp_render_fn render, esp_align align )
    {
        m_slots.push_back( esp_slot {
            .id = id,
            .name = std::move( name ),
            .side = side,
            .group = esp_group::text,
            .order = order,
            .align = align,
            .measure = std::move( measure ),
            .render = std::move( render ) } );
    }

    void c_esp_layout::add_box( int id, std::string name, esp_render_fn render )
    {
        // Box slot has zero measured size so it never contributes to side flow,
        // and is marked non-draggable so it can't be picked up or moved.
        esp_measure_fn measure = []( ImRect const&, esp_side, float, void* ) -> ImVec2 {
            return { 0.f, 0.f };
        };

        m_slots.push_back( esp_slot {
            .id = id,
            .name = std::move( name ),
            .side = esp_side::top,
            .group = esp_group::box,
            .order = 0,
            .align = esp_align::center,
            .measure = std::move( measure ),
            .render = std::move( render ),
            .enabled = true,
            .draggable = false } );
    }

    void c_esp_layout::set_slot_settings( int slot_id, esp_settings_fn fn )
    {
        if ( esp_slot* s = find_slot( slot_id ) )
            s->settings = std::move( fn );
    }

    esp_slot* c_esp_layout::find_slot( int id )
    {
        for ( auto& s : m_slots )
            if ( s.id == id )
                return &s;
        return nullptr;
    }

    void c_esp_layout::set_enabled( int slot_id, bool enabled )
    {
        if ( esp_slot* s = find_slot( slot_id ) )
            s->enabled = enabled;
    }

    bool c_esp_layout::is_enabled( int slot_id ) const
    {
        for ( auto const& s : m_slots )
            if ( s.id == slot_id )
                return s.enabled;
        return false;
    }

    bool c_esp_layout::move_slot( int slot_id, esp_side target_side, int target_index_in_group )
    {
        esp_slot* slot = find_slot( slot_id );
        if ( !slot )
            return false;

        esp_group const group = slot->group;
        esp_side const old_side = slot->side;
        int const old_order = slot->order;

        {
            std::vector< esp_slot* > peers;
            for ( auto& s : m_slots )
                if ( &s != slot && s.side == old_side && s.group == group )
                    peers.push_back( &s );
            std::sort( peers.begin( ), peers.end( ),
                       []( esp_slot* a, esp_slot* b ) { return a->order < b->order; } );
            for ( int i = 0; i < ( int ) peers.size( ); ++i )
                peers[ i ]->order = i;
        }

        slot->side = target_side;

        std::vector< esp_slot* > target_peers;
        for ( auto& s : m_slots )
            if ( &s != slot && s.side == target_side && s.group == group )
                target_peers.push_back( &s );
        std::sort( target_peers.begin( ), target_peers.end( ),
                   []( esp_slot* a, esp_slot* b ) { return a->order < b->order; } );

        int const clamped = std::clamp( target_index_in_group, 0, ( int ) target_peers.size( ) );
        for ( int i = 0; i < ( int ) target_peers.size( ); ++i )
            target_peers[ i ]->order = i < clamped ? i : i + 1;
        slot->order = clamped;

        return ( old_side != target_side ) || ( old_order != slot->order );
    }

    ImVec2 c_esp_layout::compute_cursor( esp_side side, ImRect const& bounds,
                                         float accumulated, ImVec2 slot_size, esp_align align ) const
    {
        auto along_h = [ & ]( float length ) {
            float x = bounds.Min.x;
            if ( align == esp_align::center )
                x = bounds.GetCenter( ).x - length / 2.f;
            else if ( align == esp_align::end )
                x = bounds.Max.x - length;
            return x;
        };
        auto along_v = [ & ]( float length ) {
            float y = bounds.Min.y;
            if ( align == esp_align::center )
                y = bounds.GetCenter( ).y - length / 2.f;
            else if ( align == esp_align::end )
                y = bounds.Max.y - length;
            return y;
        };

        ImVec2 r { };
        switch ( side )
        {
        case esp_side::left:
            r = { bounds.Min.x - accumulated - slot_size.x, along_v( slot_size.y ) };
            break;
        case esp_side::right:
            r = { bounds.Max.x + accumulated, along_v( slot_size.y ) };
            break;
        case esp_side::top:
            r = { along_h( slot_size.x ), bounds.Min.y - accumulated - slot_size.y };
            break;
        case esp_side::bottom:
            r = { along_h( slot_size.x ), bounds.Max.y + accumulated };
            break;
        }
        return round_pos( r );
    }

    void c_esp_layout::compute_layout( ImRect const& bounds, float alpha, void* user,
                                       std::vector< ImVec2 >& out_sizes,
                                       std::vector< ImVec2 >& out_positions ) const
    {
        out_sizes.assign( m_slots.size( ), { 0.f, 0.f } );
        out_positions.assign( m_slots.size( ), { 0.f, 0.f } );

        for ( auto side : { esp_side::top, esp_side::bottom, esp_side::left, esp_side::right } )
        {
            bool const horizontal_side = ( side == esp_side::top || side == esp_side::bottom );
            auto const& pad = m_padding[ ( int ) side ];

            std::vector< std::size_t > bars;
            std::vector< std::size_t > texts;
            for ( std::size_t i = 0; i < m_slots.size( ); ++i )
            {
                auto const& s = m_slots[ i ];
                if ( s.side != side )
                    continue;
                // box slots are rendered over bounds and never participate in
                // side-flow accumulation
                if ( s.group == esp_group::box )
                    continue;
                ( s.group == esp_group::bar ? bars : texts ).push_back( i );
            }
            auto by_order = [ & ]( std::size_t a, std::size_t b ) {
                return m_slots[ a ].order < m_slots[ b ].order;
            };
            std::sort( bars.begin( ), bars.end( ), by_order );
            std::sort( texts.begin( ), texts.end( ), by_order );

            float bar_offset = pad.box_to_bar;
            for ( auto i : bars )
            {
                auto const& s = m_slots[ i ];
                ImVec2 const sz = s.measure( bounds, side, alpha, user );
                ImVec2 const fs = { sz.x * s.vis_alpha, sz.y * s.vis_alpha };

                // bar position uses FULL sz so it never slides diagonally;
                // the visual reveal is handled inside the bar renderer via vis_alpha.
                // accumulator uses fs so disabling a bar lets following slots slide
                // toward the box.
                out_sizes[ i ] = sz;
                out_positions[ i ] = compute_cursor( side, bounds, bar_offset, sz, s.align );
                bar_offset += ( horizontal_side ? fs.y : fs.x ) + pad.between_bars * s.vis_alpha;
            }

            float const text_gap = bars.empty( )
                                       ? pad.box_to_text
                                       : bar_offset - pad.between_bars + pad.bar_to_text;

            if ( horizontal_side )
            {
                float text_offset = text_gap;
                for ( auto i : texts )
                {
                    auto const& s = m_slots[ i ];
                    ImVec2 const sz = s.measure( bounds, side, alpha, user );
                    ImVec2 const fs = { sz.x * s.vis_alpha, sz.y * s.vis_alpha };

                    out_sizes[ i ] = sz;
                    out_positions[ i ] = compute_cursor( side, bounds, text_offset, fs, esp_align::center );
                    text_offset += fs.y + pad.between_texts * s.vis_alpha;
                }
            } else
            {
                float y = bounds.Min.y - k_lr_text_y_bias;
                for ( auto i : texts )
                {
                    auto const& s = m_slots[ i ];
                    ImVec2 const sz = s.measure( bounds, side, alpha, user );
                    ImVec2 const fs = { sz.x * s.vis_alpha, sz.y * s.vis_alpha };

                    float const x = ( side == esp_side::left )
                                        ? bounds.Min.x - text_gap - sz.x
                                        : bounds.Max.x + text_gap;

                    out_sizes[ i ] = sz;
                    out_positions[ i ] = round_pos( { x, y } );
                    y += fs.y + pad.between_texts * s.vis_alpha;
                }
            }
        }
    }

    void c_esp_layout::hit_test_drop( ImRect const& bounds, ImVec2 mouse, esp_group drag_group,
                                      esp_side& out_side, int& out_index )
    {
        float const dl = mouse.x - bounds.Min.x;
        float const dr = bounds.Max.x - mouse.x;
        float const dt = mouse.y - bounds.Min.y;
        float const db = bounds.Max.y - mouse.y;
        float const min_d = ( std::min ) ( ( std::min ) ( dl, dr ), ( std::min ) ( dt, db ) );

        if ( min_d == dl )
            out_side = esp_side::left;
        else if ( min_d == dr )
            out_side = esp_side::right;
        else if ( min_d == dt )
            out_side = esp_side::top;
        else
            out_side = esp_side::bottom;

        std::vector< esp_slot* > peers;
        for ( auto& s : m_slots )
            if ( s.id != m_dragging_id && s.side == out_side && s.group == drag_group && s.enabled )
                peers.push_back( &s );
        std::sort( peers.begin( ), peers.end( ),
                   []( esp_slot* a, esp_slot* b ) { return a->order < b->order; } );

        int idx = 0;
        if ( drag_group == esp_group::bar )
        {
            float const cursor_d = perp_dist_from_box( out_side, bounds, mouse );
            for ( auto* p : peers )
            {
                ImVec2 const c { p->target_pos.x + p->size.x * 0.5f,
                                 p->target_pos.y + p->size.y * 0.5f };
                if ( cursor_d > perp_dist_from_box( out_side, bounds, c ) )
                    ++idx;
            }
        } else
        {
            bool const top_side = ( out_side == esp_side::top );
            for ( auto* p : peers )
            {
                float const cy = p->target_pos.y + p->size.y * 0.5f;
                if ( top_side ? ( cy > mouse.y ) : ( cy < mouse.y ) )
                    ++idx;
            }
        }
        out_index = idx;
    }

    void c_esp_layout::render( ImDrawList* dl, ImRect const& bounds, float alpha, void* user, bool interactive )
    {
        ImGuiIO& io = ImGui::GetIO( );
        float const dt = io.DeltaTime;
        float const t = smooth_t( dt );
        ImVec2 const mouse = io.MousePos;

        if ( m_has_last_bounds )
        {
            ImVec2 const delta { bounds.Min.x - m_last_bounds_origin.x,
                                 bounds.Min.y - m_last_bounds_origin.y };
            if ( delta.x != 0.f || delta.y != 0.f )
            {
                for ( auto& s : m_slots )
                {
                    if ( !s.anim_initialized )
                        continue;
                    if ( s.id == m_dragging_id )
                        continue;
                    s.anim_pos.x += delta.x;
                    s.anim_pos.y += delta.y;
                }
            }
        }
        m_last_bounds_origin = bounds.Min;
        m_has_last_bounds = true;

        for ( auto& s : m_slots )
            s.vis_alpha = ImLerp( s.vis_alpha, s.enabled ? 1.f : 0.f, t );

        if ( interactive )
        {
            if ( m_dragging_id >= 0 )
            {
                if ( esp_slot* dragged = find_slot( m_dragging_id ) )
                {
                    esp_side new_side = dragged->side;
                    int new_index = 0;
                    hit_test_drop( bounds, mouse, dragged->group, new_side, new_index );
                    move_slot( m_dragging_id, new_side, new_index );
                }
                m_dragging_over_delete = m_has_delete_zone && m_delete_zone.Contains( mouse );

                if ( !ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
                {
                    if ( m_dragging_over_delete )
                        if ( esp_slot* s = find_slot( m_dragging_id ) )
                            s->enabled = false;

                    m_dragging_id = -1;
                    m_drag_prev_size = { 0.f, 0.f };
                    m_dragging_over_delete = false;
                }
            } else
            {
                m_hovered_id = -1;
                for ( auto& s : m_slots )
                {
                    if ( !s.enabled || s.vis_alpha < 0.5f )
                        continue;
                    if ( !s.draggable )
                        continue;
                    ImRect rect { s.anim_pos, s.anim_pos + s.size };
                    rect.Expand( ImVec2( 2, 2 ) );
                    if ( rect.Contains( mouse ) )
                    {
                        m_hovered_id = s.id;
                        break;
                    }
                }
                if ( m_hovered_id >= 0 && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
                {
                    if ( esp_slot* s = find_slot( m_hovered_id ) )
                    {
                        m_dragging_id = s->id;
                        m_drag_grab_offset = mouse - s->anim_pos;
                        m_drag_prev_size = s->size;
                    }
                }
            }
        }

        std::vector< ImVec2 > sizes;
        std::vector< ImVec2 > positions;
        compute_layout( bounds, alpha, user, sizes, positions );

        for ( std::size_t i = 0; i < m_slots.size( ); ++i )
        {
            auto& s = m_slots[ i ];
            s.size = sizes[ i ];
            s.flow_size = { sizes[ i ].x * s.vis_alpha, sizes[ i ].y * s.vis_alpha };
            s.target_pos = positions[ i ];
        }

        if ( m_dragging_id >= 0 )
        {
            if ( esp_slot* dragged = find_slot( m_dragging_id ) )
            {
                ImVec2 const sz = dragged->size;
                bool const aspect_changed = ( sz.x > 0.f && sz.y > 0.f &&
                                              m_drag_prev_size.x > 0.f && m_drag_prev_size.y > 0.f &&
                                              ( ( sz.x > sz.y ) != ( m_drag_prev_size.x > m_drag_prev_size.y ) ) );
                bool const out_of_bounds = ( m_drag_grab_offset.x < 0.f || m_drag_grab_offset.x > sz.x ||
                                             m_drag_grab_offset.y < 0.f || m_drag_grab_offset.y > sz.y );
                if ( aspect_changed || out_of_bounds )
                    m_drag_grab_offset = { sz.x * 0.5f, sz.y * 0.5f };
                m_drag_prev_size = sz;
            }
        }

        for ( auto& s : m_slots )
        {
            float const target_hover = ( s.id == m_hovered_id || s.id == m_dragging_id ) ? 1.f : 0.f;
            s.hover_alpha = ImLerp( s.hover_alpha, target_hover, t );

            if ( !s.anim_initialized )
            {
                s.anim_pos = s.target_pos;
                s.anim_initialized = true;
                continue;
            }
            if ( s.id == m_dragging_id )
            {
                s.anim_pos = mouse - m_drag_grab_offset;
                continue;
            }

            s.anim_pos = lerp( s.anim_pos, s.target_pos, t );

            if ( std::abs( s.anim_pos.x - s.target_pos.x ) < k_snap_eps &&
                 std::abs( s.anim_pos.y - s.target_pos.y ) < k_snap_eps )
                s.anim_pos = s.target_pos;
        }

        for ( auto& s : m_slots )
        {
            if ( s.hover_alpha < 0.01f )
                continue;
            ImRect rect { round_pos( s.anim_pos ), round_pos( s.anim_pos ) + s.size };
            rect.Expand( ImVec2( 2, 2 ) );
            float const a = s.hover_alpha * 0.6f;
            ImU32 const fill = IM_COL32( 90, 160, 255, ( int ) ( 50 * a ) );
            ImU32 const edge = IM_COL32( 90, 160, 255, ( int ) ( 180 * a ) );
            dl->AddRectFilled( rect.Min, rect.Max, fill, 2.f );
            dl->AddRect( rect.Min, rect.Max, edge, 2.f );
        }

        esp_slot* dragged_render_last = nullptr;
        for ( auto& s : m_slots )
        {
            if ( s.vis_alpha < 0.01f )
                continue;
            if ( s.id == m_dragging_id )
            {
                dragged_render_last = &s;
                continue;
            }
            s.render( dl, bounds, s.side, round_pos( s.anim_pos ), alpha, s.vis_alpha, user );
        }
        if ( dragged_render_last )
        {
            ImRect const ghost { dragged_render_last->target_pos,
                                 dragged_render_last->target_pos + dragged_render_last->size };
            ImU32 const ghost_col = m_dragging_over_delete
                                        ? IM_COL32( 255, 80, 80, 200 )
                                        : IM_COL32( 255, 255, 255, 110 );
            dl->AddRect( ghost.Min - ImVec2( 1, 1 ), ghost.Max + ImVec2( 1, 1 ), ghost_col, 1.f );

            dragged_render_last->render( dl, bounds, dragged_render_last->side,
                                         round_pos( dragged_render_last->anim_pos ),
                                         alpha, dragged_render_last->vis_alpha, user );
        }
    }

    void c_esp_layout::render_static( ImDrawList* dl, ImRect const& bounds, float alpha, void* user ) const
    {
        std::vector< ImVec2 > sizes;
        std::vector< ImVec2 > positions;
        compute_layout( bounds, alpha, user, sizes, positions );

        for ( std::size_t i = 0; i < m_slots.size( ); ++i )
        {
            auto const& s = m_slots[ i ];
            if ( s.vis_alpha < 0.01f )
                continue;
            s.render( dl, bounds, s.side, round_pos( positions[ i ] ), alpha, s.vis_alpha, user );
        }
    }

} // namespace alice
