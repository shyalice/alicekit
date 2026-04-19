//
// Created by rei on 4/10/2026.
//

#include "popup_elem.hpp"

#include "alice_kit.hpp"
#include "elem_animations/elem_animations.hpp"
#include "fonts/fonts.hpp"
#include "fonts/lucide_defines.hpp"
#include "renderer/renderer.hpp"
#include "standalone_elem/standalone_elem.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"

using alice::c_popup_elem;

using namespace ImGui;

c_popup_elem::c_popup_elem( ) = default;
c_popup_elem::~c_popup_elem( ) = default;

int c_popup_elem::get_elem_height( )
{
    return k_elem_height;
}

void c_popup_elem::begin_group( c_window_animator* animator, std::string const& id )
{
    BeginChild( id.c_str( ), { GetContentRegionAvail( ).x, 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_AlwaysUseWindowPadding );
    animator->capture( GetWindowDrawList( ) );
    auto const& t = c_themes::palette( );
    renderer_t::add_rect( GetWindowDrawList( ), GetWindowPos( ), GetWindowPos( ) + GetWindowSize( ), color_t( t.border ).modulate( GetStyle( ).Alpha ), 1, 10, ImDrawFlags_RoundCornersAll, 5, 3.f );
}

void c_popup_elem::end_group( )
{
    EndChild( );
}

void c_popup_elem::text( std::string const& label, float pad_top, float pad_bottom )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return;

    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

    auto const font = core->fonts->get( "subtitle" );
    ImVec2 const label_size = font.calculate_text_size( label );

    ItemSize( { 0, pad_top } );

    ImVec2 const pos = window->DC.CursorPos;
    ImRect const bb( pos, pos + ImVec2 { GetContentRegionAvail( ).x, label_size.y } );
    ItemSize( bb );
    auto const& t = c_themes::palette( );

    if ( ItemAdd( bb, 0 ) )
        window->DrawList->AddText( font.ptr, font.size, pos, color_t( t.text_secondary ).modulate( GetStyle( ).Alpha ), label.c_str( ) );

    ItemSize( { 0, pad_bottom } );

    PopStyleVar( );
}

bool c_popup_elem::button( std::optional< std::string > const& icon, std::string const& label )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( label.c_str( ) );
    auto const font = core->fonts->get( "primary" );
    ImVec2 const label_size = font.calculate_text_size( label );

    ImVec2 const pos = window->DC.CursorPos;
    ImVec2 const size = { GetContentRegionAvail( ).x, k_elem_height };

    ImRect const bb( pos, pos + size );
    ItemSize( size, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    bool hovered, held;
    bool const pressed = ButtonBehavior( bb, id, &hovered, &held );

    auto* info = c_elem_animations::instance( ).get< button_info_t >( window->Name, id );
    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, hovered ? t.overlay_hover : with_alpha( t.overlay_hover, 0.f ), 20 * g.IO.DeltaTime );

    if ( pressed )
        info->background = t.overlay_pressed;

    window->DrawList->AddRectFilled( bb.Min, bb.Max, color_t( info->background ).modulate( style.Alpha ), 8 );

    info->text = ImLerp( info->text, hovered ? t.text : t.text_bright, 20 * g.IO.DeltaTime );

    ImVec2 const icon_size = icon.has_value( ) ? font.calculate_text_size( icon.value( ) ) : ImVec2( 0, 0 );

    if ( icon.has_value( ) )
        window->DrawList->AddText( { bb.Min.x + 8, bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), icon.value( ).c_str( ) );

    window->DrawList->AddText( { bb.Min.x + 8 + ( icon.has_value( ) ? ( 8 + icon_size.x ) : 0.f ), bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), label.c_str( ) );

    return pressed;
}

bool c_popup_elem::selectable( std::optional< std::string > const& icon, std::string const& label, bool active )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( label.c_str( ) );
    auto const font = core->fonts->get( "primary" );
    ImVec2 const label_size = font.calculate_text_size( label );

    ImVec2 const pos = window->DC.CursorPos;
    ImVec2 const size = { GetContentRegionAvail( ).x, k_elem_height };

    ImRect const bb( pos, pos + size );
    ItemSize( size, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    bool hovered, held;
    bool const pressed = ButtonBehavior( bb, id, &hovered, &held );

    float const square_sz = 12.0f;
    float const pad = ImMax( 1.0f, IM_TRUNC( square_sz / 6.0f ) );

    auto* info = c_elem_animations::instance( ).get< selectable_info_t >( window->Name, id );
    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, hovered ? t.overlay_hover : with_alpha( t.overlay_hover, 0.f ), 20 * g.IO.DeltaTime );

    if ( pressed )
        info->background = t.overlay_pressed;

    window->DrawList->AddRectFilled( bb.Min, bb.Max, color_t( info->background ).modulate( style.Alpha ), 8 );

    info->mark = ImLerp( info->mark, active ? t.accent : with_alpha( t.accent, 0.f ), 20 * g.IO.DeltaTime );

    info->text = ImLerp( info->text, hovered ? t.text : t.text_bright, 20 * g.IO.DeltaTime );

    RenderCheckMark(
        window->DrawList,
        { bb.Max.x - square_sz - 8, bb.GetCenter( ).y - square_sz * 0.5f + 2 },
        color_t( info->mark ).modulate( style.Alpha ),
        square_sz - pad * 2.0f );

    ImVec2 const icon_size = icon.has_value( ) ? font.calculate_text_size( icon.value( ) ) : ImVec2( 0, 0 );

    if ( icon.has_value( ) )
        window->DrawList->AddText( { bb.Min.x + 8, bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), icon.value( ).c_str( ) );

    window->DrawList->AddText( { bb.Min.x + 8 + ( icon.has_value( ) ? ( 8 + icon_size.x ) : 0.f ), bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), label.c_str( ) );

    return pressed;
}

bool c_popup_elem::toggle( std::optional< std::string > const& icon, std::string const& label, bool* v )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( label.c_str( ) );
    auto const font = core->fonts->get( "primary" );
    ImVec2 const label_size = font.calculate_text_size( label );

    ImVec2 const pos = window->DC.CursorPos;
    ImVec2 const size = { GetContentRegionAvail( ).x, k_elem_height };

    ImRect const bb( pos, pos + size );
    ItemSize( size, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    bool hovered, held;
    bool const pressed = ButtonBehavior( bb, id, &hovered, &held );

    float const square_sz = 12.0f;
    float const pad = ImMax( 1.0f, IM_TRUNC( square_sz / 6.0f ) );

    auto* info = c_elem_animations::instance( ).get< selectable_info_t >( window->Name, id );
    auto* toggle_info = c_elem_animations::instance( ).get< toggle_info_t >( window->Name, id + 100 );
    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, hovered ? t.overlay_hover : with_alpha( t.overlay_hover, 0.f ), 20 * g.IO.DeltaTime );

    if ( pressed )
    {
        *v = !*v;
        info->background = t.overlay_pressed;
    }

    window->DrawList->AddRectFilled( bb.Min, bb.Max, color_t( info->background ).modulate( style.Alpha ), 8 );

    info->text = ImLerp( info->text, hovered ? t.text : t.text_bright, 20 * g.IO.DeltaTime );

    ImRect const check_rect = {
        { bb.Max.x - 26 - 8, bb.GetCenter( ).y - 8 },
        { bb.Max.x - 8, bb.GetCenter( ).y + 8 } };

    std_elem::toggle( id + 100, v, toggle_info, check_rect );

    ImVec2 const icon_size = icon.has_value( ) ? font.calculate_text_size( icon.value( ) ) : ImVec2( 0, 0 );

    if ( icon.has_value( ) )
        window->DrawList->AddText( { bb.Min.x + 8, bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), icon.value( ).c_str( ) );

    window->DrawList->AddText( { bb.Min.x + 8 + ( icon.has_value( ) ? ( 8 + icon_size.x ) : 0.f ), bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), label.c_str( ) );

    return pressed;
}

alice::expander_info_t* c_popup_elem::current_expander = nullptr;

bool c_popup_elem::begin_expander( std::optional< std::string > const& icon, std::string const& label, float x, float width )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( label.c_str( ) );
    auto const font = core->fonts->get( "primary" );
    ImVec2 const label_size = font.calculate_text_size( label );

    ImVec2 const pos = window->DC.CursorPos;
    ImVec2 const size = { GetContentRegionAvail( ).x, k_elem_height };

    ImRect const bb( pos, pos + size );
    ItemSize( size, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    bool hovered, held;
    ButtonBehavior( bb, id, &hovered, &held );

    auto* info = c_elem_animations::instance( ).get< expander_info_t >( window->Name, id );

    if ( !info->initialized )
    {
        info->popup.set_title( "##expander_" + label );
        info->popup.set_width( width );
        info->popup.get_window_animator( )->set_pivot( { 0.0f, 0.0f } );
        info->initialized = true;
    }

    bool const popup_open = info->popup.is_open( );

    if ( hovered )
    {
        if ( info->popup.open_allowed( ) )
        {
            info->popup.set_position( { bb.Max.x + x, bb.Min.y } );
            info->popup.open( );
        }
    } else if ( popup_open && g.HoveredId != 0 && g.HoveredId != id && g.HoveredWindow == window )
        info->popup.close( );
    auto const animation_time = 20 * g.IO.DeltaTime;

    auto const& t = c_themes::palette( );

    info->background = ImLerp( info->background, ( hovered || popup_open ) ? t.overlay_hover : with_alpha( t.overlay_hover, 0.f ), animation_time );
    info->text = ImLerp( info->text, ( hovered || popup_open ) ? t.text : t.text_bright, animation_time );
    info->arrow = ImLerp( info->arrow, ( hovered || popup_open ) ? t.text : t.text_mid, animation_time );

    window->DrawList->AddRectFilled( bb.Min, bb.Max, color_t( info->background ).modulate( style.Alpha ), 8 );

    ImVec2 const icon_size = icon.has_value( ) ? font.calculate_text_size( icon.value( ) ) : ImVec2( 0, 0 );

    if ( icon.has_value( ) )
        window->DrawList->AddText( { bb.Min.x + 8, bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), icon.value( ).c_str( ) );

    window->DrawList->AddText( { bb.Min.x + 8 + ( icon.has_value( ) ? ( 8 + icon_size.x ) : 0.f ), bb.GetCenter( ).y - label_size.y / 2 }, color_t( info->text ).modulate( style.Alpha ), label.c_str( ) );

    static std::string const chevron = ICON_LUCIDE_CHEVRON_RIGHT;
    auto const chevron_size = font.calculate_text_size( chevron );
    window->DrawList->AddText( { bb.Max.x - 6 - chevron_size.x, bb.GetCenter( ).y - chevron_size.y / 2 }, color_t( info->arrow ).modulate( style.Alpha ), chevron.c_str( ) );

    if ( info->popup.begin( ) )
    {
        info->parent = c_popup_elem::current_expander;
        c_popup_elem::current_expander = info;
        return true;
    }

    return false;
}

void c_popup_elem::end_expander( )
{
    if ( !c_popup_elem::current_expander )
        return;
    auto* info = c_popup_elem::current_expander;
    c_popup_elem::current_expander = info->parent;
    info->parent = nullptr;
    info->popup.end( );
}