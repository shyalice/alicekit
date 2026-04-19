//
// Created by rei on 4/9/2026.
//

#include "elem.hpp"

#include "alice_kit.hpp"
#include "elem_animations/elem_animations.hpp"
#include "renderer/renderer.hpp"
#include "standalone_elem/standalone_elem.hpp"
#include "themes/themes.hpp"
#include "tooltips/tooltips.hpp"
#include "ui/ui.hpp"
#include "util/util.hpp"

#include <algorithm>
#include <iostream>
#include <ostream>
#include <type_traits>
#include <random>
#include <array>

using alice::c_ui_elements;
using alice::font_t;
using alice::item_builder;

using namespace ImGui;

font_t c_ui_elements::font { };
std::vector< int > c_ui_elements::group_item_stack { };
std::optional< bool > c_ui_elements::is_disabled { std::nullopt };

void c_ui_elements::next_item_separator( )
{
    if ( group_item_stack.empty( ) )
        return;

    auto& count = group_item_stack.back( );
    if ( count++ == 0 )
        return;

    auto* window = GetCurrentWindow( );
    auto const alpha = GetStyle( ).Alpha;
    auto const y = window->DC.CursorPos.y;

    auto const& t = c_themes::palette( );

    window->DrawList->AddLine(
        { window->Pos.x + 1, y },
        { window->Pos.x + window->Size.x - 1, y },
        color_t( t.border ).modulate( alpha ),
        1.f );

    ItemSize( ImVec2( 0, 1 ) );
}

void c_ui_elements::prepare( )
{
    font = core->fonts->get( "primary" );
}

void c_ui_elements::push_disabled( bool disabled )
{
    is_disabled = disabled;
}

void c_ui_elements::pop_disabled( )
{
    is_disabled = std::nullopt;
}

std::vector< c_ui_elements::visible_stack_entry_t > c_ui_elements::visible_stack { };

void c_ui_elements::push_visible( std::string const& id, bool visible )
{
    auto* window = GetCurrentWindow( );
    auto const key = window->GetID( ( id + "##visible" ).c_str( ) );
    auto* info = c_elem_animations::instance( ).get< visible_info_t >( window->Name, key );

    if ( !info->initialized )
    {
        info->alpha = visible ? 1.f : 0.f;
        info->current_h = visible ? info->natural_h : 0.f;
        info->initialized = true;
    }
    info->target = visible ? 1.f : 0.f;

    constexpr float k_duration = 0.18f;
    float const dt = GetIO( ).DeltaTime;
    float const alpha_step = dt / k_duration;
    float const height_step = std::max( info->natural_h, 1.f ) * alpha_step;

    auto const move_toward = []( float current, float target, float step ) {
        return current < target ? std::min( current + step, target )
                                : std::max( current - step, target );
    };

    float const target_h = visible ? info->natural_h : 0.f;
    info->alpha = move_toward( info->alpha, info->target, alpha_step );
    info->current_h = move_toward( info->current_h, target_h, height_step );

    ImVec2 const start_cursor = window->DC.CursorPos;
    float const start_max_y = window->DC.CursorMaxPos.y;

    bool const needs_measure = info->natural_h < 0.5f;
    float const clip_h = needs_measure ? 9999.f : std::max( info->current_h, 0.f );

    auto const& parent_clip = window->ClipRect;

    PushStyleVar( ImGuiStyleVar_Alpha, GetStyle( ).Alpha * info->alpha );
    PushClipRect( { parent_clip.Min.x, start_cursor.y }, { parent_clip.Max.x, start_cursor.y + clip_h }, true );
    BeginDisabled( !visible );

    visible_stack.push_back( { info, visible, start_cursor, start_max_y } );
}

void c_ui_elements::pop_visible( )
{
    auto const entry = visible_stack.back( );
    visible_stack.pop_back( );

    auto* window = GetCurrentWindow( );
    float const content_h = window->DC.CursorPos.y - entry.start_cursor.y;

    EndDisabled( );
    PopClipRect( );
    PopStyleVar( );

    bool const needs_measure = entry.info->natural_h < 0.5f;
    bool const fully_visible = entry.visible && entry.info->alpha >= 0.99f && entry.info->target >= 0.99f;

    if ( needs_measure )
    {
        entry.info->natural_h = content_h;
        entry.info->current_h = entry.visible ? content_h : 0.f;
        entry.info->alpha = entry.visible ? 1.f : 0.f;
    }
    else if ( fully_visible )
    {
        entry.info->natural_h = content_h;
    }

    float const clamped_h = std::min( content_h, std::max( entry.info->current_h, 0.f ) );
    window->DC.CursorPos = { entry.start_cursor.x, entry.start_cursor.y + clamped_h };
    window->DC.CursorMaxPos.y = std::max( entry.start_max_y, window->DC.CursorPos.y );
}

void c_ui_elements::visible_if( std::string const& id, bool visible, std::function< void( ) > const& body )
{
    push_visible( id, visible );
    body( );
    pop_visible( );
}

void c_ui_elements::begin_group( std::string const& name, ImVec2 const& size, bool absolute, ImGuiWindowFlags flags )
{
    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

    ItemSize( { 0, 15 + 18 } );

    auto* draw_list = GetWindowDrawList( );

    auto const cur = GetCursorScreenPos( ) - ImVec2( 0, 18 );
    auto const small_font = core->fonts->get( "small" );
    auto const& t = c_themes::palette( );
    draw_list->AddText( small_font.ptr, small_font.size, { cur.x + 15, cur.y }, color_t( t.text_hint ).modulate( ), name.c_str( ) );

    SetNextWindowSizeConstraints( { size.x, 0 }, { size.x, size.y } );
    BeginChild( name.c_str( ), { size.x, absolute ? ( size.y - 15 - 18 ) : 0 }, ImGuiChildFlags_AlwaysAutoResize | ImGuiChildFlags_AutoResizeY, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | flags );
    auto* window = GetCurrentWindow( );

    group_item_stack.push_back( 0 );

    core->ui->get_window_animator( )->capture( draw_list );
    core->ui->get_window_animator( )->capture( window->DrawList );

    renderer_t::add_rect_filled( draw_list, window->Pos - ImVec2( 0, 1 ), window->Pos + window->Size + ImVec2( 0, 1 ), color_t( t.card_bg ).modulate( GetStyle( ).Alpha ), 12, ImDrawFlags_RoundCornersAll, 5, 3.f );
    renderer_t::add_rect( draw_list, window->Pos - ImVec2( 0, 1 ), window->Pos + window->Size + ImVec2( 0, 1 ), color_t( t.border ).modulate( GetStyle( ).Alpha ), 1, 12, ImDrawFlags_RoundCornersAll, 5, 3.f );
}

void c_ui_elements::end_group( )
{
    if ( !group_item_stack.empty( ) )
        group_item_stack.pop_back( );

    PopStyleVar( );
    EndChild( );
}

bool c_ui_elements::select_button( std::string const& name, bool* v, ImVec2 const& size_arg, std::function< void( ) > const& rmb_callback )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return false;

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( name.c_str( ) );
    auto const label_size = font.calculate_text_size( name );

    ImVec2 const pos = window->DC.CursorPos;
    ImVec2 const size = CalcItemSize( size_arg, label_size.x + 14, label_size.y + 12 );

    ImRect const bb( pos, pos + size );
    ItemSize( size, style.FramePadding.y );
    if ( !ItemAdd( bb, id ) )
        return false;

    bool hovered, held;
    bool pressed = ButtonBehavior( bb, id, &hovered, &held, 0 );

    if ( pressed )
        *v = !*v;

    bool rmb_fired = false;
    if ( hovered && rmb_callback && IsMouseClicked( ImGuiMouseButton_Right ) )
    {
        rmb_callback( );
        rmb_fired = true;
    }

    auto* info = c_elem_animations::instance( ).get< select_button_info_t >( window->Name, id );
    auto const& t = c_themes::palette( );

    auto const animation_time = GetIO( ).DeltaTime * 20;

    info->background = ImLerp( info->background, *v ? t.accent_bg : ( hovered ? t.control_hover : t.control ), animation_time );
    info->outline = ImLerp( info->outline, *v ? t.accent_strong : ( hovered ? t.text_muted : t.text_faint ), animation_time );
    info->text = ImLerp( info->text, *v ? t.text : ( hovered ? t.text_muted : t.text_faint ), animation_time );

    window->DrawList->AddRectFilled( bb.Min, bb.Max, color_t( info->background ).modulate( ), 6 );
    window->DrawList->AddRect( bb.Min, bb.Max, color_t( info->outline ).modulate( ), 6 );

    window->DrawList->AddText( bb.GetCenter( ) - label_size / 2, color_t( info->text ).modulate( ), name.c_str( ) );

    return rmb_fired;
}

item_builder item_builder::add_tooltip( const std::string& text, bool force )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    ImGuiID const id = window->GetID( ( name + "##tooltip" ).c_str( ) );
    auto* info = c_elem_animations::instance( ).get< tooltip_info_t >( window->Name, id );

    constexpr float k_tooltip_delay = 0.5f;

    constexpr float k_idle_threshold = 2.f;

    auto const& io = GetIO( );
    bool const mouse_over = IsWindowHovered( ) && IsMouseHoveringRect( rect.Min, rect.Max );
    bool const mouse_moved = ImFabs( io.MouseDelta.x ) + ImFabs( io.MouseDelta.y ) > k_idle_threshold;
    info->hover_time = !mouse_over || mouse_moved ? 0.f : info->hover_time + io.DeltaTime;
    info->position = { rect.Max.x + 20, rect.GetCenter( ).y };
    info->hovered = force ? mouse_over : info->hover_time >= k_tooltip_delay;

    c_tooltips::simple( info, text );

    return *this;
}

item_builder item_builder::add_tooltip( const std::string& title, const std::vector< std::pair< ImColor, std::string > >& lines, bool force )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    ImGuiID const id = window->GetID( ( name + "##tooltip" ).c_str( ) );
    auto* info = c_elem_animations::instance( ).get< tooltip_info_t >( window->Name, id );

    constexpr float k_tooltip_delay = 0.5f;

    constexpr float k_idle_threshold = 2.f;

    auto const& io = GetIO( );
    bool const mouse_over = IsWindowHovered( ) && IsMouseHoveringRect( rect.Min, rect.Max );
    bool const mouse_moved = ImFabs( io.MouseDelta.x ) + ImFabs( io.MouseDelta.y ) > k_idle_threshold;
    info->hover_time = !mouse_over || mouse_moved ? 0.f : info->hover_time + io.DeltaTime;
    info->position = { rect.Max.x + 20, rect.GetCenter( ).y };
    info->hovered = force ? mouse_over : info->hover_time >= k_tooltip_delay;

    c_tooltips::standard( info, title, lines );

    return *this;
}

item_builder item_builder::add_toggle( bool* v )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    ImGuiID const id = window->GetID( ( name + "##toggle" ).c_str( ) );
    ImRect const check_rect = {
        { rect.Max.x - 26, rect.GetCenter( ).y - 8 },
        { rect.Max.x, rect.GetCenter( ).y + 8 } };

    std_elem::toggle( id, v, c_elem_animations::instance( ).get< toggle_info_t >( window->Name, id ), check_rect, ( basic_info != nullptr ) ? std::make_optional( std::make_pair( basic_info->disabled, basic_info->disable_mod ) ) : std::nullopt );

    rect.Max.x -= 26 + k_elem_spacing;

    return *this;
}

item_builder item_builder::add_dropdown( int* v, std::vector< std::string > const& entries )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    ImGuiID const id = window->GetID( ( name + "##dropdown" ).c_str( ) );
    ImRect const dropdown_rect = {
        { rect.Max.x - 120, rect.GetCenter( ).y - 11 },
        { rect.Max.x, rect.GetCenter( ).y + 11 } };

    c_elem_animations::instance( ).insert_if_empty< dropdown_info_t >( window->Name, id, dropdown_info_t { .random_title = []( ) -> char const* {
                                                                           static thread_local std::mt19937 rng { std::random_device { }( ) };
                                                                           static thread_local std::uniform_int_distribution< int > dist( 'a', 'z' );
                                                                           static thread_local std::array< char, 11 > buf { };

                                                                           for ( int i = 0; i < 10; ++i )
                                                                               buf[ i ] = static_cast< char >( dist( rng ) );
                                                                           buf[ 10 ] = '\0';
                                                                           return buf.data( );
                                                                       }( ) } );

    std_elem::dropdown( id, c_elem_animations::instance( ).get< dropdown_info_t >( window->Name, id ), dropdown_rect, v, entries );

    rect.Max.x -= 120 + k_elem_spacing;

    return *this;
}

item_builder item_builder::add_color( ImVec4* v )
{
    return add_color( { v, std::nullopt } );
}

item_builder item_builder::add_color( std::pair< ImVec4*, std::optional< ImVec4* > > v )
{
    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    ImGuiID const id = window->GetID( ( name + "##color" ).c_str( ) );
    ImRect const color_rect = {
        { rect.Max.x - 16, rect.GetCenter( ).y - 8 },
        { rect.Max.x, rect.GetCenter( ).y + 8 } };

    std_elem::color_button( id, c_elem_animations::instance( ).get< color_info_t >( window->Name, id ), color_rect, v );

    rect.Max.x -= 16 + k_elem_spacing;

    return *this;
}

item_builder c_ui_elements::start( std::string const& label )
{
    item_builder result { label };

    core->ui->try_register_search( label );

    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return result;

    next_item_separator( );

    ImGuiContext& g = *GImGui;
    ImGuiStyle const& style = g.Style;
    ImGuiID const id = window->GetID( label.c_str( ) );
    auto const label_size = font.calculate_text_size( label );

    ImVec2 const pos = window->DC.CursorPos;
    ImRect const total_bb( pos, pos + ImVec2( GetContentRegionAvail( ).x, k_elem_height ) );

    ItemSize( total_bb, 0 );

    bool const is_visible = ItemAdd( total_bb, id );
    if ( !is_visible )
        return result;

    auto* info = c_elem_animations::instance( ).get< basic_elem_info_t >( window->Name, id );

    info->disabled = is_disabled.value_or( false );
    info->disable_mod = ImLerp( info->disable_mod, info->disabled ? 1.f : 0.f, 20 * g.IO.DeltaTime );

    auto const& t = c_themes::palette( );
    window->DrawList->AddText( { total_bb.Min.x + 15, total_bb.GetCenter( ).y - label_size.y / 2 }, color_t( t.text ).modulate( style.Alpha * ( 1.0f - ( 0.4f * info->disable_mod ) ) ), label.c_str( ) );

    result.rect = { total_bb.Min, { total_bb.Max.x - 15, total_bb.Max.y } };
    result.basic_info = info;

    return result;
}

template < typename T >
item_builder item_builder::add_slider( T* v, T v_min, T v_max, char const* format, bool has_input )
{
    static_assert( std::is_same_v< T, float > || std::is_same_v< T, int >, "add_slider only supports float and int" );

    ImGuiWindow* window = GetCurrentWindow( );
    if ( window->SkipItems )
        return *this;

    if ( !format )
        format = std::is_same_v< T, float > ? "%.2f" : "%d";

    ImGuiID const id = window->GetID( ( name + "##slider" ).c_str( ) );
    int const slider_w = 120;
    constexpr float input_gap = 10.f;
    ImRect const slider_rect = {
        { rect.Max.x - slider_w, rect.GetCenter( ).y - 2 },
        { rect.Max.x, rect.GetCenter( ).y + 2 } };

    auto* info = c_elem_animations::instance( ).get< slider_info_t >( window->Name, id );
    std_elem::slider( id, info, slider_rect, v, v_min, v_max, format, has_input );

    float const input_consume = has_input ? ( info->input_w + input_gap ) : 0.f;
    rect.Max.x -= slider_w + input_consume + k_elem_spacing;
    return *this;
}

template item_builder item_builder::add_slider< int >( int*, int, int, char const*, bool );
template item_builder item_builder::add_slider< float >( float*, float, float, char const*, bool );
