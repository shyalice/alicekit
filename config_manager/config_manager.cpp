//
// Created by rei on 4/13/2026.
//

#include "config_manager.hpp"

#include "alice_kit.hpp"
#include "notifications/notifications.hpp"
#include "ui/ui.hpp"
#include "fonts/fonts.hpp"
#include "fonts/lucide_defines.hpp"
#include "popups/popup_elem/popup_elem.hpp"
#include "themes/themes.hpp"
#include "util/util.hpp"

#include <algorithm>
#include <random>

using alice::c_config_manager;
using alice::c_notifications;
using alice::c_popup_elem;
using alice::c_themes;
using alice::color_t;
using alice::notification_type;
using alice::with_alpha;

using namespace ImGui;

constexpr float k_w        = 280.f;
constexpr float k_pad      = 8.f;
constexpr float k_row      = 28.f;
constexpr float k_item_h   = 32.f;
constexpr float k_item_gap = 2.f;
constexpr float k_item_full= k_item_h + k_item_gap;
constexpr float k_sep      = 6.f;
constexpr float k_tb_btn   = 28.f;
constexpr float k_ib       = 20.f;
constexpr int   k_max_vis  = 6;

static std::string gen_id( )
{
    static std::mt19937 rng { std::random_device { }( ) };
    static std::uniform_int_distribution< int > d( 0, 15 );
    constexpr char h[] = "0123456789abcdef";
    std::string r = "cfg_";
    for ( int i = 0; i < 8; ++i ) r += h[ d( rng ) ];
    return r;
}

c_config_manager::c_config_manager( )
    : popup( k_w, "##cfgmgr" )
    , ctx_popup( 130, "##cfgctx" )
{
    popup.set_animate_layout( true );
    popup.set_window_padding( { k_pad, k_pad } );
    ctx_popup.set_window_padding( { 4, 4 } );
    configs.push_back( { "default", "Default" } );
    selected_config_id = "default";
}

void c_config_manager::set_on_load( action_callback_t cb )   { on_load = std::move( cb ); }
void c_config_manager::set_on_save( action_callback_t cb )   { on_save = std::move( cb ); }
void c_config_manager::set_on_delete( action_callback_t cb ) { on_delete = std::move( cb ); }
void c_config_manager::set_on_rename( rename_callback_t cb ) { on_rename = std::move( cb ); }
void c_config_manager::set_on_create( create_callback_t cb ) { on_create = std::move( cb ); }
void c_config_manager::set_on_refresh( void_callback_t cb )  { on_refresh = std::move( cb ); }

void c_config_manager::set_configs( std::vector< alice::config_entry_t > e ) { configs = std::move( e ); }
std::vector< alice::config_entry_t > const& c_config_manager::get_configs( ) const { return configs; }
std::string const& c_config_manager::selected_id( ) const { return selected_config_id; }

void c_config_manager::open( ImVec2 p ) { open_pos = p; open_frame = GetFrameCount( ) + 1; }
void c_config_manager::close( )         { popup.close( ); ctx_popup.close( ); open_frame = -1; renaming_id.clear( ); }
bool c_config_manager::is_open( ) const { return popup.is_open( ) || open_frame >= 0; }

void c_config_manager::save_selected( )
{
    if ( !selected_config_id.empty( ) )
    {
        if ( on_save ) on_save( selected_config_id );

        std::string name = selected_config_id;
        auto it = std::ranges::find_if( configs, [ & ]( auto const& e ) { return e.id == selected_config_id; } );
        if ( it != configs.end( ) ) name = it->name;
        c_notifications::push( notification_type::success, "Config saved", name );
    }
}

void c_config_manager::render( )
{
    float const dt  = GetIO( ).DeltaTime;
    float const spd = 20.f * dt;
    auto const& pal = c_themes::palette( );
    auto const  fnt = core->fonts->get( "primary" );

    if ( deferred == 1 )
    {
        configs.push_back( { deferred_id, "Unnamed" } );
        selected_config_id = deferred_id;
        if ( on_load ) on_load( deferred_id );
        c_notifications::push( notification_type::success, "Config created", "New config profile ready" );
    }
    else if ( deferred == 2 )
    {
        std::string deleted_name;
        auto it = std::ranges::find_if( configs, [ & ]( auto const& e ) { return e.id == deferred_id; } );
        if ( it != configs.end( ) ) deleted_name = it->name;

        if ( on_delete ) on_delete( deferred_id );
        std::erase_if( configs, [ & ]( auto const& e ) { return e.id == deferred_id; } );
        item_anims.erase( deferred_id );
        if ( selected_config_id == deferred_id )
        {
            selected_config_id = configs.empty( ) ? "" : configs.front( ).id;
            if ( on_load && !selected_config_id.empty( ) ) on_load( selected_config_id );
        }
        c_notifications::push( notification_type::failed, "Config deleted", deleted_name.empty( ) ? "Profile removed" : deleted_name + " removed" );
    }
    deferred = 0;
    deferred_id.clear( );

    std::string ql( search_buf );
    std::ranges::transform( ql, ql.begin( ), []( unsigned char c ) { return std::tolower( c ); } );
    std::vector< int > vis;
    for ( int i = 0; i < static_cast< int >( configs.size( ) ); ++i )
    {
        std::string n = configs[ i ].name;
        std::ranges::transform( n, n.begin( ), []( unsigned char c ) { return std::tolower( c ); } );
        if ( ql.empty( ) || n.find( ql ) != std::string::npos ) vis.push_back( i );
    }
    int const vc = static_cast< int >( vis.size( ) );
    bool const need_scroll = vc > k_max_vis;
    int const shown = need_scroll ? k_max_vis : vc;

    if ( open_frame == GetFrameCount( ) )
    {
        popup.set_position( open_pos );
        popup.open( );
        open_frame = -1;
    }

    float const est = k_row + k_sep + k_row + k_sep + static_cast< float >( std::max( shown, 1 ) ) * k_item_full;
    popup.set_width( k_w );
    popup.set_position( open_pos );
    popup.get_window_animator( )->set_pivot( { 0.5f, 0.f } );
    popup.hint_content_height( est );

    if ( !popup.begin( ) )
        return;

    auto* wnd = GetCurrentWindow( );
    auto* dl  = wnd->DrawList;
    float const sa = GetStyle( ).Alpha;
    float const iw = GetContentRegionAvail( ).x;

    // ── toolbar ──
    {
        ImVec2 const o = GetCursorScreenPos( );

        auto const isz = fnt.calculate_text_size( ICON_LUCIDE_CLOUD );
        float const tw_t = fnt.calculate_text_size( "Configs" ).x;
        tab_anim.mod = ImLerp( tab_anim.mod, 1.f, spd );
        float const tw = k_row + tab_anim.mod * ( tw_t + 6.f );
        ImVec2 const a = o, b = { o.x + tw, o.y + k_row };

        if ( tab_anim.bg.w < 0.01f ) { tab_anim.bg = pal.control_selected; tab_anim.ic = pal.accent; tab_anim.tx = pal.text; }
        tab_anim.bg = ImLerp( tab_anim.bg, pal.control_selected, spd );
        tab_anim.ic = ImLerp( tab_anim.ic, pal.accent, spd );
        tab_anim.tx = ImLerp( tab_anim.tx, pal.text, spd );

        dl->AddRectFilled( a, b, color_t( tab_anim.bg ).modulate( sa ), 6 );
        dl->AddText( fnt.ptr, fnt.size, { a.x + k_row / 2 - isz.x / 2, a.y + k_row / 2 - isz.y / 2 }, color_t( tab_anim.ic ).modulate( sa ), ICON_LUCIDE_CLOUD );
        dl->PushClipRect( a, b, true );
        auto const ls = fnt.calculate_text_size( "Configs" );
        dl->AddText( fnt.ptr, fnt.size, { a.x + k_row, a.y + k_row / 2 - ls.y / 2 }, color_t( tab_anim.tx ).modulate( sa ), "Configs" );
        dl->PopClipRect( );

        float bx = o.x + iw;
        auto const tbtn = [ & ]( char const* icon, ImVec4 const& accent, char const* ids ) {
            bx -= k_tb_btn;
            ImVec2 const p = { bx, o.y + k_row / 2 - k_tb_btn / 2 };
            ImRect const r = { p, { p.x + k_tb_btn, p.y + k_tb_btn } };
            ImGuiID const id = wnd->GetID( ids );
            bool h = false, hd = false;
            bool const pr = ButtonBehavior( r, id, &h, &hd );
            ItemAdd( r, id );
            auto& ba = tb_anims[ id ];
            ba.background = ImLerp( ba.background, h ? pal.control_hover : with_alpha( pal.control_hover, 0.f ), spd );
            ba.icon = ImLerp( ba.icon, h ? accent : pal.text_muted, spd );
            dl->AddRectFilled( r.Min, r.Max, color_t( ba.background ).modulate( sa ), 6 );
            auto const sz = fnt.calculate_text_size( icon );
            dl->AddText( fnt.ptr, fnt.size, { r.GetCenter( ).x - sz.x / 2, r.GetCenter( ).y - sz.y / 2 }, color_t( ba.icon ).modulate( sa ), icon );
            bx -= 2.f;
            return pr;
        };

        if ( tbtn( ICON_LUCIDE_PLUS, pal.accent, "##+" ) && !deferred )
        {
            deferred = 1;
            deferred_id = on_create ? on_create( "Unnamed" ) : gen_id( );
        }
        if ( tbtn( ICON_LUCIDE_REFRESH_CW, pal.text_secondary, "##r" ) && on_refresh )
            on_refresh( );

        SetCursorScreenPos( { o.x, o.y + k_row + k_sep } );
    }

    // ── search ──
    {
        ImVec2 const s0 = GetCursorScreenPos( );
        ImVec2 const s1 = { s0.x + iw, s0.y + k_row };

        bool const search_hov = ImRect( s0, s1 ).Contains( GetIO( ).MousePos );
        dl->AddRectFilled( s0, s1, color_t( pal.control ).modulate( sa ), 6 );
        dl->AddRect( s0, s1, color_t( search_hov ? pal.border_hover : pal.control_hover ).modulate( sa ), 6 );

        auto const sic = fnt.calculate_text_size( ICON_LUCIDE_SEARCH );
        dl->AddText( fnt.ptr, fnt.size, { s0.x + 8, s0.y + k_row / 2 - sic.y / 2 }, color_t( pal.text_dim ).modulate( sa ), ICON_LUCIDE_SEARCH );

        float const tx = s0.x + 8 + sic.x + 6;
        SetCursorScreenPos( { tx, s0.y } );
        PushStyleColor( ImGuiCol_FrameBg, ImVec4( 0, 0, 0, 0 ) );
        PushStyleColor( ImGuiCol_Text, with_alpha( pal.text, 1.f ) );
        PushStyleColor( ImGuiCol_TextSelectedBg, with_alpha( pal.accent, 0.3f ) );
        PushStyleVar( ImGuiStyleVar_FramePadding, { 0, ( k_row - GetFontSize( ) ) / 2 } );
        PushItemWidth( s1.x - tx - 4 );
        InputText( "##cfs", search_buf, sizeof( search_buf ) );
        PopItemWidth( );
        PopStyleVar( );
        PopStyleColor( 3 );

        if ( strlen( search_buf ) == 0 )
        {
            auto const ph = fnt.calculate_text_size( "Search" );
            dl->AddText( fnt.ptr, fnt.size, { tx, s0.y + k_row / 2 - ph.y / 2 }, color_t( pal.text_placeholder ).modulate( sa ), "Search" );
        }

        SetCursorScreenPos( { s0.x, s1.y + k_sep } );
    }

    // ── items ──
    constexpr float k_sb_target = 8.f;
    sb_reserve_anim = ImLerp( sb_reserve_anim, need_scroll ? k_sb_target : 0.f, spd );

    if ( need_scroll )
    {
        float const lh = static_cast< float >( k_max_vis ) * k_item_full;
        PushStyleVar( ImGuiStyleVar_WindowPadding, { 0, 0 } );
        PushStyleVar( ImGuiStyleVar_ItemSpacing, { 0, 0 } );
        BeginChild( "##cfl", { iw, lh }, 0, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoBackground );
        popup.get_window_animator( )->capture( GetWindowDrawList( ) );

        float const item_w = GetContentRegionAvail( ).x - sb_reserve_anim;
        for ( int vi = 0; vi < vc; ++vi )
        {
            render_item( vi, configs[ vis[ vi ] ], item_w );
            if ( vi < vc - 1 ) ItemSize( { 0, k_item_gap } );
        }

        list_sb.render( GetCurrentWindow( ), { 0, 2 }, true, 3.f );
        EndChild( );
        PopStyleVar( 2 );
    }
    else
    {
        float const item_w = iw - sb_reserve_anim;
        for ( int vi = 0; vi < vc; ++vi )
        {
            render_item( vi, configs[ vis[ vi ] ], item_w );
            if ( vi < vc - 1 ) ItemSize( { 0, k_item_gap } );
        }
    }

    popup.end( );

    // ── context ──
    if ( ctx_popup.begin( ) )
    {
        if ( c_popup_elem::button( ICON_LUCIDE_REFRESH_CW, "Reload" ) )
        {
            if ( on_load && !ctx_id.empty( ) )
            {
                on_load( ctx_id );
                c_notifications::push( notification_type::success, "Config reloaded", "Profile refreshed" );
            }
            ctx_popup.close( );
        }
        if ( ctx_id != "default" && c_popup_elem::button( ICON_LUCIDE_TRASH_2, "Delete" ) && !deferred )
        {
            deferred = 2;
            deferred_id = ctx_id;
            ctx_popup.close( );
        }
        ctx_popup.end( );
    }
}

void c_config_manager::render_item( int vi, config_entry_t const& entry, float width )
{
    float const spd = 20.f * GetIO( ).DeltaTime;
    auto const& pal = c_themes::palette( );
    auto const  fnt = core->fonts->get( "primary" );
    float const sa  = GetStyle( ).Alpha;
    auto* dl = GetWindowDrawList( );

    bool const sel = entry.id == selected_config_id;
    bool const ren = entry.id == renaming_id;
    auto& a = item_anims[ entry.id ];

    if ( a.name_color.w < 0.01f )
    {
        a.name_color = pal.text_secondary;
        a.save_icon = pal.text_faint;
        a.dots_icon = pal.text_faint;
    }

    PushID( vi );

    ImVec2 const pos = GetCursorScreenPos( );
    ImVec2 const sz  = { width, k_item_h };
    ImRect const bb  = { pos, pos + sz };
    ImVec2 const m   = GetIO( ).MousePos;

    constexpr float rp = 6.f, bw = k_ib * 2 + 2.f + rp;
    ImVec2 const sc = { bb.Max.x - rp - k_ib / 2, bb.GetCenter( ).y };
    ImVec2 const dc = { sc.x - k_ib - 2.f, bb.GetCenter( ).y };
    ImRect const sr = { { sc.x - k_ib / 2, bb.Min.y }, { sc.x + k_ib / 2, bb.Max.y } };
    ImRect const dr = { { dc.x - k_ib / 2, bb.Min.y }, { dc.x + k_ib / 2, bb.Max.y } };
    bool const ms = sr.Contains( m ), md = dr.Contains( m ), mb = ms || md;

    InvisibleButton( "##i", sz );
    bool const hov = IsItemHovered( ) && !mb;
    bool const clk = IsItemClicked( ) && !mb;

    a.background = ImLerp( a.background, sel ? with_alpha( pal.accent_surface, 0.12f ) : ( hov ? pal.overlay_hover : with_alpha( pal.overlay_hover, 0.f ) ), spd );
    a.outline    = ImLerp( a.outline,    sel ? with_alpha( pal.accent, 0.25f )         : with_alpha( pal.border, 0.f ), spd );
    a.name_color = ImLerp( a.name_color, sel ? pal.text_bright                         : ( hov ? pal.text : pal.text_secondary ), spd );
    a.save_icon  = ImLerp( a.save_icon,  ms ? pal.accent    : pal.text_faint, spd );
    a.dots_icon  = ImLerp( a.dots_icon,  md ? pal.text      : pal.text_faint, spd );

    dl->AddRectFilled( bb.Min, bb.Max, color_t( a.background ).modulate( sa ), 6 );
    dl->AddRect( bb.Min, bb.Max, color_t( a.outline ).modulate( sa ), 6 );

    auto const si = fnt.calculate_text_size( ICON_LUCIDE_SAVE );
    dl->AddText( fnt.ptr, fnt.size, { sc.x - si.x / 2, sc.y - si.y / 2 }, color_t( a.save_icon ).modulate( sa ), ICON_LUCIDE_SAVE );
    auto const di = fnt.calculate_text_size( ICON_LUCIDE_ELLIPSIS );
    dl->AddText( fnt.ptr, fnt.size, { dc.x - di.x / 2, dc.y - di.y / 2 }, color_t( a.dots_icon ).modulate( sa ), ICON_LUCIDE_ELLIPSIS );

    float const nmw = bb.GetWidth( ) - 10.f - bw;

    a.rename_t = ImLerp( a.rename_t, ren ? 1.f : 0.f, spd );

    {
        float const text_alpha = sa * ( 1.f - a.rename_t );
        if ( text_alpha > 0.01f )
        {
            auto const dn = truncate_text( fnt.ptr, fnt.size, entry.name, nmw );
            auto const ns = fnt.calculate_text_size( dn );
            dl->AddText( fnt.ptr, fnt.size, { bb.Min.x + 10, bb.GetCenter( ).y - ns.y / 2 }, color_t( a.name_color ).modulate( text_alpha ), dn.c_str( ) );
        }
    }

    if ( ren || a.rename_t > 0.01f )
    {
        auto* cw = GetCurrentWindow( );
        ImVec2 const sc_saved = cw->DC.CursorPos;
        float const mx_saved = cw->DC.CursorMaxPos.y;

        float const input_alpha = sa * a.rename_t;

        SetCursorScreenPos( { bb.Min.x + 8, bb.GetCenter( ).y - ( fnt.size + 6.f ) / 2 } );
        PushStyleColor( ImGuiCol_FrameBg, static_cast< ImVec4 >( color_t( pal.control ).modulate( input_alpha ) ) );
        PushStyleColor( ImGuiCol_Text, with_alpha( pal.text, input_alpha ) );
        PushStyleColor( ImGuiCol_Border, with_alpha( pal.accent, 0.4f * a.rename_t ) );
        PushStyleColor( ImGuiCol_TextSelectedBg, with_alpha( pal.accent, 0.3f ) );
        PushStyleVar( ImGuiStyleVar_FramePadding, { 4, 3 } );
        PushStyleVar( ImGuiStyleVar_FrameRounding, 3.f );
        PushStyleVar( ImGuiStyleVar_FrameBorderSize, 1.f );
        PushStyleVar( ImGuiStyleVar_Alpha, input_alpha );
        PushItemWidth( nmw );

        if ( rename_focus_pending ) SetKeyboardFocusHere( );

        bool const ok = InputText( "##rn", rename_buf, sizeof( rename_buf ), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll );
        bool const act = IsItemActive( ) || IsItemFocused( );

        PopItemWidth( );
        PopStyleVar( 4 );
        PopStyleColor( 4 );

        cw->DC.CursorPos = sc_saved;
        cw->DC.CursorMaxPos.y = mx_saved;

        bool const done = ok || ( !act && !rename_focus_pending );
        rename_focus_pending = false;

        if ( done )
        {
            if ( strlen( rename_buf ) > 0 )
            {
                auto it = std::ranges::find_if( configs, [ & ]( auto const& e ) { return e.id == renaming_id; } );
                if ( it != configs.end( ) ) { it->name = rename_buf; if ( on_rename ) on_rename( it->id, it->name ); }
            }
            renaming_id.clear( );
        }
    }

    if ( ms && IsMouseClicked( 0 ) )
    {
        if ( on_save ) on_save( entry.id );
        c_notifications::push( notification_type::success, "Config saved", entry.name );
    }
    if ( md && IsMouseClicked( 0 ) )
    {
        ctx_id = entry.id;
        ctx_popup.set_position( { dr.Max.x, dr.Max.y + 2 } );
        if ( ctx_popup.open_allowed( ) ) ctx_popup.open( );
    }
    if ( clk )
    {
        selected_config_id = entry.id;
        if ( on_load ) on_load( entry.id );
        c_notifications::push( notification_type::success, "Config loaded", entry.name );
        if ( ctx_popup.is_open( ) ) ctx_popup.close( );
        if ( !renaming_id.empty( ) && renaming_id != entry.id ) renaming_id.clear( );
    }
    if ( !ren && IsMouseDoubleClicked( 0 ) && bb.Contains( m ) && !mb )
    {
        renaming_id = entry.id;
        std::ranges::copy( entry.name, rename_buf );
        rename_buf[ entry.name.size( ) ] = '\0';
        rename_focus_pending = true;
    }

    PopID( );
}
