//
// Created by rei on 4/9/2026.
//

#include "themes.hpp"

#include <nlohmann/json.hpp>

#include <array>
#include <fstream>

using alice::c_themes;
using alice::gray;
using alice::rgb;
using alice::theme_field_t;
using alice::theme_palette_t;
using nlohmann::json;

int c_themes::active_idx = 0;
std::vector< theme_palette_t > c_themes::presets;
std::filesystem::path c_themes::themes_dir = "C:\\everness\\themes";

#define FIELD_ENTRY( field )                       \
    theme_field_t                                  \
    {                                              \
        #field, offsetof( theme_palette_t, field ) \
    }

static std::array const k_fields = {
    FIELD_ENTRY( window_bg ),
    FIELD_ENTRY( content_bg ),
    FIELD_ENTRY( title_bg ),
    FIELD_ENTRY( tab_bg ),
    FIELD_ENTRY( header_bg ),
    FIELD_ENTRY( card_bg ),
    FIELD_ENTRY( popup_bg ),
    FIELD_ENTRY( notification_bg ),
    FIELD_ENTRY( notification_border ),

    FIELD_ENTRY( control ),
    FIELD_ENTRY( control_hover ),
    FIELD_ENTRY( control_active ),
    FIELD_ENTRY( control_selected ),
    FIELD_ENTRY( control_subtle ),

    FIELD_ENTRY( border ),
    FIELD_ENTRY( border_subtle ),
    FIELD_ENTRY( border_strong ),
    FIELD_ENTRY( border_hover ),
    FIELD_ENTRY( border_active ),
    FIELD_ENTRY( divider ),

    FIELD_ENTRY( text ),
    FIELD_ENTRY( text_bright ),
    FIELD_ENTRY( text_secondary ),
    FIELD_ENTRY( text_light ),
    FIELD_ENTRY( text_mid ),
    FIELD_ENTRY( text_muted ),
    FIELD_ENTRY( text_subtle ),
    FIELD_ENTRY( text_faint ),
    FIELD_ENTRY( text_hint ),
    FIELD_ENTRY( text_dim ),
    FIELD_ENTRY( text_placeholder ),

    FIELD_ENTRY( pill ),
    FIELD_ENTRY( pill_hover ),

    FIELD_ENTRY( shadow ),

    FIELD_ENTRY( accent ),
    FIELD_ENTRY( accent_bg ),
    FIELD_ENTRY( accent_subtle ),
    FIELD_ENTRY( accent_muted ),
    FIELD_ENTRY( accent_surface ),
    FIELD_ENTRY( accent_border ),
    FIELD_ENTRY( accent_strong ),

    FIELD_ENTRY( danger ),
    FIELD_ENTRY( danger_subtle ),

    FIELD_ENTRY( overlay_hover ),
    FIELD_ENTRY( overlay_pressed ),
    FIELD_ENTRY( overlay_subtle ),
    FIELD_ENTRY( overlay_faint ),
};

#undef FIELD_ENTRY

theme_field_t const* alice::theme_fields( )
{
    return k_fields.data( );
}

int alice::theme_field_count( )
{
    return static_cast< int >( k_fields.size( ) );
}

static ImVec4& field_ref( theme_palette_t& t, std::size_t offset )
{
    return *reinterpret_cast< ImVec4* >( reinterpret_cast< char* >( &t ) + offset );
}

static ImVec4 const& field_cref( theme_palette_t const& t, std::size_t offset )
{
    return *reinterpret_cast< ImVec4 const* >( reinterpret_cast< char const* >( &t ) + offset );
}

static json color_to_json( ImVec4 const& c )
{
    return json::array( {
        static_cast< int >( c.x * 255.f + 0.5f ),
        static_cast< int >( c.y * 255.f + 0.5f ),
        static_cast< int >( c.z * 255.f + 0.5f ),
        static_cast< int >( c.w * 255.f + 0.5f ),
    } );
}

static ImVec4 color_from_json( json const& j )
{
    if ( !j.is_array( ) || j.size( ) < 4 )
        return { 0, 0, 0, 1 };
    return rgb( j[ 0 ].get< int >( ), j[ 1 ].get< int >( ), j[ 2 ].get< int >( ), j[ 3 ].get< int >( ) );
}

static json theme_to_json( theme_palette_t const& t )
{
    json j;
    j[ "name" ] = t.name;
    for ( auto const& f : k_fields )
        j[ f.label ] = color_to_json( field_cref( t, f.offset ) );
    return j;
}

static theme_palette_t theme_from_json( json const& j )
{
    theme_palette_t t { };
    t.name = j.value( "name", "Unnamed" );
    t.builtin = false;
    for ( auto const& f : k_fields )
    {
        if ( j.contains( f.label ) )
            field_ref( t, f.offset ) = color_from_json( j[ f.label ] );
    }
    return t;
}

static theme_palette_t dark_solid_preset( )
{
    auto const accent = rgb( 130, 255, 157 );

    return {
        .name = "Dark Solid",
        .builtin = true,

        .window_bg = gray( 10 ),
        .content_bg = gray( 11 ),
        .title_bg = gray( 12 ),
        .tab_bg = gray( 14 ),
        .header_bg = gray( 16 ),
        .card_bg = gray( 17 ),
        .popup_bg = gray( 18 ),
        .notification_bg = gray( 18 ),
        .notification_border = gray( 28 ),

        .control = gray( 22 ),
        .control_hover = gray( 30 ),
        .control_active = gray( 38 ),
        .control_selected = gray( 32 ),
        .control_subtle = gray( 20 ),

        .border = gray( 24 ),
        .border_subtle = gray( 20 ),
        .border_strong = gray( 28 ),
        .border_hover = gray( 30 ),
        .border_active = gray( 48 ),
        .divider = gray( 26 ),

        .text = gray( 255 ),
        .text_bright = gray( 220 ),
        .text_secondary = gray( 200 ),
        .text_light = gray( 180 ),
        .text_mid = gray( 140 ),
        .text_muted = gray( 120 ),
        .text_subtle = gray( 100 ),
        .text_faint = gray( 90 ),
        .text_hint = gray( 64 ),
        .text_dim = gray( 55 ),
        .text_placeholder = gray( 45 ),

        .pill = gray( 70 ),
        .pill_hover = gray( 90 ),

        .shadow = gray( 8, 55 ),

        .accent = accent,
        .accent_bg = rgb( static_cast< int >( accent.x * 255 * 0.3f ), static_cast< int >( accent.y * 255 * 0.3f ), static_cast< int >( accent.z * 255 * 0.3f ) ),
        .accent_subtle = rgb( static_cast< int >( accent.x * 255 * 0.4f ), static_cast< int >( accent.y * 255 * 0.4f ), static_cast< int >( accent.z * 255 * 0.4f ) ),
        .accent_muted = rgb( static_cast< int >( accent.x * 255 * 0.6f ), static_cast< int >( accent.y * 255 * 0.6f ), static_cast< int >( accent.z * 255 * 0.6f ) ),
        .accent_surface = rgb( static_cast< int >( accent.x * 255 * 0.7f ), static_cast< int >( accent.y * 255 * 0.7f ), static_cast< int >( accent.z * 255 * 0.7f ) ),
        .accent_border = rgb( static_cast< int >( accent.x * 255 * 0.8f ), static_cast< int >( accent.y * 255 * 0.8f ), static_cast< int >( accent.z * 255 * 0.8f ) ),
        .accent_strong = rgb( static_cast< int >( accent.x * 255 * 0.9f ), static_cast< int >( accent.y * 255 * 0.9f ), static_cast< int >( accent.z * 255 * 0.9f ) ),

        .danger = rgb( 255, 60, 60 ),
        .danger_subtle = rgb( 220, 60, 60 ),

        .overlay_hover = gray( 255, 5 ),
        .overlay_pressed = gray( 255, 10 ),
        .overlay_subtle = gray( 255, 8 ),
        .overlay_faint = gray( 255, 14 ),
    };
}

static theme_palette_t light_solid_preset( )
{
    return {
        .name = "Light Solid",
        .builtin = true,

        .window_bg = rgb( 238, 238, 243 ),
        .content_bg = rgb( 248, 248, 253 ),
        .title_bg = rgb( 225, 225, 230 ),
        .tab_bg = rgb( 225, 225, 230 ),
        .header_bg = rgb( 240, 240, 245 ),
        .card_bg = rgb( 238, 238, 243 ),
        .popup_bg = rgb( 245, 245, 250 ),
        .notification_bg = rgb( 245, 245, 250 ),
        .notification_border = rgb( 200, 200, 205 ),

        .control = rgb( 225, 225, 230 ),
        .control_hover = rgb( 210, 210, 215 ),
        .control_active = rgb( 195, 195, 200 ),
        .control_selected = gray( 255 ),
        .control_subtle = rgb( 232, 232, 237 ),

        .border = rgb( 198, 198, 203 ),
        .border_subtle = rgb( 212, 212, 217 ),
        .border_strong = rgb( 185, 185, 190 ),
        .border_hover = rgb( 170, 170, 175 ),
        .border_active = rgb( 155, 155, 160 ),
        .divider = rgb( 205, 205, 210 ),

        .text = gray( 0 ),
        .text_bright = rgb( 15, 15, 20 ),
        .text_secondary = rgb( 55, 55, 62 ),
        .text_light = rgb( 85, 85, 90 ),
        .text_mid = rgb( 110, 110, 115 ),
        .text_muted = rgb( 142, 142, 147 ),
        .text_subtle = rgb( 155, 155, 160 ),
        .text_faint = rgb( 165, 165, 170 ),
        .text_hint = rgb( 150, 150, 155 ),
        .text_dim = rgb( 155, 155, 160 ),
        .text_placeholder = rgb( 192, 192, 197 ),

        .pill = gray( 255 ),
        .pill_hover = gray( 255 ),

        .shadow = gray( 0, 20 ),

        .accent = rgb( 0, 122, 255 ),
        .accent_bg = rgb( 232, 243, 255 ),
        .accent_subtle = rgb( 184, 220, 255 ),
        .accent_muted = rgb( 77, 163, 255 ),
        .accent_surface = rgb( 0, 122, 255 ),
        .accent_border = rgb( 0, 108, 230 ),
        .accent_strong = rgb( 0, 100, 215 ),

        .danger = rgb( 255, 59, 48 ),
        .danger_subtle = rgb( 255, 69, 58 ),

        .overlay_hover = gray( 0, 5 ),
        .overlay_pressed = gray( 0, 10 ),
        .overlay_subtle = gray( 0, 3 ),
        .overlay_faint = gray( 0, 7 ),
    };
}

static theme_palette_t dark_preset( )
{
    auto t = dark_solid_preset( );
    t.name = "Dark";

    t.window_bg.w = 170 / 255.f;
    t.content_bg.w = 180 / 255.f;
    t.title_bg.w = 190 / 255.f;
    t.tab_bg.w = 205 / 255.f;
    t.header_bg.w = 190 / 255.f;
    t.card_bg.w = 185 / 255.f;
    t.popup_bg.w = 75 / 255.f;
    t.notification_bg.w = 50 / 255.f;
    t.notification_border = rgb( 255, 255, 255, 22 );

    auto white = []( int a ) { return rgb( 255, 255, 255, a ); };

    t.border = white( 12 );
    t.border_subtle = white( 10 );
    t.border_strong = white( 14 );
    t.border_hover = white( 14 );
    t.border_active = white( 16 );
    t.divider = white( 12 );

    t.control = white( 6 );
    t.control_hover = white( 10 );
    t.control_active = white( 15 );
    t.control_selected = white( 18 );
    t.control_subtle = white( 3 );

    t.overlay_hover = white( 8 );
    t.overlay_pressed = white( 16 );
    t.overlay_subtle = white( 6 );
    t.overlay_faint = white( 12 );

    return t;
}

static theme_palette_t light_preset( )
{
    auto t = light_solid_preset( );
    t.name = "Light";

    t.window_bg.w = 215 / 255.f;
    t.content_bg.w = 235 / 255.f;
    t.title_bg.w = 245 / 255.f;
    t.tab_bg.w = 245 / 255.f;
    t.header_bg.w = 245 / 255.f;
    t.card_bg.w = 245 / 255.f;
    t.popup_bg.w = 200 / 255.f;
    t.notification_bg.w = 130 / 255.f;
    t.notification_border = rgb( 0, 0, 0, 25 );

    return t;
}

void c_themes::initialize( )
{
    presets.clear( );
    presets.push_back( dark_preset( ) );
    presets.push_back( dark_solid_preset( ) );
    load_user_themes( );
    active_idx = 0;
}

theme_palette_t const& c_themes::palette( )
{
    if ( presets.empty( ) )
        initialize( );
    return presets[ active_idx ];
}

theme_palette_t& c_themes::palette_mut( )
{
    if ( presets.empty( ) )
        initialize( );
    return presets[ active_idx ];
}

void c_themes::set( int idx )
{
    if ( idx >= 0 && idx < static_cast< int >( presets.size( ) ) )
        active_idx = idx;
}

int c_themes::count( )
{
    if ( presets.empty( ) )
        initialize( );
    return static_cast< int >( presets.size( ) );
}

std::string const& c_themes::name( int idx )
{
    return presets[ idx ].name;
}

int c_themes::index( )
{
    return active_idx;
}

void c_themes::add( theme_palette_t theme )
{
    presets.push_back( std::move( theme ) );
}

void c_themes::remove( int idx )
{
    if ( idx < 0 || idx >= static_cast< int >( presets.size( ) ) || presets[ idx ].builtin )
        return;

    auto const theme_name = presets[ idx ].name;
    presets.erase( presets.begin( ) + idx );

    if ( active_idx >= static_cast< int >( presets.size( ) ) )
        active_idx = static_cast< int >( presets.size( ) ) - 1;

    delete_theme_file( theme_name );
}

bool c_themes::is_builtin( int idx )
{
    if ( idx < 0 || idx >= static_cast< int >( presets.size( ) ) )
        return false;
    return presets[ idx ].builtin;
}

theme_palette_t& c_themes::at( int idx )
{
    if ( presets.empty( ) )
        initialize( );
    if ( idx < 0 || idx >= static_cast< int >( presets.size( ) ) )
        return presets[ active_idx ];
    return presets[ idx ];
}

void c_themes::save_theme( int idx )
{
    if ( idx < 0 || idx >= static_cast< int >( presets.size( ) ) || presets[ idx ].builtin )
        return;

    std::filesystem::create_directories( themes_dir );

    auto const path = themes_dir / ( presets[ idx ].name + ".json" );
    std::ofstream out( path );
    if ( out.is_open( ) )
        out << theme_to_json( presets[ idx ] ).dump( 2 );
}

void c_themes::save_all_user( )
{
    for ( int i = 0; i < static_cast< int >( presets.size( ) ); ++i )
        save_theme( i );
}

void c_themes::load_user_themes( )
{
    if ( !std::filesystem::exists( themes_dir ) )
        return;

    for ( auto const& entry : std::filesystem::directory_iterator( themes_dir ) )
    {
        if ( entry.path( ).extension( ) != ".json" )
            continue;

        std::ifstream in( entry.path( ) );
        if ( !in.is_open( ) )
            continue;

        try
        {
            auto j = json::parse( in );
            presets.push_back( theme_from_json( j ) );
        } catch ( ... )
        {
        }
    }
}

void c_themes::delete_theme_file( std::string const& theme_name )
{
    auto const path = themes_dir / ( theme_name + ".json" );
    std::filesystem::remove( path );
}
