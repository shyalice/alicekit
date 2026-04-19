//
// Created by rei on 4/12/2026.
//

#include "theme_editor.hpp"

#include "alice_kit.hpp"
#include "custom_scrollbar/custom_scrollbar.hpp"
#include "elem_animations/elem_animations.hpp"
#include "fonts/fonts.hpp"
#include "fonts/lucide_defines.hpp"
#include "popups/popup_elem/popup_elem.hpp"
#include "standalone_elem/standalone_elem.hpp"
#include "ui/ui.hpp"
#include "ui/elem/elem.hpp"
#include "util/util.hpp"
#include "windows/windows.hpp"

#include <charconv>
#include <cstring>

using alice::c_theme_editor;
using alice::c_themes;
using alice::c_ui_elements;
using alice::c_window_animator;
using alice::color_t;
using alice::theme_palette_t;
using alice::with_alpha;

using namespace ImGui;

std::string c_theme_editor::generate_name( )
{
    int n = 1;
    for ( int i = 0; i < c_themes::count( ); ++i )
    {
        auto const& nm = c_themes::name( i );
        if ( nm.starts_with( "Custom " ) )
        {
            int v = 0;
            auto sub = nm.substr( 7 );
            auto [ptr, ec] = std::from_chars( sub.data( ), sub.data( ) + sub.size( ), v );
            if ( ec == std::errc( ) && v >= n )
                n = v + 1;
        }
    }
    return "Custom " + std::to_string( n );
}

theme_palette_t c_theme_editor::draft { };
theme_palette_t c_theme_editor::backup { };
int c_theme_editor::editing_idx = -1;
bool c_theme_editor::pending_open = false;
bool c_theme_editor::cancelling = false;
char c_theme_editor::name_buf[ 128 ] { };

alice::c_custom_scrollbar c_theme_editor::scrollbar;
float c_theme_editor::sb_inset = 0.f;

struct field_group_t
{
    char const* label;
    int begin;
    int end;
};

static constexpr field_group_t k_groups[] = {
    { "BACKGROUND", 0, 9 },
    { "CONTROLS", 9, 14 },
    { "BORDERS", 14, 20 },
    { "TEXT", 20, 31 },
    { "PILL", 31, 33 },
    { "SHADOW", 33, 34 },
    { "ACCENT", 34, 41 },
    { "DANGER", 41, 43 },
    { "OVERLAYS", 43, 47 },
};

void c_theme_editor::open( int idx )
{
    if ( idx < 0 || idx >= c_themes::count( ) )
        return;

    if ( editing_idx >= 0 )
        cancel( );

    editing_idx = idx;
    c_themes::set( idx );
    draft = c_themes::at( idx );
    backup = draft;
    std::strncpy( name_buf, draft.name.c_str( ), sizeof( name_buf ) - 1 );
    name_buf[ sizeof( name_buf ) - 1 ] = '\0';
    cancelling = false;
    pending_open = true;
}

void c_theme_editor::cancel( )
{
    if ( editing_idx >= 0 && editing_idx < c_themes::count( ) )
        c_themes::at( editing_idx ) = backup;
    cancelling = true;
    core->windows->close( "theme_editor" );
}

void c_theme_editor::save( )
{
    if ( editing_idx < 0 || editing_idx >= c_themes::count( ) )
        return;

    std::string const old_name = c_themes::at( editing_idx ).name;

    draft.name = name_buf;
    draft.builtin = false;
    c_themes::at( editing_idx ) = draft;
    backup = draft;

    if ( old_name != draft.name )
        c_themes::delete_theme_file( old_name );

    c_themes::save_theme( editing_idx );
    cancelling = true;
    core->windows->close( "theme_editor" );
}

bool c_theme_editor::is_open( )
{
    return editing_idx >= 0;
}

int c_theme_editor::editing( )
{
    return editing_idx;
}

void c_theme_editor::render( )
{
    if ( pending_open )
    {
        core->windows->open( "theme_editor", { GetIO( ).DisplaySize / 2 }, { 0.5f, 0.5f } );
        pending_open = false;
    }

    if ( editing_idx < 0 || editing_idx >= c_themes::count( ) )
    {
        editing_idx = -1;
        return;
    }

    if ( !core->windows->begin( "theme_editor", "Theme Editor", { 320, 520 } ) )
    {
        editing_idx = -1;
        cancelling = false;
        return;
    }

    auto* win_anim = core->windows->current_animator( );

    auto capture = [ & ]( ) {
        core->ui->get_window_animator( )->capture( GetWindowDrawList( ) );
        if ( win_anim )
            win_anim->capture( GetWindowDrawList( ) );
    };

    capture( );

    auto const& t = c_themes::palette( );
    auto const font = core->fonts->get( "primary" );
    auto const alpha = GetStyle( ).Alpha;
    auto const avail_w = GetContentRegionAvail( ).x;

    PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );

    {
        PushStyleColor( ImGuiCol_FrameBg, t.control );
        PushStyleColor( ImGuiCol_FrameBgHovered, t.control_hover );
        PushStyleColor( ImGuiCol_FrameBgActive, t.control_active );
        PushStyleColor( ImGuiCol_Border, t.border );
        PushStyleColor( ImGuiCol_Text, t.text );
        PushStyleColor( ImGuiCol_TextSelectedBg, t.accent_bg );
        PushStyleVar( ImGuiStyleVar_FrameRounding, 4.f );
        PushStyleVar( ImGuiStyleVar_FramePadding, { 8, 6 } );
        PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1.f );
        PushItemWidth( avail_w );

        InputText( "##theme_name", name_buf, sizeof( name_buf ) );

        PopItemWidth( );
        PopStyleVar( 3 );
        PopStyleColor( 6 );

        ItemSize( { 0, 8 } );
    }

    float const fields_h = GetContentRegionAvail( ).y - 36;

    BeginChild( "##editor_fields", { avail_w, fields_h }, 0, ImGuiWindowFlags_NoScrollbar );
    {
        capture( );

        auto* fields_window = GetCurrentWindow( );

        {
            constexpr float k_sb_reserve = 4.f + 4.f;
            float const sb_target = c_theme_editor::scrollbar.get_fade( ) > 0.01f ? k_sb_reserve : 0.f;
            c_theme_editor::sb_inset = ImLerp( c_theme_editor::sb_inset, sb_target, 20.f * GetIO( ).DeltaTime );
            fields_window->ContentRegionRect.Max.x -= c_theme_editor::sb_inset;
            fields_window->WorkRect.Max.x -= c_theme_editor::sb_inset;
        }

        auto const* fields = alice::theme_fields( );
        auto const field_count = alice::theme_field_count( );

        for ( auto const& group : k_groups )
        {
            int const actual_end = group.end <= field_count ? group.end : field_count;

            c_ui_elements::begin_group( group.label, { GetContentRegionAvail( ).x, 99999.f } );
            capture( );

            for ( int i = group.begin; i < actual_end; ++i )
            {
                auto& f = fields[ i ];
                auto* col = reinterpret_cast< ImVec4* >( reinterpret_cast< char* >( &draft ) + f.offset );
                c_ui_elements::start( f.label ).add_color( col ).finish( );
            }

            c_ui_elements::end_group( );
        }

        ItemSize( { 0, 15 } );
        c_theme_editor::scrollbar.render( fields_window, { -2, 8 }, true, 4.f );

        EndChild( );
    }

    {
        constexpr float btn_h = 30.f;
        constexpr float btn_gap = 8.f;
        float const btn_w = ( avail_w - btn_gap ) / 2.f;
        float const anim_speed = 20.f * GetIO( ).DeltaTime;

        ImVec2 const cursor = GetCursorScreenPos( );
        auto* dl = GetWindowDrawList( );

        ImRect const save_rect = { cursor + ImVec2( 0, 4 ), cursor + ImVec2( btn_w, 4 + btn_h ) };
        ImRect const cancel_rect = { cursor + ImVec2( btn_w + btn_gap, 4 ), cursor + ImVec2( avail_w, 4 + btn_h ) };

        struct btn_anim_t { ImVec4 bg { }; };
        static btn_anim_t save_a { };
        static btn_anim_t cancel_a { };

        auto draw_btn = [ & ]( char const* id, ImRect const& rect, btn_anim_t& anim,
                               ImVec4 const& bg, ImVec4 const& bg_hover, ImVec4 const& bg_active,
                               ImVec4 const& tc, ImVec4 const& bc, char const* label ) -> bool {
            SetCursorScreenPos( rect.Min );
            bool const clicked = InvisibleButton( id, rect.GetSize( ) );
            bool const hovered = IsItemHovered( );
            bool const held = IsItemActive( );

            anim.bg = ImLerp( anim.bg, held ? bg_active : ( hovered ? bg_hover : bg ), anim_speed );

            dl->AddRectFilled( rect.Min, rect.Max, color_t( anim.bg ).modulate( alpha ), 6 );
            dl->AddRect( rect.Min, rect.Max, color_t( bc ).modulate( alpha ), 6 );

            auto const sz = font.calculate_text_size( label );
            dl->AddText( rect.GetCenter( ) - sz / 2, color_t( tc ).modulate( alpha ), label );

            return clicked;
        };

        if ( draw_btn( "##save", save_rect, save_a, t.accent_surface, t.accent_border, t.accent_strong, ImVec4( 1, 1, 1, 1 ), t.accent_border, "Save" ) )
            save( );

        if ( draw_btn( "##cancel", cancel_rect, cancel_a, t.control, t.control_hover, t.control_active, t.text_secondary, t.border, "Cancel" ) )
            cancel( );

        ItemSize( { avail_w, btn_h + 8 } );
    }

    PopStyleVar( );

    if ( editing_idx >= 0 && editing_idx < c_themes::count( ) && !cancelling )
        c_themes::at( editing_idx ) = draft;

    core->windows->end( );

    if ( !core->windows->is_open( "theme_editor" ) && !cancelling && editing_idx >= 0 )
        cancel( );
}
