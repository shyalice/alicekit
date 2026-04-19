//
// Created by rei on 4/8/2026.
//

#include "ui.hpp"

#include "alice_kit.hpp"
#include "elem/elem.hpp"
#include "fonts/fonts.hpp"
#include "popups/popup_elem/popup_elem.hpp"
#include "renderer/renderer.hpp"
#include "../elem_animations/elem_animations.hpp"
#include "blur/blur.hpp"
#include "themes/themes.hpp"
#include "themes/theme_editor.hpp"
#include "util/util.hpp"
#include "windows/windows.hpp"
#include "notifications/notifications.hpp"

#include "fonts/lucide_defines.hpp"

#include "backends/imgui_impl_dx11.h"

#include <algorithm>
#include <string>

using alice::c_ui;
using alice::c_ui_subpage;
using alice::c_window_animator;
using alice::page_t;

c_ui::c_ui( ) = default;
c_ui::~c_ui( ) = default;

using namespace ImGui;

std::unordered_map< int, page_t > c_ui::page_data;

bool c_ui::render_tab( int idx, std::unique_ptr< c_ui_page >& page, ImVec2 const& size, bool active )
{
    auto const cur = GetCursorScreenPos( );
    ImRect const rect = { cur, cur + size };

    auto const result = InvisibleButton( page->name( ).c_str( ), size );
    auto const hovered = IsItemHovered( );
    auto const alpha = GetStyle( ).Alpha;
    auto const animation_time = 20.f * GetIO( ).DeltaTime;
    auto const& t = c_themes::palette( );

    auto& data = page_data[ idx ];
    if ( data.text.w < 0.01f )
    {
        data.text = t.text_muted;
        data.icon = t.text_muted;
    }
    data.background = ImLerp( data.background, active ? t.tab_bg : with_alpha( t.tab_bg, 0.f ), animation_time );
    data.outline = ImLerp( data.outline, active ? t.border : ( hovered ? with_alpha( t.border, 100.f / 255.f ) : with_alpha( t.border, 0.f ) ), animation_time );
    data.text = ImLerp( data.text, active ? t.text : ( hovered ? t.text_light : t.text_muted ), animation_time );
    data.icon = ImLerp( data.icon, active ? t.accent : ( hovered ? t.text_light : t.text_muted ), animation_time );

    auto* draw = GetWindowDrawList( );
    draw->AddRectFilled( rect.Min, rect.Max, color_t( data.background ).modulate( alpha ), 6 );
    draw->AddRect( rect.Min, rect.Max, color_t( data.outline ).modulate( alpha ), 6 );

    auto const font = core->fonts->get( "primary" );
    auto const title_size = font.calculate_text_size( page->name( ), 0 );
    auto const icon_size = font.calculate_text_size( page->icon( ), 0 );

    draw->AddText( { rect.Min.x + 10 + 1 /*idk lucide is kinda shit sooo*/, rect.GetCenter( ).y - icon_size.y / 2 }, color_t( data.icon ).modulate( alpha ), page->icon( ).c_str( ) );

    draw->AddText( { rect.Min.x + 10 + 10 + icon_size.x, rect.GetCenter( ).y - title_size.y / 2 }, color_t( data.text ).modulate( alpha ), page->name( ).c_str( ) );

    return result;
}

bool c_ui::render_subtab( c_ui_subpage* sub, ImVec2 const& size, bool active )
{
    auto const cur = GetCursorScreenPos( );
    ImRect const rect = { cur, cur + size };

    auto const result = InvisibleButton( sub->name( ).c_str( ), size );
    auto const hovered = IsItemHovered( );
    auto const alpha = GetStyle( ).Alpha;
    auto const animation_time = 20.f * GetIO( ).DeltaTime;

    auto const& t = c_themes::palette( );

    auto& data = sub->anim;
    if ( data.text.w < 0.01f )
    {
        data.text = t.text_muted;
        data.icon = t.text_muted;
    }
    data.text = ImLerp( data.text, active ? t.text : ( hovered ? t.text_light : t.text_muted ), animation_time );
    data.icon = ImLerp( data.icon, active ? t.accent : ( hovered ? t.text_light : t.text_muted ), animation_time );

    auto* draw = GetWindowDrawList( );

    auto const font = core->fonts->get( "primary" );
    auto const title_size = font.calculate_text_size( sub->name( ), 0 );
    auto const icon_str = sub->icon( );
    auto const has_icon = !icon_str.empty( );
    auto const icon_size = has_icon ? font.calculate_text_size( icon_str, 0 ) : ImVec2( 0, 0 );

    float const content_w = title_size.x + ( has_icon ? icon_size.x + 8.f : 0.f );
    float cx = rect.GetCenter( ).x - content_w * 0.5f;

    if ( has_icon )
    {
        draw->AddText( { cx, rect.GetCenter( ).y - icon_size.y / 2 }, color_t( data.icon ).modulate( alpha ), icon_str.c_str( ) );
        cx += icon_size.x + 8.f;
    }
    draw->AddText( { cx, rect.GetCenter( ).y - title_size.y / 2 }, color_t( data.text ).modulate( alpha ), sub->name( ).c_str( ) );

    return result;
}

bool c_ui::render_config_button( float height )
{
    auto const font = core->fonts->get( "primary" );
    auto const alpha = GetStyle( ).Alpha;
    auto const dt = 20.f * GetIO( ).DeltaTime;

    constexpr auto save_icon = ICON_LUCIDE_SAVE;
    constexpr auto arrow_icon = ICON_LUCIDE_CHEVRON_DOWN;

    auto const& configs = config_manager.get_configs( );
    std::string label = "Default";
    for ( auto const& cfg : configs )
    {
        if ( cfg.id == config_manager.selected_id( ) )
        {
            label = cfg.name;
            break;
        }
    }

    auto const save_sz = font.calculate_text_size( save_icon );
    auto const arrow_sz = font.calculate_text_size( arrow_icon );
    auto const ellipsis_sz = font.calculate_text_size( "..." );

    float constexpr pad = 8.f;
    float constexpr k_max_label_w = 100.f;
    float const save_w = save_sz.x + pad * 2;

    float const raw_label_w = font.calculate_text_size( label ).x;
    float const label_area_w = std::min( raw_label_w, k_max_label_w );

    std::string display_label = label;
    if ( raw_label_w > k_max_label_w )
    {
        display_label.clear( );
        for ( auto const ch : label )
        {
            display_label += ch;
            if ( font.calculate_text_size( display_label ).x + ellipsis_sz.x > k_max_label_w )
            {
                display_label += "...";
                break;
            }
        }
    }

    auto& a = config_button_anim;
    auto const& t = c_themes::palette( );

    auto const label_sz = font.calculate_text_size( display_label );
    float const target_select_w = label_area_w + arrow_sz.x + pad * 3;

    if ( a.smooth_w < 1.f )
        a.smooth_w = target_select_w;
    else
        a.smooth_w = ImLerp( a.smooth_w, target_select_w, dt );

    float const select_w = a.smooth_w;
    float const total_w = save_w + 1.f + select_w;

    auto const cur = GetCursorScreenPos( );
    ImRect const save_rect = { cur, { cur.x + save_w, cur.y + height } };
    ImRect const select_rect = { { cur.x + save_w + 1.f, cur.y }, { cur.x + total_w, cur.y + height } };

    InvisibleButton( "##config_save", { save_w, height } );
    bool const save_hovered = IsItemHovered( );
    bool const save_active = IsItemActive( );
    bool const save_clicked = IsItemClicked( );

    SameLine( 0, 1 );

    InvisibleButton( "##config_select", { select_w, height } );
    bool const select_hovered = IsItemHovered( );
    bool const select_active = IsItemActive( );
    bool const select_clicked = IsItemClicked( );

    auto const save_target_bg = save_active ? with_alpha( t.tab_bg, 1.4f )
                                : ( save_hovered ? with_alpha( t.tab_bg, 1.1f ) : t.tab_bg );
    auto const save_target_icon = save_active ? t.text : ( save_hovered ? t.text_secondary : t.text_muted );
    auto const select_target_bg = select_active ? with_alpha( t.tab_bg, 1.4f )
                                  : ( select_hovered ? with_alpha( t.tab_bg, 1.1f ) : t.tab_bg );
    auto const select_target_text = select_active ? t.text : ( select_hovered ? t.text_secondary : t.text_muted );
    auto const select_target_arrow = select_active ? t.text : ( select_hovered ? t.text_secondary : t.text_faint );

    if ( !a.initialized )
    {
        a.save_bg = save_target_bg;
        a.save_icon = save_target_icon;
        a.select_bg = select_target_bg;
        a.select_text = select_target_text;
        a.select_arrow = select_target_arrow;
        a.initialized = true;
    }

    a.save_bg = ImLerp( a.save_bg, save_target_bg, dt );
    a.save_icon = ImLerp( a.save_icon, save_target_icon, dt );
    a.select_bg = ImLerp( a.select_bg, select_target_bg, dt );
    a.select_text = ImLerp( a.select_text, select_target_text, dt );
    a.select_arrow = ImLerp( a.select_arrow, select_target_arrow, dt );

    auto* dl = GetWindowDrawList( );
    ImRect const full_rect = { save_rect.Min, select_rect.Max };

    dl->AddRectFilled( save_rect.Min, save_rect.Max, color_t( a.save_bg ).modulate( alpha ), 6, ImDrawFlags_RoundCornersLeft );
    dl->AddRectFilled( select_rect.Min, select_rect.Max, color_t( a.select_bg ).modulate( alpha ), 6, ImDrawFlags_RoundCornersRight );

    dl->AddRect( full_rect.Min, full_rect.Max, color_t( t.border_strong ).modulate( alpha ), 6 );

    dl->AddLine( { save_rect.Max.x, save_rect.Min.y + 4 },
                 { save_rect.Max.x, save_rect.Max.y - 4 },
                 color_t( t.border_hover ).modulate( alpha ) );

    dl->AddText( font.ptr, font.size,
                 { save_rect.GetCenter( ).x - save_sz.x * 0.5f + 1, save_rect.GetCenter( ).y - save_sz.y * 0.5f },
                 color_t( a.save_icon ).modulate( alpha ), save_icon );

    dl->AddText( font.ptr, font.size,
                 { select_rect.Min.x + pad, select_rect.GetCenter( ).y - label_sz.y * 0.5f },
                 color_t( a.select_text ).modulate( alpha ), display_label.c_str( ) );

    dl->AddText( font.ptr, font.size,
                 { select_rect.Max.x - pad - arrow_sz.x, select_rect.GetCenter( ).y - arrow_sz.y * 0.5f },
                 color_t( a.select_arrow ).modulate( alpha ), arrow_icon );

    if ( save_clicked && !config_manager.selected_id( ).empty( ) )
        config_manager.save_selected( );

    if ( select_clicked )
    {
        if ( config_manager.is_open( ) )
            config_manager.close( );
        else
            config_manager.open( { full_rect.GetCenter( ).x - 280.f / 2, full_rect.Max.y + 4.f } );
    }

    config_manager.render( );

    return select_clicked;
}

bool c_ui::render_action_button( int idx, std::string const& icon, ImVec2 const& size, ImVec4 const& accent )
{
    auto const cur = GetCursorScreenPos( );
    ImRect const rect = { cur, cur + size };

    auto const id = std::string( "##action_" ) + std::to_string( idx );
    auto const result = InvisibleButton( id.c_str( ), size );
    auto const hovered = IsItemHovered( );
    auto const alpha = GetStyle( ).Alpha;
    auto const animation_time = 20.f * GetIO( ).DeltaTime;

    auto const& t = c_themes::palette( );

    auto& data = action_buttons[ idx ];
    data.background = ImLerp( data.background, hovered ? t.tab_bg : with_alpha( t.tab_bg, 0.f ), animation_time );
    data.outline = ImLerp( data.outline, hovered ? t.border : with_alpha( t.border, 0.f ), animation_time );
    data.icon = ImLerp( data.icon, hovered ? accent : t.text_muted, animation_time );

    auto* draw = GetWindowDrawList( );
    draw->AddRectFilled( rect.Min, rect.Max, color_t( data.background ).modulate( alpha ), 6 );

    auto const font = core->fonts->get( "primary" );
    auto const icon_size = font.calculate_text_size( icon, 0 );
    draw->AddText( { rect.GetCenter( ).x - icon_size.x / 2, rect.GetCenter( ).y - icon_size.y / 2 }, color_t( data.icon ).modulate( alpha ), icon.c_str( ) );

    return result;
}

void c_ui::set_search_context( int page_idx, int subpage_idx )
{
    search_ctx_page = page_idx;
    search_ctx_subpage = subpage_idx;
}

void c_ui::try_register_search( std::string const& label )
{
    if ( search_ctx_page < 0 || search_ctx_page >= static_cast< int >( pages.size( ) ) )
        return;

    auto const key = label + "\0" + std::to_string( search_ctx_page ) + "\0" + std::to_string( search_ctx_subpage );
    if ( search_registered_keys.contains( key ) )
        return;

    search_registered_keys.insert( key );

    auto const& page = pages[ search_ctx_page ];
    std::string path = page->name( );
    if ( search_ctx_subpage >= 0 && search_ctx_subpage < static_cast< int >( page->subpages.size( ) ) )
        path += " > " + page->subpages[ search_ctx_subpage ]->name( );

    search_registry.push_back( { label, std::move( path ), search_ctx_page, search_ctx_subpage } );
}

bool c_ui::is_indexing( ) const
{
    return search_indexing;
}

void c_ui::build_search_index( )
{
    search_registry.clear( );
    search_registered_keys.clear( );
    search_indexing = true;

    PushStyleVar( ImGuiStyleVar_Alpha, 0.f );
    SetNextWindowPos( { -10000, -10000 } );
    SetNextWindowSize( window_size );
    Begin( "##search_index", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav );

    for ( int p = 0; p < static_cast< int >( pages.size( ) ); p++ )
    {
        auto& page = pages[ p ];

        if ( page->subpages.empty( ) )
        {
            BeginChild( ( "##si_p" + std::to_string( p ) ).c_str( ), { GetContentRegionAvail( ).x, 99999 }, 0, ImGuiWindowFlags_NoScrollbar );
            set_search_context( p, -1 );
            page->render( );
            EndChild( );
        } else
        {
            for ( int s = 0; s < static_cast< int >( page->subpages.size( ) ); s++ )
            {
                BeginChild( ( "##si_s" + std::to_string( p ) + "_" + std::to_string( s ) ).c_str( ), { GetContentRegionAvail( ).x, 99999 }, 0, ImGuiWindowFlags_NoScrollbar );
                set_search_context( p, s );
                page->subpages[ s ]->render( );
                EndChild( );
            }
        }
    }

    set_search_context( -1, -1 );
    End( );
    PopStyleVar( );
    search_indexing = false;
}

void c_ui::update_search_results( )
{
    search_results.clear( );

    std::string q( search_query );
    std::transform( q.begin( ), q.end( ), q.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

    for ( int i = 0; i < static_cast< int >( search_registry.size( ) ); i++ )
    {
        std::string label = search_registry[ i ].label;
        std::transform( label.begin( ), label.end( ), label.begin( ), []( unsigned char c ) { return std::tolower( c ); } );

        if ( q.empty( ) || label.find( q ) != std::string::npos )
            search_results.push_back( i );
    }

    if ( search_selected >= static_cast< int >( search_results.size( ) ) )
        search_selected = std::max( 0, static_cast< int >( search_results.size( ) ) - 1 );
}

void c_ui::navigate_to( int page_idx, int subpage_idx )
{
    if ( page_idx >= 0 && page_idx < static_cast< int >( pages.size( ) ) )
    {
        selected_page_idx = page_idx;
        if ( subpage_idx >= 0 )
            pages[ page_idx ]->selected_subpage_idx = subpage_idx;
    }
    search_open = false;
}

void c_ui::render_search_overlay( ImRect const& parent_rect, float overlay_alpha )
{
    constexpr float k_width = 450.f;
    constexpr float k_input_h = 44.f;
    constexpr float k_result_h = 48.f;
    constexpr float k_pad = 10.f;
    constexpr float k_rounding = 10.f;

    auto const dt = GetIO( ).DeltaTime;
    auto const anim_speed = 20.f * dt;
    auto const font = core->fonts->get( "primary" );
    auto* parent_draw = GetWindowDrawList( );
    auto const& t = c_themes::palette( );

    ImVec2 const pos = { parent_rect.GetCenter( ).x - k_width / 2, parent_rect.Min.y + 80 };

    auto const icon_str = std::string( ICON_LUCIDE_SEARCH );
    auto const icon_size = font.calculate_text_size( icon_str, 0 );
    float const text_x = pos.x + k_pad + 2 + icon_size.x + 10.f;
    float const input_w = k_width - ( text_x - pos.x ) - k_pad;

    parent_draw->ChannelsSplit( 2 );

    parent_draw->ChannelsSetCurrent( 1 );
    {
        SetCursorScreenPos( { text_x, pos.y } );
        PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_Text, with_alpha( t.text, overlay_alpha ) );
        PushStyleColor( ImGuiCol_TextSelectedBg, with_alpha( t.accent, 0.3f ) );
        PushStyleVar( ImGuiStyleVar_FramePadding, { 0, ( k_input_h - GetFontSize( ) ) / 2 } );
        PushItemWidth( input_w );

        if ( search_focus_pending )
        {
            SetKeyboardFocusHere( );
            search_focus_pending = false;
        }

        if ( InputText( "##search_input", search_query, sizeof( search_query ) ) )
        {
            update_search_results( );
            search_selected = 0;
        }

        PopItemWidth( );
        PopStyleVar( );
        PopStyleColor( 3 );
    }

    // counts are now safe
    int const result_count = static_cast< int >( search_results.size( ) );
    search_selected = ImClamp( search_selected, 0, std::max( result_count - 1, 0 ) );

    if ( static_cast< int >( search_result_anims.size( ) ) < result_count )
        search_result_anims.resize( result_count );

    constexpr int k_max_visible = 7;
    int const clamped_count = std::min( result_count, k_max_visible );
    float const results_area_h = clamped_count > 0 ? 6.f + ( k_result_h * clamped_count ) + 6.f : 0.f;
    float const target_h = k_input_h + results_area_h;
    if ( search_height_anim < 1.f && search_open )
        search_height_anim = k_input_h;
    search_height_anim = ImLerp( search_height_anim, target_h, anim_speed );

    ImVec2 const size = { k_width, search_height_anim };
    ImRect const box = { pos, pos + size };

    // ch0: background, icon, placeholder
    parent_draw->ChannelsSetCurrent( 0 );
    parent_draw->PushClipRectFullScreen( );

    core->blur->apply_squircle( parent_draw, box.Min, box.Max, 18, k_rounding, 5, 3.f, 2, overlay_alpha );

    renderer_t::add_rect_filled( parent_draw, box.Min, box.Max, color_t( t.header_bg ).modulate( overlay_alpha ), k_rounding, ImDrawFlags_RoundCornersAll, 5, 3.f );
    renderer_t::add_rect( parent_draw, box.Min, box.Max, color_t( t.border_strong ).modulate( overlay_alpha ), 1, k_rounding, ImDrawFlags_RoundCornersAll, 5, 3.f );

    parent_draw->AddText( { box.Min.x + k_pad + 2, box.Min.y + k_input_h / 2 - icon_size.y / 2 },
                          color_t( t.text_dim ).modulate( overlay_alpha ), icon_str.c_str( ) );

    if ( strlen( search_query ) == 0 )
    {
        auto const hint_size = font.calculate_text_size( "Search...", 0 );
        parent_draw->AddText( { text_x, box.Min.y + k_input_h / 2 - hint_size.y / 2 },
                              color_t( t.text_placeholder ).modulate( overlay_alpha ), "Search..." );
    }

    parent_draw->PopClipRect( );
    parent_draw->ChannelsMerge( );

    // keyboard nav
    if ( IsKeyPressed( ImGuiKey_DownArrow ) && search_selected < result_count - 1 )
        search_selected++;
    if ( IsKeyPressed( ImGuiKey_UpArrow ) && search_selected > 0 )
        search_selected--;
    if ( IsKeyPressed( ImGuiKey_Enter ) && result_count > 0 )
    {
        auto const& entry = search_registry[ search_results[ search_selected ] ];
        navigate_to( entry.page_idx, entry.subpage_idx );
    }
    if ( IsKeyPressed( ImGuiKey_Escape ) )
        search_open = false;
    if ( IsMouseClicked( 0 ) && !IsMouseHoveringRect( box.Min, box.Max ) )
        search_open = false;

    // results in scrollable child
    if ( result_count > 0 )
    {
        parent_draw->PushClipRectFullScreen( );
        parent_draw->AddLine( { box.Min.x + k_pad, box.Min.y + k_input_h },
                              { box.Max.x - k_pad, box.Min.y + k_input_h },
                              color_t( t.divider ).modulate( overlay_alpha ) );
        parent_draw->PopClipRect( );

        SetCursorScreenPos( { box.Min.x, box.Min.y + k_input_h + 6.f } );
        PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
        BeginChild( "##search_results", { k_width, results_area_h - 6.f }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground );
        {
            window_animator.capture( GetWindowDrawList( ) );
            auto* results_window = GetCurrentWindow( );
            auto* draw = GetWindowDrawList( );

            for ( int i = 0; i < result_count; i++ )
            {
                auto const& entry = search_registry[ search_results[ i ] ];
                bool const is_selected = i == search_selected;

                ImVec2 const item_min = GetCursorScreenPos( );
                ImRect const rr = { { item_min.x + 6, item_min.y }, { item_min.x + k_width - 6, item_min.y + k_result_h - 4 } };

                ItemSize( { k_width, k_result_h } );
                bool const hovered_result = IsMouseHoveringRect( rr.Min, rr.Max );

                auto& ra = search_result_anims[ i ];
                ra.background = ImLerp( ra.background,
                                        is_selected ? t.control_hover : ( hovered_result ? t.control_subtle : with_alpha( t.header_bg, 0.f ) ),
                                        anim_speed );

                draw->AddRectFilled( rr.Min, rr.Max, color_t( ra.background ).modulate( overlay_alpha ), 6 );

                auto const label_size = font.calculate_text_size( entry.label, 0 );
                auto const path_size = font.calculate_text_size( entry.path, 0 );
                float const block_h = label_size.y + 2.f + path_size.y;
                float const ty = rr.GetCenter( ).y - block_h / 2;

                draw->AddText( { rr.Min.x + 10, ty },
                               color_t( is_selected ? t.text : t.text_secondary ).modulate( overlay_alpha ),
                               entry.label.c_str( ) );

                draw->AddText( { rr.Min.x + 10, ty + label_size.y + 2.f },
                               color_t( t.text_dim ).modulate( overlay_alpha ),
                               entry.path.c_str( ) );

                if ( hovered_result )
                    search_selected = i;
                if ( hovered_result && IsMouseClicked( 0 ) )
                    navigate_to( entry.page_idx, entry.subpage_idx );
            }

            // scroll selected into view
            if ( IsKeyPressed( ImGuiKey_DownArrow ) || IsKeyPressed( ImGuiKey_UpArrow ) )
            {
                float const item_top = search_selected * k_result_h;
                float const item_bottom = item_top + k_result_h;
                if ( item_bottom > results_window->Scroll.y + results_window->InnerRect.GetHeight( ) )
                    SetScrollY( item_bottom - results_window->InnerRect.GetHeight( ) );
                if ( item_top < results_window->Scroll.y )
                    SetScrollY( item_top );
            }

            EndChild( );
        }
        PopStyleVar( );
    }
}

c_window_animator* c_ui::get_window_animator( )
{
    return &window_animator;
}

void c_ui::initialize( )
{
    c_themes::initialize( );
    auto& style = GetStyle( );

    style.Colors[ ImGuiCol_WindowBg ]    = ImVec4( 0, 0, 0, 0 );
    style.Colors[ ImGuiCol_ChildBg ]     = ImVec4( 0, 0, 0, 0 );
    style.Colors[ ImGuiCol_PopupBg ]     = ImVec4( 0, 0, 0, 0 );
    style.Colors[ ImGuiCol_Border ]      = ImVec4( 0, 0, 0, 0 );
    style.Colors[ ImGuiCol_FrameBg ]     = ImVec4( 0, 0, 0, 0 );
    style.Colors[ ImGuiCol_TitleBg ]     = ImVec4( 0, 0, 0, 0 );

    style.WindowShadowSize = 0;
    style.WindowMinSize = { 1, 1 };
    style.Scale = 1;
    c_themes::set( 0 );
    c_ui_elements::prepare( );
}

void c_ui::render( )
{
    c_elem_animations::instance( ).tick( );
    core->windows->update( GetIO( ).DeltaTime );

    c_theme_editor::render( );

    if ( core->windows->begin( "settings", "Settings", { 300, 400 } ) )
    {
        auto* settings_anim = core->windows->current_animator( );
        core->ui->get_window_animator( )->capture( GetWindowDrawList( ) );

        auto const& t = c_themes::palette( );
        auto const font = core->fonts->get( "primary" );
        auto const sa = GetStyle( ).Alpha;

        Image( core->assets->get( "banner" ), { GetContentRegionAvail( ).x, GetContentRegionAvail( ).x / 4 } );

        c_ui_elements::begin_group( "THEME", { GetContentRegionAvail( ).x, 99999.f } );
        if ( settings_anim )
            settings_anim->capture( GetWindowDrawList( ) );

        {
            auto const preview = truncate_text( font.ptr, font.size, c_themes::name( c_themes::index( ) ), 100.f );
            auto elem = c_ui_elements::start( "Theme" );

            ImRect const dd_rect = {
                { elem.rect.Max.x - 120, elem.rect.GetCenter( ).y - 11 },
                { elem.rect.Max.x, elem.rect.GetCenter( ).y + 11 } };

            ImGuiID const dd_id = GetCurrentWindow( )->GetID( "##theme_dd" );

            c_elem_animations::instance( ).insert_if_empty< dropdown_info_t >(
                GetCurrentWindow( )->Name, dd_id, dropdown_info_t { .random_title = "theme_sel_dd" } );

            auto* dd_info = c_elem_animations::instance( ).get< dropdown_info_t >( GetCurrentWindow( )->Name, dd_id );
            dd_info->popup.set_animate_layout( true );

            if ( std_elem::begin_dropdown( dd_id, dd_rect, dd_info, preview, c_themes::count( ) + 1 ) )
            {
                int action = -1;
                int action_idx = -1;
                float const anim_dt = 20.f * GetIO( ).DeltaTime;

                struct row_anim_t
                {
                    ImVec4 text_col { };
                    ImVec4 pencil { };
                    ImVec4 trash { };
                };
                static std::unordered_map< int, row_anim_t > row_anims;

                constexpr float k_row_h = 28.f;
                constexpr float k_pad = 8.f;

                for ( int i = 0; i < c_themes::count( ); ++i )
                {
                    PushID( i );

                    bool const active = i == c_themes::index( );
                    bool const custom = !c_themes::is_builtin( i );
                    float const icons_w = custom ? 44.f : 0.f;

                    ImVec2 const pos = GetCursorScreenPos( );
                    ImVec2 const row_size = { GetContentRegionAvail( ).x, k_row_h };
                    ImRect const bb = { pos, pos + row_size };

                    InvisibleButton( "##row", row_size );
                    bool const hovered = IsItemHovered( );
                    bool const clicked = IsItemClicked( );

                    auto& ra = row_anims[ i ];
                    ra.text_col = ImLerp( ra.text_col, active ? t.accent : ( hovered ? t.text : t.text_bright ), anim_dt );

                    auto* row_dl = GetWindowDrawList( );
                    row_dl->AddRectFilled( bb.Min, bb.Max, color_t( hovered ? t.overlay_hover : with_alpha( t.overlay_hover, 0.f ) ).modulate( sa ), 6 );

                    auto const entry_name = truncate_text( font.ptr, font.size, c_themes::name( i ), row_size.x - icons_w - k_pad * 2 );
                    auto const name_sz = font.calculate_text_size( entry_name );
                    row_dl->AddText( { bb.Min.x + k_pad, bb.GetCenter( ).y - name_sz.y / 2 },
                                     color_t( ra.text_col ).modulate( sa ), entry_name.c_str( ) );

                    if ( custom )
                    {
                        float const cy = bb.GetCenter( ).y;
                        auto const p_sz = font.calculate_text_size( ICON_LUCIDE_PENCIL );
                        auto const t_sz = font.calculate_text_size( ICON_LUCIDE_TRASH_2 );

                        ImVec2 const t_pos = { bb.Max.x - k_pad - t_sz.x, cy - t_sz.y / 2 };
                        ImVec2 const p_pos = { t_pos.x - 6 - p_sz.x, cy - p_sz.y / 2 };
                        ImRect const p_hit = { p_pos - ImVec2( 3, 3 ), p_pos + p_sz + ImVec2( 3, 3 ) };
                        ImRect const t_hit = { t_pos - ImVec2( 3, 3 ), t_pos + t_sz + ImVec2( 3, 3 ) };

                        bool const p_hov = hovered && p_hit.Contains( GetIO( ).MousePos );
                        bool const t_hov = hovered && t_hit.Contains( GetIO( ).MousePos );

                        ra.pencil = ImLerp( ra.pencil, p_hov ? t.accent : t.text_faint, anim_dt );
                        ra.trash = ImLerp( ra.trash, t_hov ? t.danger : t.text_faint, anim_dt );

                        row_dl->AddText( p_pos, color_t( ra.pencil ).modulate( sa ), ICON_LUCIDE_PENCIL );
                        row_dl->AddText( t_pos, color_t( ra.trash ).modulate( sa ), ICON_LUCIDE_TRASH_2 );

                        if ( p_hov && IsMouseClicked( 0 ) )
                        {
                            action = 1;
                            action_idx = i;
                        } else if ( t_hov && IsMouseClicked( 0 ) )
                        {
                            action = 2;
                            action_idx = i;
                        } else if ( clicked && !p_hov && !t_hov )
                        {
                            c_themes::set( i );
                        }
                    } else if ( clicked )
                    {
                        c_themes::set( i );
                    }

                    PopID( );
                }

                if ( c_popup_elem::button( ICON_LUCIDE_PLUS, "Create new" ) )
                    action = 0;

                dd_info->popup.end( );

                if ( action == 0 )
                {
                    theme_palette_t copy = c_themes::palette( );
                    copy.name = c_theme_editor::generate_name( );
                    copy.builtin = false;
                    c_themes::add( std::move( copy ) );
                    int const new_idx = c_themes::count( ) - 1;
                    c_themes::set( new_idx );
                    c_theme_editor::open( new_idx );
                } else if ( action == 1 && action_idx >= 0 )
                {
                    c_themes::set( action_idx );
                    c_theme_editor::open( action_idx );
                } else if ( action == 2 && action_idx >= 0 )
                {
                    if ( c_theme_editor::is_open( ) && c_theme_editor::editing( ) == action_idx )
                        c_theme_editor::cancel( );
                    c_themes::remove( action_idx );
                }
            }

            elem.finish( );
        }

        {
            static ImVec4 prev_accent { };
            auto& palette = c_themes::palette_mut( );
            c_ui_elements::start( "Accent" ).add_color( &palette.accent ).finish( );

            if ( prev_accent.x != palette.accent.x || prev_accent.y != palette.accent.y ||
                 prev_accent.z != palette.accent.z )
            {
                palette.derive_accent_from( palette.accent );
                if ( !c_themes::is_builtin( c_themes::index( ) ) )
                    c_themes::save_theme( c_themes::index( ) );
            }
            prev_accent = palette.accent;
        }

        c_ui_elements::end_group( );

        core->windows->end( );
    }

    if ( !search_index_built && !pages.empty( ) )
    {
        build_search_index( );
        search_index_built = true;
    }

    presented ? window_animator.open( ) : window_animator.close( );
    window_animator.update( GetIO( ).DeltaTime );

    if ( !window_animator.visible( ) || pages.empty( ) )
    {
        c_notifications::render( );
        return;
    }

    window_animator.begin( );

    PushStyleVar( ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f } );

    SetNextWindowSize( window_size );
    SetNextWindowPos( GetIO( ).DisplaySize / 2 - window_size / 2, ImGuiCond_Once );
    Begin( "##everness2.0", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus );
    {
        auto* window = GetCurrentWindow( );
        auto* draw = window->DrawList;
        auto const alpha = window_animator.alpha( );
        auto const rect = window->Rect( );

        window_animator.capture( draw );

        float const tabs_height = 60.f;
        float const subtabs_height_max = 44.f;

        bool const current_has_subs = !pages[ display_page_idx ]->subpages.empty( );
        subtabs_anim = ImLerp( subtabs_anim, current_has_subs ? 1.f : 0.f, 20.f * GetIO( ).DeltaTime );
        float const subtabs_height = subtabs_height_max * subtabs_anim;

        auto const& t = c_themes::palette( );

        draw->PushClipRectFullScreen( );
        {
            float const s_scale = window_animator.scale( );
            ImVec2 const c = ( rect.Min + rect.Max ) * 0.5f;
            ImVec2 const sm = c + ( rect.Min - c ) * s_scale;
            ImVec2 const sx = c + ( rect.Max - c ) * s_scale;
            core->blur->apply_squircle( draw, sm, sx, 18, 12, 5, 3.f, 2, alpha );
        }

        renderer_t::add_rect_filled( draw, rect.Min, rect.Max, color_t( t.window_bg ).modulate( alpha ), 12, ImDrawFlags_RoundCornersAll, 5, 3.f );

        renderer_t::add_rect_filled( draw, rect.Min, rect.Min + ImVec2( rect.GetWidth( ), tabs_height + subtabs_height ), color_t( t.header_bg ).modulate( alpha ), 12, ImDrawFlags_RoundCornersTop, 5, 3.f );

        renderer_t::add_rect_filled( draw, rect.Min + ImVec2( 0, tabs_height + subtabs_height ), rect.Max, color_t( t.content_bg ).modulate( alpha ), 12, ImDrawFlags_RoundCornersBottom, 5, 3.f );

        window->DrawList->AddLine( { rect.Min.x, rect.Min.y + tabs_height }, { rect.Max.x, rect.Min.y + tabs_height }, color_t( t.border_subtle ).modulate( alpha ) );
        window->DrawList->AddLine( { rect.Min.x, rect.Min.y + tabs_height + subtabs_height }, { rect.Max.x, rect.Min.y + tabs_height + subtabs_height }, color_t( t.border_subtle ).modulate( alpha ) );

        renderer_t::add_rect( draw, rect.Min, rect.Max, color_t( t.border_subtle ).modulate( alpha ), 1, 12, ImDrawFlags_RoundCornersAll, 5, 3.f );
        draw->PopClipRect( );

        SetCursorPos( { 0, 0 } );
        BeginChild( "##tabs", { GetContentRegionAvail( ).x, tabs_height }, 0 );
        {
            window_animator.capture( GetWindowDrawList( ) );

            window->DrawList->AddCallback( ImGui_ImplDX11_UsePointSampling, nullptr );
            window->DrawList->AddImage( core->assets->get( "logo" ), GetWindowPos( ) + ImVec2( 10, 2 ), GetWindowPos( ) + ImVec2( 10 + tabs_height - 2 * 2, tabs_height - 2 ), { 0, 0 }, { 1, 1 }, color_t( 1.f, 1.f, 1.f ).modulate( alpha ) );
            window->DrawList->AddCallback( ImDrawCallback_ResetRenderState, nullptr );

            // tabs
            {
                SetCursorPos( { 15 + tabs_height, 15 } );

                for ( int it { 0 }; it < pages.size( ); it++ )
                {
                    auto& page = pages[ it ];

                    auto const font = core->fonts->get( "primary" );
                    auto const title_size = font.calculate_text_size( page->name( ), 0 );
                    auto const icon_size = font.calculate_text_size( page->icon( ), 0 );

                    if ( render_tab( it, page, { 30 + title_size.x + icon_size.y, tabs_height - 30 }, it == selected_page_idx ) )
                        selected_page_idx = it;

                    SameLine( 0, 10 );
                }
            }

            // action buttons (3 buttons pinned to right, config button before them)
            {
                float const btn_size = tabs_height - 30;
                float const btn_spacing = 6.f;
                float const right_pad = 15.f;

                float const three_w = btn_size * 3 + btn_spacing * 2;
                float const three_x = GetWindowWidth( ) - right_pad - three_w;

                auto const cfg_font = core->fonts->get( "primary" );
                float const cfg_save_w = cfg_font.calculate_text_size( ICON_LUCIDE_SAVE ).x + 16.f;
                float const cfg_sel_w = config_button_anim.smooth_w > 1.f ? config_button_anim.smooth_w : 80.f;
                float const cfg_w = cfg_save_w + 1.f + cfg_sel_w;

                SetCursorPos( { three_x - btn_spacing - cfg_w, 15 } );
                render_config_button( btn_size );

                SetCursorPos( { three_x, 15 } );

                if ( render_action_button( 0, ICON_LUCIDE_SEARCH, { btn_size, btn_size }, t.text_secondary ) )
                {
                    search_open = !search_open;
                    if ( search_open )
                    {
                        memset( search_query, 0, sizeof( search_query ) );
                        search_selected = 0;
                        search_focus_pending = true;
                        update_search_results( );
                    }
                }
                SameLine( 0, btn_spacing );

                if ( render_action_button( 1, ICON_LUCIDE_SETTINGS, { btn_size, btn_size }, t.text_secondary ) )
                    core->windows->toggle( "settings", { GetIO( ).DisplaySize / 2 }, { 0.5f, 0.5f } );

                SameLine( 0, btn_spacing );

                if ( render_action_button( 2, ICON_LUCIDE_X, { btn_size, btn_size }, t.danger_subtle ) )
                    presented = false;
            }

            EndChild( );
        }

        float const dt = GetIO( ).DeltaTime;
        float const fade_speed = 20.f * dt;

        if ( display_page_idx != selected_page_idx )
        {
            page_switch_t = ImLerp( page_switch_t, 0.f, fade_speed );
            if ( page_switch_t < 0.02f )
            {
                page_switch_t = 0.f;
                display_page_idx = selected_page_idx;
            }
        } else
        {
            page_switch_t = ImLerp( page_switch_t, 1.f, fade_speed );
        }

        auto& active_page = pages[ display_page_idx ];
        auto const has_subpages = !active_page->subpages.empty( );
        float const page_t = ImClamp( page_switch_t, 0.f, 1.f );
        float const page_offset = 40.f * ( 1.f - page_t );

        SetCursorPos( { 0, tabs_height + 1 } );

        BeginChild( "##subtabs", { GetContentRegionAvail( ).x, subtabs_height }, 0 );
        {
            window_animator.capture( GetWindowDrawList( ) );

            if ( has_subpages )
            {
                PushStyleVar( ImGuiStyleVar_Alpha, GetStyle( ).Alpha * page_t );

                auto const animation_time = 20.f * dt;
                auto const font = core->fonts->get( "primary" );
                auto const win_pos = GetWindowPos( );

                SetCursorPos( { 15, ( subtabs_height - 28 ) / 2 } );

                float target_x = active_page->indicator_x;
                float target_w = active_page->indicator_w;

                for ( int it { 0 }; it < static_cast< int >( active_page->subpages.size( ) ); it++ )
                {
                    auto* sub = active_page->subpages[ it ].get( );

                    auto const title_size = font.calculate_text_size( sub->name( ), 0 );
                    auto const icon_str = sub->icon( );
                    auto const icon_size = icon_str.empty( ) ? ImVec2( 0, 0 ) : font.calculate_text_size( icon_str, 0 );
                    float const btn_w = title_size.x + ( icon_str.empty( ) ? 0.f : icon_size.x + 8.f ) + 24.f;

                    auto const pre_cursor = GetCursorScreenPos( );
                    float const local_x = pre_cursor.x - win_pos.x;
                    bool const is_target = it == active_page->selected_subpage_idx;

                    if ( render_subtab( sub, { btn_w, 28 }, is_target ) && it != active_page->selected_subpage_idx )
                        active_page->selected_subpage_idx = it;

                    if ( is_target )
                    {
                        target_x = local_x;
                        target_w = btn_w;
                    }

                    SameLine( 0, 6 );
                }

                if ( active_page->indicator_x < 0.f )
                {
                    active_page->indicator_x = target_x;
                    active_page->indicator_w = target_w;
                } else
                {
                    active_page->indicator_x = ImLerp( active_page->indicator_x, target_x, animation_time );
                    active_page->indicator_w = ImLerp( active_page->indicator_w, target_w, animation_time );
                }

                float const strip_local_y = ( subtabs_height - 28 ) / 2 + 28 + 6;
                auto const alpha = window_animator.alpha( ) * page_t;
                GetWindowDrawList( )->AddRectFilled(
                    { win_pos.x + active_page->indicator_x, win_pos.y + strip_local_y },
                    { win_pos.x + active_page->indicator_x + active_page->indicator_w, win_pos.y + strip_local_y + 2 },
                    color_t( t.accent ).modulate( alpha ), 1 );

                PopStyleVar( );
            }

            EndChild( );
        }

        SetCursorPos( { 15, tabs_height + subtabs_height } );
        BeginChild( "##main", { GetContentRegionAvail( ).x - 15, GetContentRegionAvail( ).y }, 0, ImGuiWindowFlags_NoScrollbar );
        {
            window_animator.capture( GetWindowDrawList( ) );
            auto* main_window = GetCurrentWindow( );

            {
                constexpr float k_sb_reserve = 4.f + 4.f;
                float const target = page_scrollbar.get_fade( ) > 0.01f ? k_sb_reserve : 0.f;
                scrollbar_inset = ImLerp( scrollbar_inset, target, 20.f * dt );
                if ( ImFabs( scrollbar_inset - target ) < 0.01f )
                    scrollbar_inset = target;

                main_window->ContentRegionRect.Max.x -= scrollbar_inset;
                main_window->WorkRect.Max.x -= scrollbar_inset;
            }

            auto const render_with_anim = [ & ]( float t, float offset, auto&& fn ) {
                bool const animating = t < 1.f || offset != 0.f;
                if ( animating )
                {
                    PushStyleVar( ImGuiStyleVar_Alpha, GetStyle( ).Alpha * t );
                    SetCursorPos( GetCursorPos( ) + ImVec2( 0, offset ) );
                }
                fn( );
                if ( animating )
                    PopStyleVar( );
            };

            if ( !has_subpages )
            {
                render_with_anim( page_t, ImFloor( page_offset ), [ & ] {
                    set_search_context( display_page_idx, -1 );
                    active_page->render( );
                } );
            } else
            {
                if ( active_page->display_subpage_idx != active_page->selected_subpage_idx )
                {
                    active_page->switch_t = ImLerp( active_page->switch_t, 0.f, fade_speed );
                    if ( active_page->switch_t < 0.02f )
                    {
                        active_page->switch_t = 0.f;
                        active_page->display_subpage_idx = active_page->selected_subpage_idx;
                    }
                } else
                {
                    active_page->switch_t = ImLerp( active_page->switch_t, 1.f, fade_speed );
                }

                float const sub_t = ImClamp( active_page->switch_t, 0.f, 1.f );
                float const combined_t = sub_t * page_t;
                float const sub_offset = ImFloor( 40.f * ( 1.f - sub_t ) + page_offset );

                render_with_anim( combined_t, sub_offset, [ & ] {
                    set_search_context( display_page_idx, active_page->display_subpage_idx );
                    active_page->subpages[ active_page->display_subpage_idx ]->render( );
                    ItemSize( { 0, 15 } );
                } );
            }

            page_scrollbar.render( main_window, { -9, 12 }, true, 4.f );

            EndChild( );
        }

        set_search_context( -1, -1 );

        search_anim = ImLerp( search_anim, search_open ? 1.f : 0.f, 20.f * GetIO( ).DeltaTime );
        if ( search_anim < 0.01f )
            search_anim = 0.f;

        if ( search_anim > 0.01f )
        {
            SetCursorPos( { 0, 0 } );
            BeginChild( "##blur_layer", rect.GetSize( ), 0, ImGuiWindowFlags_NoInputs );
            {
                window_animator.capture( GetWindowDrawList( ) );
                core->blur->apply_squircle( GetWindowDrawList( ), rect.Min, rect.Max, 14.f, 12.f, 5, 3.f, 2, search_anim );
            }
            EndChild( );

            SetCursorPos( { 0, 0 } );
            BeginChild( "##search_input_host", rect.GetSize( ), 0, ImGuiWindowFlags_NoScrollbar );
            {
                window_animator.capture( GetWindowDrawList( ) );
                render_search_overlay( rect, search_anim );
            }
            EndChild( );
        }

        End( );

        window_animator.end( window->Pos, window->Size );
    }

    PopStyleVar( );

    c_notifications::render( );
}

void c_ui::set_presence( bool value )
{
    presented = value;
}

bool c_ui::is_presented( )
{
    return presented;
}
