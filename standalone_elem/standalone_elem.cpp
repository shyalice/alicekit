//
// Created by rei on 4/9/2026.
//

#include "standalone_elem.hpp"

#include "alice_kit.hpp"
#include "../elem_animations/elem_animations.hpp"
#include "fonts/fonts.hpp"
#include "fonts/lucide_defines.hpp"
#include "popups/popup_elem/popup_elem.hpp"
#include "renderer/renderer.hpp"
#include "smooth_scroll/smooth_scroll.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"
#include "window_animator/window_animator.hpp"

#include <cmath>
#include <functional>
#include <string>
#include <type_traits>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

using namespace ImGui;

bool alice::std_elem::toggle( ImGuiID const& id, bool* v, toggle_info_t* info, ImRect const& rect, const std::optional< std::pair< bool, float > >& disable_mod )
{
    if ( !ItemAdd( rect, id ) )
        return false;

    bool const disabled = disable_mod.has_value( ) ? disable_mod.value( ).first : false;
    float const disable_alpha_mod = disable_mod.has_value( ) ? ( 1.0f - ( 0.6f * disable_mod->second ) ) : 1.f;

    bool hovered, held;
    bool const pressed = ButtonBehavior( rect, id, &hovered, &held );

    auto* window = GetCurrentWindow( );
    auto const alpha = GetStyle( ).Alpha;

    if ( pressed && !disabled )
        *v = !*v;

    auto const animation_time = 20 * GetIO( ).DeltaTime;
    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, *v ? t.accent_surface : ( ( hovered && !disabled ) ? t.control_hover : t.control ), animation_time );
    info->outline = ImLerp( info->outline, *v ? t.accent_border : ( ( hovered && !disabled ) ? t.control_active : t.control_hover ), animation_time );
    info->pill = ImLerp( info->pill, *v ? ImVec4( 1, 1, 1, 1 ) : ( ( hovered && !disabled ) ? t.pill_hover : t.pill ), animation_time );

    int const vtx0 = window->DrawList->VtxBuffer.Size;
    int const idx0 = window->DrawList->IdxBuffer.Size;

    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( info->background ).modulate( alpha * disable_alpha_mod ), 128 );
    window->DrawList->AddRect( rect.Min, rect.Max, color_t( info->outline ).modulate( alpha * disable_alpha_mod ), 128 );

    auto const radius = std::floor( rect.GetHeight( ) / 2 - 2 );
    auto const pad = radius + 2;

    if ( alpha >= 0.99f )
        info->pos = ImLerp( info->pos, *v ? ( rect.GetWidth( ) - pad * 2 ) : 0.f, animation_time * 0.5f );

    window->DrawList->AddShadowCircle( { rect.Min.x + pad + info->pos, rect.GetCenter( ).y }, radius, color_t( 0, 0, 0 ).modulate( alpha * disable_alpha_mod ), 8, { 0, 2 } );
    window->DrawList->AddCircleFilled( { rect.Min.x + pad + info->pos, rect.GetCenter( ).y }, radius, color_t( info->pill ).modulate( alpha * disable_alpha_mod ), 64 );

    int const vtx1 = window->DrawList->VtxBuffer.Size;
    int const idx1 = window->DrawList->IdxBuffer.Size;

    if ( disable_mod.has_value( ) )
        core->grayscale->apply( window->DrawList, vtx0, vtx1, idx0, idx1, disable_mod->second );

    return pressed;
}

bool alice::std_elem::begin_dropdown( ImGuiID const& id, ImRect const& rect, dropdown_info_t* info, std::string const& preview_value, int entries_amount, const std::optional< std::pair< bool, float > >& disable_mod )
{
    if ( !ItemAdd( rect, id ) )
        return false;

    auto const font = core->fonts->get( "primary" );

    bool const disabled = disable_mod.has_value( ) ? disable_mod.value( ).first : false;
    float const disable_alpha_mod = disable_mod.has_value( ) ? ( 1.0f - ( 0.6f * disable_mod->second ) ) : 1.f;

    bool hovered, held;
    bool const pressed = ButtonBehavior( rect, id, &hovered, &held );

    auto* window = GetCurrentWindow( );
    auto const alpha = GetStyle( ).Alpha;

    auto const animation_time = 20 * GetIO( ).DeltaTime;
    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, info->popup.is_open( ) ? t.control_active : ( ( hovered && !disabled ) ? t.control_hover : t.control ), animation_time );
    info->outline = ImLerp( info->outline, info->popup.is_open( ) ? t.border_active : ( ( hovered && !disabled ) ? t.control_active : t.control_hover ), animation_time );
    info->text = ImLerp( info->text, info->popup.is_open( ) ? t.text_mid : ( ( hovered && !disabled ) ? t.text_muted : t.text_subtle ), animation_time );

    int const vtx0 = window->DrawList->VtxBuffer.Size;
    int const idx0 = window->DrawList->IdxBuffer.Size;

    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( info->background ).modulate( alpha ), 4 );

    static std::string const angle_down = ICON_LUCIDE_CHEVRON_DOWN;
    auto const angle_size = font.calculate_text_size( angle_down );

    window->DrawList->PushClipRect( rect.Min, rect.Max - ImVec2( angle_size.x + 12, 0 ), true );
    window->DrawList->AddText( { rect.Min.x + 6, rect.GetCenter( ).y - font.calculate_text_size( preview_value ).y / 2 }, color_t( info->text ).modulate( ), preview_value.c_str( ) );
    window->DrawList->PopClipRect( );

    window->DrawList->AddText( { rect.Max.x - 6 - angle_size.x, rect.GetCenter( ).y - angle_size.y / 2 }, color_t( info->text ).modulate( ), angle_down.c_str( ) );

    window->DrawList->AddRect( rect.Min, rect.Max, color_t( info->outline ).modulate( alpha ), 4 );

    if ( pressed && !disabled )
    {
        MarkItemEdited( id );
        if ( info->popup.open_allowed( ) )
            info->popup.open( );
    }

    int const vtx1 = window->DrawList->VtxBuffer.Size;
    int const idx1 = window->DrawList->IdxBuffer.Size;

    if ( disable_mod.has_value( ) )
        core->grayscale->apply( window->DrawList, vtx0, vtx1, idx0, idx1, disable_mod->second );

    info->popup.get_window_animator( )->update( GetIO( ).DeltaTime );

    float const single_height = c_popup_elem::get_elem_height( );
    float const entries_height = single_height * entries_amount;
    float const max_height = single_height * 6;
    float const visible_height = std::min( entries_height, max_height ) + 10;

    info->popup.set_size_constraints( { rect.GetWidth( ) + 10, max_height + 10 } );
    info->popup.set_title( info->random_title );
    info->popup.set_width( rect.GetWidth( ) + 10 );
    info->popup.set_position( { rect.Min.x - 5, rect.GetCenter( ).y - visible_height / 2 } );
    info->popup.hint_content_height( std::min( entries_height, max_height ) );

    if ( !info->popup.begin( ) )
        return false;

    alice::c_smooth_scroll::instance( ).push_step( 3.f * single_height );
    return true;
}

bool alice::std_elem::dropdown( ImGuiID const& id, dropdown_info_t* info, ImRect const& rect, int* v, std::vector< std::string > const& entries, const std::optional< std::pair< bool, float > >& disable_mod )
{
    ImGuiContext& g = *GImGui;

    if ( !begin_dropdown( id, rect, info, entries[ *v ], entries.size( ), disable_mod ) )
        return false;

    bool value_changed = false;
    for ( int it { 0 }; it < entries.size( ); it++ )
    {
        PushID( it );
        bool const item_selected = ( it == *v );
        if ( c_popup_elem::selectable( std::nullopt, entries[ it ], item_selected ) )
        {
            value_changed = true;
            *v = it;
        }
        if ( item_selected )
            SetItemDefaultFocus( );
        PopID( );
    }

    alice::c_smooth_scroll::instance( ).pop_step( );
    info->popup.end( );

    if ( value_changed )
        MarkItemEdited( g.LastItemData.ID );

    return value_changed;
}

void alice::std_elem::simple_tabs( ImGuiID const& id, simple_tabs_info_t* info, int* v, ImVec2 const& center, std::vector< std::pair< tab_badge_t, std::string > > const& entries, c_window_animator* animator )
{
    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
    PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );

    auto const font = core->fonts->get( "primary" );
    auto const alpha = GetStyle( ).Alpha;
    auto const animation_time = 20 * GetIO( ).DeltaTime;
    auto const& t = c_themes::palette( );

    constexpr float k_badge_size = 12.f;
    constexpr float k_badge_rounding = 6.f;
    constexpr float k_cell = 26.f;

    float target_width { 0 };
    for ( int it { 0 }; it < static_cast< int >( entries.size( ) ); it++ )
    {
        float const text_w = ( *v == it ) ? ( font.calculate_text_size( entries[ it ].second ).x + 5 ) : 0.f;
        target_width += k_cell + text_w + 2;
    }
    target_width -= 2;

    info->smooth_width = ( info->smooth_width < 1.f )
                             ? target_width
                             : ImLerp( info->smooth_width, target_width, animation_time );

    auto const saved_cur = GetCursorScreenPos( );
    SetCursorScreenPos( { center.x - info->smooth_width / 2 - 20, center.y - 2 } );
    BeginChild( std::to_string( id ).c_str( ), { info->smooth_width + 40, 26 }, 0 );
    {
        if ( animator )
            animator->capture( GetWindowDrawList( ) );

        float const content_width = info->smooth_width;
        SetCursorPosX( GetWindowWidth( ) / 2 - content_width / 2 );

        for ( int it { 0 }; it < static_cast< int >( entries.size( ) ); it++ )
        {
            auto& entry_info = info->entries_info[ id + ( it + 1 ) ];
            auto& entry = entries[ it ];

            bool const selected = ( *v == it );

            auto const size = ImVec2( k_cell + ( entry_info.mod * ( font.calculate_text_size( entry.second ).x + 5 ) ), k_cell );
            auto const btn_pos = GetCursorScreenPos( );

            PushID( id + ( it + 1 ) );
            if ( InvisibleButton( "##tab", size ) )
                *v = it;
            PopID( );

            bool const hovered = IsItemHovered( );

            entry_info.mod = ImLerp( entry_info.mod, selected ? 1.f : 0.f, animation_time );
            entry_info.background = ImLerp( entry_info.background, selected ? t.control_selected : ( hovered ? t.control_hover : ImVec4( 0, 0, 0, 0 ) ), animation_time );
            entry_info.icon = ImLerp( entry_info.icon, selected ? t.accent : t.text_muted, animation_time );
            entry_info.text = ImLerp( entry_info.text, selected ? t.text : ImVec4( t.text.x, t.text.y, t.text.z, 0.f ), animation_time );

            ImRect const rect = { btn_pos, btn_pos + size };
            auto* dl = GetWindowDrawList( );

            dl->AddRectFilled( rect.Min, rect.Max, color_t( entry_info.background ).modulate( alpha ), 6 );

            ImVec2 const badge_center = { rect.Min.x + k_cell * 0.5f, rect.GetCenter( ).y };

            std::visit( [ & ]( auto const& badge ) {
                using badge_type = std::decay_t< decltype( badge ) >;
                if constexpr ( std::is_same_v< badge_type, std::string > )
                {
                    auto const icon_sz = font.calculate_text_size( badge );
                    dl->AddText( font.ptr, font.size, badge_center - icon_sz / 2, color_t( entry_info.icon ).modulate( alpha ), badge.c_str( ) );
                } else
                {
                    ImVec2 const half = { k_badge_size * 0.5f, k_badge_size * 0.5f };
                    dl->AddRectFilled( badge_center - half, badge_center + half, color_t( badge ).modulate( alpha ), k_badge_rounding );
                }
            },
                        entry.first );

            dl->PushClipRect( rect.Min, rect.Max, true );
            auto const title_sz = font.calculate_text_size( entry.second );
            dl->AddText( font.ptr, font.size, { rect.Min.x + 20 + 5, rect.GetCenter( ).y - title_sz.y / 2 }, color_t( entry_info.text ).modulate( alpha ), entry.second.c_str( ) );
            dl->PopClipRect( );

            SameLine( 0, 2 );
        }

        EndChild( );
    }

    SetCursorScreenPos( saved_cur + ImVec2( 0, 26 ) );

    PopStyleVar( 2 );
}

void alice::std_elem::osx_tabs( ImGuiID const& id, osx_tabs_info_t* info, int* v, ImRect const& rect, std::vector< std::string > const& entries )
{
    auto* window = GetCurrentWindow( );
    auto const alpha = GetStyle( ).Alpha;

    auto const animation_time = 20 * GetIO( ).DeltaTime;
    auto const& t = c_themes::palette( );

    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( t.control_subtle ).modulate( alpha ), 6 );

    auto const entry_width = rect.GetWidth( ) / entries.size( );

    auto const font = core->fonts->get( "primary" );

    ImVec2 cursor = { rect.Min.x, rect.Min.y };
    for ( int it { 0 }; it < entries.size( ); it++ )
    {
        auto& entry_info = info->entries_info[ id + ( it + 1 ) ];
        auto const entry_id = id + ( it + 1 );

        ImRect const entry_rect = {
            cursor,
            cursor + ImVec2( entry_width, rect.GetHeight( ) ) };

        if ( !ItemAdd( entry_rect, entry_id ) )
            continue;

        bool hovered, held;
        bool const pressed = ButtonBehavior( entry_rect, entry_id, &hovered, &held );

        if ( pressed )
        {
            *v = it;
        }

        entry_info.background = ImLerp( entry_info.background, ( it == *v ) ? t.control_selected : ( hovered ? ImLerp( t.control_subtle, t.control_hover, 0.5f ) : t.control_subtle ), animation_time );
        entry_info.text = ImLerp( entry_info.text, ( it == *v ) ? t.text : ( hovered ? t.text_light : t.text_muted ), animation_time );

        window->DrawList->AddRectFilled( entry_rect.Min, entry_rect.Max, color_t( entry_info.background ).modulate( alpha ), 6 );
        window->DrawList->AddText( font.ptr, font.size, entry_rect.GetCenter( ) - font.calculate_text_size( entries[ it ] ) / 2, color_t( entry_info.text ).modulate( alpha ), entries[ it ].c_str( ) );

        cursor.x += entry_width;
    }

    window->DrawList->AddRect( rect.Min, rect.Max, color_t( t.control_selected ).modulate( alpha ), 6 );
}

ImVec4 alice_color_clipboard { };
std::vector< ImVec4 > alice_color_palette { };

void alice::std_elem::color_button( ImGuiID const& id, color_info_t* info, ImRect const& rect, std::pair< ImVec4*, std::optional< ImVec4* > > const& colors, const std::optional< std::pair< bool, float > >& disable_mod )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;

    if ( !ItemAdd( rect, id ) )
        return;

    bool const disabled = disable_mod.has_value( ) ? disable_mod.value( ).first : false;
    float const disable_alpha_mod = disable_mod.has_value( ) ? ( 1.0f - ( 0.6f * disable_mod->second ) ) : 1.f;

    bool hovered, held;
    bool const pressed = ButtonBehavior( rect, id, &hovered, &held );

    auto const alpha = GetStyle( ).Alpha;

    auto const& t = c_themes::palette( );

    int const vtx0 = window->DrawList->VtxBuffer.Size;
    int const idx0 = window->DrawList->IdxBuffer.Size;

    info->hover = ImLerp( info->hover, ( pressed || info->popup.is_open( ) ) ? t.overlay_faint : ( ( hovered && !disabled ) ? t.overlay_subtle : with_alpha( t.overlay_subtle, 0.f ) ), 20 * g.IO.DeltaTime );

    window->DrawList->AddRectFilled( rect.Min - ImVec2( 4, 4 ), rect.Max + ImVec2( 4, 4 ), color_t( info->hover ).modulate( alpha ), 6 );

    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( t.border ).modulate( alpha ), 6 );

    // renderer_t::add_rect( window->DrawList, rect.Min, rect.Max, color_t( 32, 32, 32 ).modulate( alpha ), 1, 8, ImDrawFlags_RoundCornersAll, 5, 1.4f );

    int const vert_start_idx = window->DrawList->VtxBuffer.Size;
    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( *( colors.first ) ).modulate( alpha ), 6 );
    int const vert_end_idx = window->DrawList->VtxBuffer.Size;

    if ( colors.second.has_value( ) )
        ShadeVertsLinearColorGradientKeepAlpha( window->DrawList, vert_start_idx, vert_end_idx, rect.Min, rect.Max, ImColor( *( colors.first ) ), ImColor( *( colors.second.value( ) ) ) );

    int const vtx1 = window->DrawList->VtxBuffer.Size;
    int const idx1 = window->DrawList->IdxBuffer.Size;

    if ( disable_mod.has_value( ) )
        core->grayscale->apply( window->DrawList, vtx0, vtx1, idx0, idx1, disable_mod->second );

    if ( pressed && info->popup.open_allowed( ) && !disabled )
    {
        info->popup.set_position( { rect.Max.x + 5, rect.Min.y - 80 } );
        info->popup.open( );

        info->first = colors.first;
        info->second = colors.second;
    }

    PushID( id * 150 );

    info->popup.set_title( std::to_string( id ) + "##picker__" );
    info->popup.set_window_padding( { 10, 10 } );
    info->popup.set_width( 260 );

    if ( info->popup.begin( ) )
    {
        render_color_picker( id, info, info->first, info->second );

        info->popup.end( );
    }

    PopID( );
}

void alice::std_elem::color_button( ImGuiID const& id, color_info_t* info, ImRect const& rect, ImVec4* color, const std::optional< std::pair< bool, float > >& disable_mod )
{
    color_button( id, info, rect, { color, std::nullopt }, disable_mod );
}

void alice::std_elem::render_color_picker( ImGuiID const& id, color_info_t* info, ImVec4* color, std::optional< ImVec4* > secondary )
{
    auto& g = *GImGui;
    auto& style = g.Style;
    auto* window = GetCurrentWindow( );
    auto const pos = window->DC.CursorPos;
    auto const col_size = ImVec2( GetContentRegionAvail( ).x, 28 );
    auto const base_alpha = style.Alpha;
    auto const anim_speed = GetIO( ).DeltaTime * 20;

    auto const& t = c_themes::palette( );

    float const fade_speed = 18.f * g.IO.DeltaTime;

    if ( info->display_page_idx != info->selected_page_idx )
    {
        info->switch_t = ImLerp( info->switch_t, 0.f, fade_speed );
        if ( info->switch_t < 0.02f )
        {
            info->switch_t = 0.f;
            info->display_page_idx = info->selected_page_idx;
        }
    } else
    {
        info->switch_t = ImLerp( info->switch_t, 1.f, fade_speed );
    }

    float const page_t = ImClamp( info->switch_t, 0.f, 1.f );

    PushStyleVar( ImGuiStyleVar_Alpha, base_alpha * page_t );
    PushStyleVar( ImGuiStyleVar_ItemInnerSpacing, { 10, 10 } );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 10, 10 } );

    PushStyleVar( ImGuiStyleVar_Alpha, base_alpha );
    if ( secondary.has_value( ) )
    {
        auto const tabs_rect = ImRect { pos, pos + col_size };
        simple_tabs( id, c_elem_animations::instance( ).get< simple_tabs_info_t >( window->Name, id ), &info->selected_page_idx, { tabs_rect.GetCenter( ).x, tabs_rect.Min.y }, { { *color, "First" }, { *( secondary.value( ) ), "Second" } }, info->popup.get_window_animator( ) );
        ItemSize( { 0, 0 } );
        SetCursorPosY( GetCursorPosY( ) - 4 );
    }
    PopStyleVar( );

    float const alpha = style.Alpha;

    auto* color_ptr = secondary.has_value( ) ? ( ( info->display_page_idx == 0 ) ? color : secondary.value( ) ) : color;

    if ( !color_ptr )
    {
        PopStyleVar( 2 );
        return;
    }

    auto flags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoSidePreview | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoOptions;
    flags &= ~ImGuiColorEditFlags_PickerHueBar;
    flags &= ~ImGuiColorEditFlags_PickerHueWheel;

    SetNextItemWidth( col_size.x );
    ColorPicker4( ( "##picker" + std::to_string( reinterpret_cast< uintptr_t >( color_ptr ) ) ).c_str( ),
                  ( float* ) color_ptr, flags );

    auto const size = ImVec2( 18, 18 );
    auto const begin = GetCursorScreenPos( );
    auto const avail_width = GetContentRegionAvail( ).x;

    ItemSize( ImVec2( 0, size.y ) );

    static int selected_color = -1;
    static constexpr auto compare_colors = []( ImVec4 const& a, ImVec4 const& b ) -> bool {
        return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w;
    };

    for ( int it { 0 }; it < ( int ) alice_color_palette.size( ); it++ )
    {
        auto const col = alice_color_palette[ it ];

        ImRect rect = {
            { begin.x + ( ( size.x + 5 ) * it ), begin.y },
            { begin.x + ( ( size.x + 5 ) * it ) + size.x, begin.y + size.y } };

        ImGuiID cid = GetID( ( "##palette_at_" + std::to_string( static_cast< float >( it ) + col.w + col.z + col.y + col.x ) ).c_str( ) ) * 24 * ( it + 1 );
        ItemAdd( rect, cid );

        bool hov, held;
        bool const pressed = ButtonBehavior( rect, cid, &hov, &held );

        auto& cst = info->palette_infos[ cid ];

        ImVec4 const bg_target = pressed ? t.control_active
                                         : ( hov ? t.control_hover : with_alpha( t.control_hover, 0.f ) );
        cst.background = ImLerp( cst.background, bg_target, anim_speed );

        window->DrawList->AddRectFilled( rect.Min - ImVec2( 2, 2 ), rect.Max + ImVec2( 2, 2 ), color_t( cst.background ).modulate( alpha ), 4 );

        bool const selected = compare_colors( *color_ptr, col ) && selected_color == it;

        ImVec4 const sel_target = selected ? t.text_bright : with_alpha( t.text_bright, 0.f );
        cst.select = ImLerp( cst.select, sel_target, anim_speed );
        cst.alpha = ImLerp( cst.alpha, 1.f, anim_speed );

        window->DrawList->AddCircleFilled( rect.GetCenter( ), rect.GetHeight( ) / 2, color_t( cst.select ).modulate( alpha ), 24 );
        window->DrawList->AddCircleFilled( rect.GetCenter( ), rect.GetHeight( ) / 3, color_t( col ).modulate( alpha * cst.alpha ), 24 );

        if ( pressed )
        {
            selected_color = it;
            *color_ptr = col;
        }
    }

    static std::vector< std::pair< std::string, std::function< void( ) > > > service_btns = {
        std::make_pair< std::string, std::function< void( ) > >( ICON_LUCIDE_PLUS, [] {} ) };

    for ( int it { 0 }; it < ( int ) service_btns.size( ); it++ )
    {
        auto const& btn = service_btns[ it ];

        ImRect rect = {
            { begin.x + avail_width - ( size.x * ( it + 1 ) ), begin.y },
            { begin.x + avail_width - ( size.x * it ), begin.y + size.y } };

        ImGuiID cid = GetID( btn.first.c_str( ) );
        ItemAdd( rect, cid );

        bool hov, held;
        bool const pressed = ButtonBehavior( rect, cid, &hov, &held );

        auto& cst = info->palette_infos[ cid ];

        ImVec4 const bg_target = pressed ? t.control_active
                                         : ( hov ? t.control_hover : with_alpha( t.control_hover, 0.f ) );
        cst.background = ImLerp( cst.background, bg_target, anim_speed );

        window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( cst.background ).modulate( alpha ), 4 );
        window->DrawList->AddText( rect.GetCenter( ) - CalcTextSize( btn.first.c_str( ) ) / 2, color_t( t.text_secondary ).modulate( alpha ), btn.first.c_str( ) );

        if ( pressed )
        {
            if ( alice_color_palette.size( ) >= 9 )
                alice_color_palette.erase( alice_color_palette.begin( ) );

            alice_color_palette.push_back( *color_ptr );
        }
    }

    PopStyleVar( 2 );

    c_popup_elem::begin_group( info->popup.get_window_animator( ), "Actions" );

    if ( c_popup_elem::selectable( ICON_LUCIDE_CLIPBOARD_COPY, "Copy", false ) )
        alice_color_clipboard = *color_ptr;

    if ( c_popup_elem::selectable( ICON_LUCIDE_CLIPBOARD_PASTE, "Paste", false ) )
        *color_ptr = alice_color_clipboard;

#ifdef _WIN32
    static bool search_col = false;
    static bool picking = false;
    static HDC screen_dc = nullptr;

    if ( c_popup_elem::selectable( ICON_LUCIDE_PALETTE, "Pick", false ) )
        search_col = true;

    if ( search_col && !picking )
    {
        screen_dc = GetDC( NULL );
        picking = true;
    }

    if ( !search_col && picking )
    {
        ReleaseDC( NULL, screen_dc );
        screen_dc = nullptr;
        picking = false;
    }

    if ( search_col && picking )
    {
        POINT pt { };
        GetCursorPos( &pt );

        COLORREF px = GetPixel( screen_dc, pt.x, pt.y );
        if ( px != CLR_INVALID )
        {
            color_ptr->x = GetRValue( px ) / 255.f;
            color_ptr->y = GetGValue( px ) / 255.f;
            color_ptr->z = GetBValue( px ) / 255.f;
        }

        if ( GetAsyncKeyState( VK_LBUTTON ) & 0x8000 )
            search_col = false;
        if ( GetAsyncKeyState( VK_ESCAPE ) & 0x8000 )
            search_col = false;
    }
#endif

    c_popup_elem::end_group( );

    PopStyleVar( );
}

template < typename T >
bool alice::std_elem::slider( ImGuiID const& id, slider_info_t* info, ImRect const& rect, T* v, T v_min, T v_max, char const* format, bool has_input, const std::optional< std::pair< bool, float > >& disable_mod )
{
    float const grab_radius = 5.f;
    float const center_y = rect.GetCenter( ).y;
    ImRect const hit_rect = { { rect.Min.x, center_y - grab_radius }, { rect.Max.x, center_y + grab_radius } };

    if ( !ItemAdd( hit_rect, id ) )
        return false;

    auto* window = GetCurrentWindow( );
    auto const alpha = GetStyle( ).Alpha;

    bool const disabled = disable_mod.has_value( ) ? disable_mod.value( ).first : false;
    float const disable_alpha_mod = disable_mod.has_value( ) ? ( 1.0f - ( 0.6f * disable_mod->second ) ) : 1.f;

    bool const hovered = !disabled && ItemHoverable( hit_rect, id, 0 );
    bool const clicked = hovered && IsMouseClicked( 0, ImGuiInputFlags_None, id );

    if ( clicked )
    {
        SetActiveID( id, window );
        SetFocusID( id, window );
        FocusWindow( window );
    }

    if ( disabled && GetActiveID( ) == id )
        ClearActiveID( );

    constexpr auto data_type = std::is_same_v< T, float > ? ImGuiDataType_Float : ImGuiDataType_S32;

    ImRect grab_bb;
    bool value_changed = false;
    if ( !disabled )
    {
        value_changed = SliderBehavior( rect, id, data_type, v, &v_min, &v_max, format, ImGuiSliderFlags_None, &grab_bb );
        if ( value_changed )
            MarkItemEdited( id );
    }
    else
    {
        float const progress = ( data_type == ImGuiDataType_Float )
            ? ( ( *reinterpret_cast< float* >( v ) - *reinterpret_cast< float* >( &v_min ) ) / ( *reinterpret_cast< float* >( &v_max ) - *reinterpret_cast< float* >( &v_min ) ) )
            : ( ( *reinterpret_cast< int* >( v ) - *reinterpret_cast< int* >( &v_min ) ) / static_cast< float >( *reinterpret_cast< int* >( &v_max ) - *reinterpret_cast< int* >( &v_min ) ) );
        float const x = rect.Min.x + std::clamp( progress, 0.f, 1.f ) * ( rect.Max.x - rect.Min.x );
        grab_bb = { { x - grab_radius, center_y - grab_radius }, { x + grab_radius, center_y + grab_radius } };
    }

    bool const active = ( GetActiveID( ) == id );

    auto const anim_speed = GetIO( ).DeltaTime * 20;
    auto const& t = c_themes::palette( );

    info->track = ImLerp( info->track, ( ( hovered || active ) && !disabled ) ? t.control_hover : t.control, anim_speed );
    info->fill = ImLerp( info->fill, ( active && !disabled ) ? t.accent_strong : t.accent_surface, anim_speed );
    info->grab = ImLerp( info->grab, ImVec4( 1, 1, 1, 1 ), anim_speed );
    info->text = ImLerp( info->text, ( ( hovered || active ) && !disabled ) ? t.text_secondary : t.text_muted, anim_speed );

    int const vtx0 = window->DrawList->VtxBuffer.Size;
    int const idx0 = window->DrawList->IdxBuffer.Size;

    window->DrawList->AddRectFilled( rect.Min, rect.Max, color_t( info->track ).modulate( alpha ), 4 );

    if ( grab_bb.Max.x > rect.Min.x )
        window->DrawList->AddRectFilled( rect.Min, { rect.Min.x + info->pos, rect.Max.y }, color_t( info->fill ).modulate( alpha ), 4 );

    ImVec2 const grab_center = { grab_bb.GetCenter( ).x - rect.Min.x, center_y };
    info->pos = ImLerp( info->pos, grab_center.x, anim_speed * 0.5f );
    info->mod = ImLerp( info->mod, ( active || value_changed ) ? 1.f : 0.f, anim_speed );

    window->DrawList->AddShadowCircle( { rect.Min.x + info->pos, grab_center.y }, grab_radius + ( 2 * info->mod ), color_t( 0, 0, 0 ).modulate( alpha ), 10, { 0, 0 } );
    window->DrawList->AddCircleFilled( { rect.Min.x + info->pos, grab_center.y }, grab_radius + ( 2 * info->mod ), color_t( info->grab ).modulate( alpha ), 32 );

    char value_buf[ 64 ];
    DataTypeFormatString( value_buf, IM_ARRAYSIZE( value_buf ), data_type, v, format );

    auto const cur = GetCursorPos( );

    if ( has_input )
    {
        float const input_h = 18.f;
        float const input_pad = 6.f;
        float const min_w = CalcTextSize( "00" ).x + input_pad * 2;

        if ( !info->input_was_active )
            DataTypeFormatString( info->input_buf, IM_ARRAYSIZE( info->input_buf ), data_type, v, format );

        float const text_w = CalcTextSize( info->input_buf ).x + input_pad * 2;
        float const target_w = ImMax( min_w, text_w );
        info->input_w = ImLerp( info->input_w, target_w, info->input_w == 0.f ? 1.f : anim_speed );

        ImRect const input_rect = {
            { rect.Min.x - info->input_w - 10, center_y - input_h / 2 },
            { rect.Min.x - 10, center_y + input_h / 2 } };

        info->input_bg = ImLerp( info->input_bg,
                                 info->input_was_active ? t.control_hover : t.control, anim_speed );

        window->DrawList->AddRectFilled( input_rect.Min, input_rect.Max, color_t( info->input_bg ).modulate( alpha ), 4, ImDrawFlags_RoundCornersAll );

        window->DrawList->PushClipRect( input_rect.Min, input_rect.Max, true );

        PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_FrameBgHovered, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_FrameBgActive, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_Border, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_Text, color_t( t.text ).modulate( alpha ).Value );
        PushStyleColor( ImGuiCol_TextSelectedBg, t.accent_bg );
        PushStyleVar( ImGuiStyleVar_FramePadding, { input_pad, ( input_h - GetFontSize( ) ) / 2 } );
        PushStyleVar( ImGuiStyleVar_FrameBorderSize, 0 );

        SetCursorScreenPos( input_rect.Min );
        PushItemWidth( input_rect.GetWidth( ) );
        PushID( id + 100 );

        ImGuiInputTextFlags const input_flags = ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_NoHorizontalScroll | ImGuiInputTextFlags_CharsDecimal
            | ( disabled ? ImGuiInputTextFlags_ReadOnly : 0 );
        InputText( "##slider_v", info->input_buf, 5, input_flags );

        bool const edited = IsItemDeactivatedAfterEdit( );
        bool const input_focused = IsItemActive( );
        bool const input_hovered = IsItemHovered( );
        info->input_was_active = input_focused;

        bool input_out_of_range = false;
        if ( input_focused && info->input_buf[ 0 ] )
        {
            T preview { };
            if ( DataTypeApplyFromText( info->input_buf, data_type, &preview, format ) )
                input_out_of_range = ( preview < v_min || preview > v_max );
        }

        PopID( );
        PopItemWidth( );
        PopStyleVar( 2 );
        PopStyleColor( 6 );

        window->DrawList->PopClipRect( );

        ImVec4 const outline_target = input_out_of_range
                                          ? ImVec4( t.danger.x, t.danger.y * 0.35f, t.danger.z * 0.35f, 1.f )
                                          : ( input_focused ? t.accent_muted : t.control_hover );
        info->input_outline = ImLerp( info->input_outline, outline_target, anim_speed );

        window->DrawList->AddRect( input_rect.Min, input_rect.Max, color_t( info->input_outline ).modulate( alpha ), 4, ImDrawFlags_RoundCornersAll );

        if ( edited )
        {
            T parsed { };
            DataTypeApplyFromText( info->input_buf, data_type, &parsed, format );
            if ( parsed >= v_min && parsed <= v_max )
                *v = parsed;
            info->input_was_active = false;
        }
    } else
    {
        auto const text_size = CalcTextSize( value_buf );
        window->DrawList->AddText( { rect.Min.x - text_size.x - 8, center_y - text_size.y / 2 }, color_t( info->text ).modulate( alpha ), value_buf );
    }

    int const vtx1 = window->DrawList->VtxBuffer.Size;
    int const idx1 = window->DrawList->IdxBuffer.Size;

    if ( disable_mod.has_value( ) )
        core->grayscale->apply( window->DrawList, vtx0, vtx1, idx0, idx1, disable_mod->second );

    SetCursorPos( cur );

    return value_changed;
}

template bool alice::std_elem::slider< int >( ImGuiID const&, slider_info_t*, ImRect const&, int*, int, int, char const*, bool, const std::optional< std::pair< bool, float > >& );
template bool alice::std_elem::slider< float >( ImGuiID const&, slider_info_t*, ImRect const&, float*, float, float, char const*, bool, const std::optional< std::pair< bool, float > >& );
