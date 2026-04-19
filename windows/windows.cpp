#include "windows.hpp"

#include "alice_kit.hpp"
#include "fonts/fonts.hpp"
#include "renderer/renderer.hpp"
#include "themes/themes.hpp"
#include "ui/ui.hpp"
#include "util/util.hpp"

using alice::c_window_animator;
using alice::c_windows;
using alice::window_state_t;

void c_windows::open( std::string const& id, ImVec2 const& spawn_pos, ImVec2 const& pivot )
{
    auto& s = states[ id ];
    s.open = true;
    s.spawn_position = spawn_pos;
    s.position = spawn_pos;
    s.focus_pending = true;
    s.animator.open( );
    if ( !s.animator.visible( ) )
        s.animator.update( ImGui::GetIO( ).DeltaTime );
}

void c_windows::close( std::string const& id )
{
    auto it = states.find( id );
    if ( it == states.end( ) )
        return;

    it->second.open = false;
    it->second.animator.close( );
}

void c_windows::toggle( std::string const& id, ImVec2 const& spawn_pos, ImVec2 const& pivot )
{
    auto& s = states[ id ];
    if ( s.open )
    {
        s.open = false;
        s.animator.close( );
    } else
    {
        s.open = true;
        s.spawn_position = spawn_pos;
        s.position = spawn_pos;
        s.focus_pending = true;
        s.animator.open( );
    }
}

bool c_windows::is_open( std::string const& id ) const
{
    auto it = states.find( id );
    return it != states.end( ) && it->second.open;
}

void c_windows::update( float dt )
{
    for ( auto& [ id, s ] : states )
        s.animator.update( dt );
}

bool c_windows::begin( std::string const& id, std::string const& title, ImVec2 const& size )
{
    auto& s = states[ id ];
    current_id = id;
    current_size = size;

    if ( !s.animator.visible( ) )
    {
        s.focus_pending = true;
        return false;
    }

    auto const& io = ImGui::GetIO( );
    auto const font = core->fonts->get( "primary" );
    ImVec2 const total_size = { size.x, size.y + k_title_h };

    ImVec2 draw_pos = s.position - total_size * 0.5f;

    s.animator.begin( );

    ImGui::SetNextWindowPos( draw_pos );
    ImGui::SetNextWindowSize( total_size );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowBorderSize, 0.f );
    ImGui::PushStyleColor( ImGuiCol_WindowBg, ImVec4( 0, 0, 0, 0 ) );
    ImGui::PushStyleColor( ImGuiCol_ChildBg, ImVec4( 0, 0, 0, 0 ) );
    ImGui::PushStyleColor( ImGuiCol_Border, ImVec4( 0, 0, 0, 0 ) );

    auto flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoNav;
    if ( !s.open )
        flags |= ImGuiWindowFlags_NoInputs;

    ImGui::Begin( ( "##win_" + id ).c_str( ), nullptr, flags );

    auto* window = ImGui::GetCurrentWindow( );
    auto* dl = window->DrawList;

    core->ui->get_window_animator( )->capture( dl );
    s.animator.capture( dl );

    ImVec2 const win_pos = window->Pos;

    auto const& t = c_themes::palette( );

    dl->PushClipRectFullScreen( );
    renderer_t::add_shadow( dl, win_pos, win_pos + total_size,
                            color_t( t.shadow ).modulate( ImGui::GetStyle( ).Alpha ),
                            8, 16, ImDrawFlags_RoundCornersAll, 5, 3.f, 20 );
    {
        float const s_scale = s.animator.scale( );
        ImVec2 const blur_min = win_pos;
        ImVec2 const blur_max = win_pos + total_size;
        ImVec2 const blur_center = ( blur_min + blur_max ) * 0.5f;
        ImVec2 const sm = blur_center + ( blur_min - blur_center ) * s_scale;
        ImVec2 const sx = blur_center + ( blur_max - blur_center ) * s_scale;
        core->blur->apply_squircle( dl, sm, sx, 18, k_rounding, 5, 3.f, 2, ImGui::GetStyle( ).Alpha );
    }
    renderer_t::add_rect_filled( dl, win_pos, win_pos + total_size,
                                 color_t( t.card_bg ).modulate( ), k_rounding,
                                 ImDrawFlags_RoundCornersAll, 5, 3.f );
    renderer_t::add_rect_filled( dl, win_pos, { win_pos.x + total_size.x, win_pos.y + k_title_h },
                                 color_t( t.title_bg ).modulate( ), k_rounding,
                                 ImDrawFlags_RoundCornersTop, 5, 3.f );
    dl->PopClipRect( );

    if ( s.focus_pending )
    {
        ImGui::BringWindowToDisplayFront( window );
        s.focus_pending = false;
    }

    ImRect const title_rect = { win_pos, { win_pos.x + total_size.x, win_pos.y + k_title_h } };

    dl->AddText( font.ptr, font.size,
                 { win_pos.x + k_pad, title_rect.GetCenter( ).y - font.size * 0.5f },
                 color_t( t.text_secondary ).modulate( ), title.c_str( ) );

    ImVec2 const close_center = { win_pos.x + total_size.x - k_pad - k_close_radius,
                                  title_rect.GetCenter( ).y };
    ImRect const close_rect = { close_center - ImVec2( k_close_radius, k_close_radius ),
                                close_center + ImVec2( k_close_radius, k_close_radius ) };

    ImGuiContext& g = *GImGui;
    bool const is_top_window = g.HoveredWindow == window;
    bool const close_hovered = s.open && is_top_window && close_rect.Contains( io.MousePos );

    static std::unordered_map< std::string, float > close_hover_anims;
    auto& close_anim = close_hover_anims[ id ];
    close_anim = ImLerp( close_anim, close_hovered ? 1.f : 0.f, 20.f * io.DeltaTime );

    ImU32 const close_bg = color_t( with_alpha( t.danger, ( 40.f / 255.f ) * close_anim ) ).modulate( );
    dl->AddCircleFilled( close_center, k_close_radius, close_bg, 32 );

    constexpr auto x_icon = "\xee\x86\xb2";
    auto const x_size = font.calculate_text_size( x_icon );
    ImVec4 const close_icon_col = ImLerp( t.text_muted, t.danger, close_anim );
    dl->AddText( font.ptr, font.size,
                 close_center - x_size * 0.5f,
                 color_t( close_icon_col ).modulate( ),
                 x_icon );

    if ( close_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
    {
        s.open = false;
        s.animator.close( );
    }

    dl->AddLine( { win_pos.x, win_pos.y + k_title_h },
                 { win_pos.x + total_size.x, win_pos.y + k_title_h },
                 color_t( t.divider ).modulate( ) );

    bool const title_hovered = s.open && is_top_window && title_rect.Contains( io.MousePos ) && !close_hovered;

    if ( title_hovered && ImGui::IsMouseClicked( ImGuiMouseButton_Left ) )
    {
        s.dragging = true;
        s.drag_offset = io.MousePos - s.position;
    }

    if ( s.dragging )
    {
        if ( ImGui::IsMouseDown( ImGuiMouseButton_Left ) )
            s.position = io.MousePos - s.drag_offset;
        else
            s.dragging = false;
    }

    ImGui::SetCursorScreenPos( { win_pos.x, win_pos.y + k_title_h } );
    ImGui::PushStyleVar( ImGuiStyleVar_WindowPadding, { k_pad, k_pad } );
    ImGui::BeginChild( ( "##win_content_" + id ).c_str( ), size, 0,
                       ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_NoBackground );

    core->ui->get_window_animator( )->capture( ImGui::GetWindowDrawList( ) );
    s.animator.capture( ImGui::GetWindowDrawList( ) );

    return true;
}

c_window_animator* c_windows::current_animator( )
{
    auto it = states.find( current_id );
    if ( it == states.end( ) )
        return nullptr;
    return &it->second.animator;
}

void c_windows::end( )
{
    ImGui::EndChild( );
    ImGui::PopStyleVar( );

    auto& s = states[ current_id ];
    auto* window = ImGui::GetCurrentWindow( );
    ImVec2 const win_pos = window->Pos;
    ImVec2 const total_size = { current_size.x, current_size.y + k_title_h };

    ImGui::End( );
    ImGui::PopStyleColor( 3 );
    ImGui::PopStyleVar( 2 );

    s.animator.end( win_pos, total_size );
}
