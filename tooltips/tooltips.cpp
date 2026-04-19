//
// Created by rei on 4/14/2026.
//

#include "tooltips.hpp"

#include "renderer/renderer.hpp"
#include "alice_kit.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"
#include "fonts/fonts.hpp"

using alice::c_tooltips;
using alice::tooltip_info_t;

using namespace ImGui;

void c_tooltips::render_frame( const ImRect& rect, float alpha )
{
    auto* draw = GetWindowDrawList( );

    auto const& t = c_themes::palette( );

    draw->PushClipRectFullScreen( );
    renderer_t::add_shadow( draw, rect.Min, rect.Max, color_t( t.shadow ).modulate( alpha ), 8, 16, ImDrawFlags_RoundCornersAll, 5, 3.f, 20 );
    core->blur->apply_squircle( draw, rect.Min, rect.Max, 18, 10, 5, 3.f, 2, alpha );
    renderer_t::add_rect_filled( draw, rect.Min, rect.Max, color_t( t.popup_bg ).modulate( alpha ), 10, ImDrawFlags_RoundCornersAll, 5, 3.f );
    renderer_t::add_rect( draw, rect.Min, rect.Max, color_t( t.border ).modulate( alpha ), 1, 10, ImDrawFlags_RoundCornersAll, 5, 3.f );
    draw->PopClipRect( );
}

bool c_tooltips::window_backend( tooltip_info_t* info, const std::string& title, const ImVec2& pos )
{
    if ( info->mod < 0.001f )
        return false;

    constexpr float k_max_width = 260.f;

    SetNextWindowPos( pos, ImGuiCond_Always, { 0.f, 0.5f } );
    SetNextWindowSizeConstraints( { 1, 1 }, { k_max_width, FLT_MAX } );

    auto const orig_alpha = GetStyle( ).Alpha;

    PushStyleVar( ImGuiStyleVar_WindowPadding, { 10, 10 } );
    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 10, 5 } );
    PushStyleVar( ImGuiStyleVar_Alpha, info->mod * orig_alpha );

    Begin( ( "##tooltip:" + title ).c_str( ), nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs );

    auto* window = GetCurrentWindow( );

    PushTextWrapPos( GetCursorPosX( ) + k_max_width - GetStyle( ).WindowPadding.x * 2.f );

    render_frame( window->Rect( ), info->mod * orig_alpha );

    return true;
}

void c_tooltips::standard( tooltip_info_t* info, const std::string& title, const std::vector< std::pair< ImColor, std::string > >& lines )
{
    info->mod = ImLerp( info->mod, info->hovered ? 1.f : 0.f, 20 * GetIO( ).DeltaTime );

    if ( !window_backend( info, title, info->position ) )
        return;

    auto const bold_font = core->fonts->get( "bold" );
    auto const norm_font = core->fonts->get( "primary" );

    PushFont( bold_font.ptr );
    TextUnformatted( title.c_str( ) );
    PopFont( );

    for ( auto const& line : lines )
    {
        PushStyleColor( ImGuiCol_Text, line.first.Value );
        TextUnformatted( line.second.c_str( ) );
        PopStyleColor( );
    }

    PopTextWrapPos( );
    End( );
    PopStyleVar( 3 );
}

void c_tooltips::simple( tooltip_info_t* info, const std::string& text )
{
    info->mod = ImLerp( info->mod, info->hovered ? 1.f : 0.f, 20 * GetIO( ).DeltaTime );

    if ( !window_backend( info, text, info->position ) )
        return;

    TextUnformatted( text.c_str( ) );

    PopTextWrapPos( );
    End( );
    PopStyleVar( 3 );
}