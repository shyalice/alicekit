//
// font_loader.cpp
//

#include "font_loader.hpp"
#include "font_data_format.hpp"

#include "alice_kit.hpp"
#include "backends/backend.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include "logger/logger.hpp"

#include <cstdio>
#include <cstring>
#include <algorithm>

#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

using alice::c_font_loader;

bool c_font_loader::memory_reader_t::read( void* dst, size_t bytes )
{
    if ( !dst || offset + bytes > size )
        return false;

    memcpy( dst, data + offset, bytes );
    offset += bytes;
    return true;
}

bool c_font_loader::memory_reader_t::skip( size_t bytes )
{
    if ( offset + bytes > size )
        return false;

    offset += bytes;
    return true;
}

size_t c_font_loader::memory_reader_t::remaining( ) const
{
    return size - offset;
}

bool c_font_loader::load( char const* atlas_path, char const* data_path )
{
    release( );

    if ( !load_atlas( atlas_path ) )
        return false;

    if ( !load_data( data_path ) )
        return false;

    if ( !create_gpu_atlas( ) )
        return false;

    return true;
}

bool c_font_loader::load_from_memory(
    void const* atlas_data, size_t atlas_size,
    void const* font_data, size_t font_data_size )
{
    release( );

    if ( !load_atlas_from_memory( atlas_data, atlas_size ) )
        return false;

    if ( !load_data_from_memory( font_data, font_data_size ) )
        return false;

    if ( !create_gpu_atlas( ) )
        return false;

    return true;
}

bool c_font_loader::load_atlas( char const* path )
{
    FILE* f = fopen( path, "rb" );
    if ( !f )
    {
        log_err( "can't open {}", path );
        return false;
    }

    fseek( f, 0, SEEK_END );
    long const file_size_long = ftell( f );
    fseek( f, 0, SEEK_SET );

    if ( file_size_long <= 0 )
    {
        log_err( "atlas file is empty" );
        fclose( f );
        return false;
    }

    size_t const file_size = ( size_t ) file_size_long;
    std::vector< uint8_t > bytes( file_size );

    if ( fread( bytes.data( ), 1, file_size, f ) != file_size )
    {
        log_err( "atlas read failed" );
        fclose( f );
        return false;
    }

    fclose( f );
    return load_atlas_from_memory( bytes.data( ), bytes.size( ) );
}

bool c_font_loader::load_data( char const* path )
{
    FILE* f = fopen( path, "rb" );
    if ( !f )
    {
        log_err( "can't open {}", path );
        return false;
    }

    fseek( f, 0, SEEK_END );
    long const file_size_long = ftell( f );
    fseek( f, 0, SEEK_SET );

    if ( file_size_long <= 0 )
    {
        log_err( "file is empty" );
        fclose( f );
        return false;
    }

    size_t const file_size = ( size_t ) file_size_long;
    std::vector< uint8_t > bytes( file_size );

    if ( fread( bytes.data( ), 1, file_size, f ) != file_size )
    {
        log_err( "data read failed" );
        fclose( f );
        return false;
    }

    fclose( f );
    return load_data_from_memory( bytes.data( ), bytes.size( ) );
}

bool c_font_loader::load_atlas_from_memory( void const* data, size_t size )
{
    if ( !data || size == 0 )
    {
        log_err( "atlas is empty" );
        return false;
    }

    memory_reader_t rd { ( uint8_t const* ) data, size, 0 };
    return parse_atlas( rd );
}

bool c_font_loader::load_data_from_memory( void const* data, size_t size )
{
    if ( !data || size == 0 )
    {
        log_err( "data is empty" );
        return false;
    }

    memory_reader_t rd { ( uint8_t const* ) data, size, 0 };
    return parse_data( rd );
}

bool c_font_loader::parse_atlas( memory_reader_t& rd )
{
    font_format::AtlasHeader hdr { };
    if ( !rd.read( &hdr, sizeof( hdr ) ) || hdr.magic != font_format::MAGIC_ATLAS )
    {
        log_err( "bad atlas magic" );
        return false;
    }

    m_atlas_w = hdr.width;
    m_atlas_h = hdr.height;

    uint64_t const pixel_count_64 = ( uint64_t ) m_atlas_w * ( uint64_t ) m_atlas_h;
    if ( pixel_count_64 == 0 || pixel_count_64 > 0xFFFFFFFFull )
    {
        log_err( "invalid atlas size" );
        return false;
    }

    uint32_t const pixel_count = ( uint32_t ) pixel_count_64;
    size_t const pixel_bytes = ( size_t ) pixel_count * sizeof( uint32_t );

    m_atlas_pixels.resize( pixel_count );
    if ( !rd.read( m_atlas_pixels.data( ), pixel_bytes ) )
    {
        log_err( "atlas read failure" );
        return false;
    }

    apply_atlas_postprocess( );

    log_success( "atlas {}x{} loaded", m_atlas_w, m_atlas_h );
    return true;
}

bool c_font_loader::parse_data( memory_reader_t& rd )
{
    font_format::DataHeader hdr { };
    if ( !rd.read( &hdr, sizeof( hdr ) ) || hdr.magic != font_format::MAGIC_DATA )
    {
        log_err( "bad data magic" );
        return false;
    }

    m_uv_white_x = hdr.uv_white_x;
    m_uv_white_y = hdr.uv_white_y;

    ImGuiIO& io = ImGui::GetIO( );
    m_fonts.resize( hdr.font_count );

    for ( uint32_t fi = 0; fi < hdr.font_count; ++fi )
    {
        auto& lf = m_fonts[ fi ];
        if ( !rd.read( &lf.meta, sizeof( font_format::FontEntry ) ) )
        {
            log_err( "font entry magic" );
            return false;
        }

        ImFont* font = IM_NEW( ImFont );
        font->FontSize = lf.meta.font_size;
        font->Ascent = lf.meta.ascent;
        font->Descent = lf.meta.descent;
        font->ContainerAtlas = io.Fonts;
        font->Scale = 1.0f;
        font->FallbackChar = '?';

        lf.source_cfg = ImFontConfig { };
        lf.source_cfg.SizePixels = lf.meta.font_size;
        lf.source_cfg.MergeMode = false;
        lf.source_cfg.PixelSnapH = false;
        lf.source_cfg.OversampleH = 1;
        lf.source_cfg.OversampleV = 1;
        lf.source_cfg.EllipsisChar = ( ImWchar ) 0x2026;
        lf.source_cfg.DstFont = font;

        font->Sources = &lf.source_cfg;
        font->SourcesCount = 1;

        for ( uint32_t gi = 0; gi < lf.meta.glyph_count; ++gi )
        {
            font_format::GlyphEntry ge { };
            if ( !rd.read( &ge, sizeof( ge ) ) )
            {
                log_err( "glyph read failed" );
                return false;
            }

            if ( ge.visible )
            {
                font->AddGlyph(
                    nullptr,
                    ( ImWchar ) ge.codepoint,
                    ge.x0, ge.y0, ge.x1, ge.y1,
                    ge.u0, ge.v0, ge.u1, ge.v1,
                    ge.advance_x );
            } else
            {
                font->AddGlyph(
                    nullptr,
                    ( ImWchar ) ge.codepoint,
                    0, 0, 0, 0,
                    0, 0, 0, 0,
                    ge.advance_x );
            }
        }

        font->BuildLookupTable( );
        font->FallbackGlyph = font->FindGlyphNoFallback( '?' );
        font->FallbackAdvanceX = lf.meta.fallback_advance_x;

        if ( !io.FontDefault )
            io.FontDefault = font;

        io.Fonts->Fonts.push_back( font );
        lf.font = font;

        log_success( "loaded '{}' {}px w={} rasterizer={} glyphs={}",
                     lf.meta.family, lf.meta.size, ( int ) lf.meta.weight,
                     lf.meta.rasterizer == ( uint8_t ) font_format::Rasterizer::FreeType ? "ft" : "dx9",
                     lf.meta.glyph_count );
    }

    font_format::ShadowData sd { };
    if ( rd.remaining( ) >= sizeof( sd ) && rd.read( &sd, sizeof( sd ) ) && sd.valid )
    {
        ImFontAtlas* atlas = ImGui::GetIO( ).Fonts;

        m_shadow_rect_x[ 0 ] = sd.rects[ 0 ].x;
        m_shadow_rect_y[ 0 ] = sd.rects[ 0 ].y;
        m_shadow_rect_w[ 0 ] = sd.rects[ 0 ].w;
        m_shadow_rect_h[ 0 ] = sd.rects[ 0 ].h;

        m_shadow_rect_x[ 1 ] = sd.rects[ 1 ].x;
        m_shadow_rect_y[ 1 ] = sd.rects[ 1 ].y;
        m_shadow_rect_w[ 1 ] = sd.rects[ 1 ].w;
        m_shadow_rect_h[ 1 ] = sd.rects[ 1 ].h;

        memcpy( m_shadow_rect_uvs, sd.rect_uvs, sizeof( m_shadow_rect_uvs ) );
        m_shadow_valid = true;

        int const base = atlas->CustomRects.Size;

        atlas->CustomRects.push_back( ImFontAtlasCustomRect( ) );
        atlas->CustomRects.push_back( ImFontAtlasCustomRect( ) );

        atlas->CustomRects[ base + 0 ].X = ( unsigned short ) m_shadow_rect_x[ 0 ];
        atlas->CustomRects[ base + 0 ].Y = ( unsigned short ) m_shadow_rect_y[ 0 ];
        atlas->CustomRects[ base + 0 ].Width = ( unsigned short ) m_shadow_rect_w[ 0 ];
        atlas->CustomRects[ base + 0 ].Height = ( unsigned short ) m_shadow_rect_h[ 0 ];

        atlas->CustomRects[ base + 1 ].X = ( unsigned short ) m_shadow_rect_x[ 1 ];
        atlas->CustomRects[ base + 1 ].Y = ( unsigned short ) m_shadow_rect_y[ 1 ];
        atlas->CustomRects[ base + 1 ].Width = ( unsigned short ) m_shadow_rect_w[ 1 ];
        atlas->CustomRects[ base + 1 ].Height = ( unsigned short ) m_shadow_rect_h[ 1 ];

        memcpy( atlas->ShadowRectUvs, m_shadow_rect_uvs, sizeof( m_shadow_rect_uvs ) );

        atlas->ShadowRectIds[ 0 ] = base + 0;
        atlas->ShadowRectIds[ 1 ] = base + 1;

        log_success( "shadow rects loaded" );
    }

    return true;
}

void c_font_loader::apply_atlas_postprocess( )
{
    uint32_t const pixel_count = m_atlas_w * m_atlas_h;

    for ( uint32_t i = 0; i < pixel_count; ++i )
    {
        uint32_t px = m_atlas_pixels[ i ];
        uint8_t a = ( px >> 24 ) & 0xFF;
        if ( a == 0 || a == 255 )
            continue;

        uint8_t r = ( px >> 16 ) & 0xFF;
        uint8_t g = ( px >> 8 ) & 0xFF;
        uint8_t b = ( px >> 0 ) & 0xFF;

        r = ( uint8_t ) std::min( 255, r * 255 / a );
        g = ( uint8_t ) std::min( 255, g * 255 / a );
        b = ( uint8_t ) std::min( 255, b * 255 / a );

        m_atlas_pixels[ i ] =
            ( ( uint32_t ) a << 24 ) |
            ( ( uint32_t ) r << 16 ) |
            ( ( uint32_t ) g << 8 ) |
            ( uint32_t ) b;
    }
}

bool c_font_loader::create_gpu_atlas( )
{
    if ( !alice::core || !alice::core->backend )
    {
        log_err( "backend not initialized" );
        return false;
    }

    if ( m_srv )
    {
        alice::core->backend->destroy_texture( m_srv );
        m_srv = 0;
    }

    m_srv = alice::core->backend->create_texture_rgba(
        m_atlas_pixels.data( ), ( int ) m_atlas_w, ( int ) m_atlas_h );

    if ( !m_srv )
    {
        log_err( "backend create_texture_rgba failed" );
        return false;
    }

    ImFontAtlas* atlas = ImGui::GetIO( ).Fonts;
    atlas->TexWidth = ( int ) m_atlas_w;
    atlas->TexHeight = ( int ) m_atlas_h;
    atlas->TexID = m_srv;
    atlas->TexReady = true;
    atlas->TexUvWhitePixel = ImVec2( m_uv_white_x, m_uv_white_y );

    m_atlas_pixels.clear( );
    m_atlas_pixels.shrink_to_fit( );

    log_info( "gpu atlas ready" );
    return true;
}

void c_font_loader::bind_atlas( )
{
    ImFontAtlas* atlas = ImGui::GetIO( ).Fonts;
    atlas->TexWidth = ( int ) m_atlas_w;
    atlas->TexHeight = ( int ) m_atlas_h;
    atlas->TexUvScale = ImVec2( 1.0f / m_atlas_w, 1.0f / m_atlas_h );
    atlas->SetTexID( m_srv );
    atlas->TexReady = true;
    atlas->TexUvWhitePixel = ImVec2( m_uv_white_x, m_uv_white_y );
    atlas->Flags |= ImFontAtlasFlags_NoBakedLines;

    if ( m_shadow_valid )
    {
        int id0 = atlas->ShadowRectIds[ 0 ];
        int id1 = atlas->ShadowRectIds[ 1 ];

        if ( id0 < 0 || id0 >= atlas->CustomRects.Size || id1 < 0 || id1 >= atlas->CustomRects.Size )
        {
            int const base = atlas->CustomRects.Size;
            atlas->CustomRects.push_back( ImFontAtlasCustomRect( ) );
            atlas->CustomRects.push_back( ImFontAtlasCustomRect( ) );
            id0 = base + 0;
            id1 = base + 1;
            atlas->ShadowRectIds[ 0 ] = id0;
            atlas->ShadowRectIds[ 1 ] = id1;
        }

        atlas->CustomRects[ id0 ].X = ( unsigned short ) m_shadow_rect_x[ 0 ];
        atlas->CustomRects[ id0 ].Y = ( unsigned short ) m_shadow_rect_y[ 0 ];
        atlas->CustomRects[ id0 ].Width = ( unsigned short ) m_shadow_rect_w[ 0 ];
        atlas->CustomRects[ id0 ].Height = ( unsigned short ) m_shadow_rect_h[ 0 ];

        atlas->CustomRects[ id1 ].X = ( unsigned short ) m_shadow_rect_x[ 1 ];
        atlas->CustomRects[ id1 ].Y = ( unsigned short ) m_shadow_rect_y[ 1 ];
        atlas->CustomRects[ id1 ].Width = ( unsigned short ) m_shadow_rect_w[ 1 ];
        atlas->CustomRects[ id1 ].Height = ( unsigned short ) m_shadow_rect_h[ 1 ];

        memcpy( atlas->ShadowRectUvs, m_shadow_rect_uvs, sizeof( m_shadow_rect_uvs ) );
    }

    ImGuiStyle& style = ImGui::GetStyle( );
    style.AntiAliasedLinesUseTex = false;
}

ImFont* c_font_loader::get( int index ) const
{
    if ( index < 0 || index >= ( int ) m_fonts.size( ) )
        return nullptr;
    return m_fonts[ index ].font;
}

ImFont* c_font_loader::get( char const* family, int size,
                            int32_t weight, uint8_t charset, uint32_t quality,
                            font_format::Rasterizer rasterizer ) const
{
    for ( auto const& lf : m_fonts )
    {
        auto const& m = lf.meta;
        if ( m.size != size )
            continue;
        if ( m.weight != weight )
            continue;
        if ( m.charset != charset )
            continue;
        if ( m.quality != ( uint8_t ) quality )
            continue;
        if ( ( uint8_t ) rasterizer != 0xFF &&
             m.rasterizer != ( uint8_t ) rasterizer )
            continue;
        if ( _stricmp( m.family, family ) != 0 )
            continue;
        return lf.font;
    }
    return nullptr;
}

char const* c_font_loader::name( int index ) const
{
    if ( index < 0 || index >= ( int ) m_fonts.size( ) )
        return nullptr;
    return m_fonts[ index ].meta.family;
}

alice::font_format::Rasterizer c_font_loader::rasterizer( int index ) const
{
    if ( index < 0 || index >= ( int ) m_fonts.size( ) )
        return font_format::Rasterizer::DX9;
    return ( font_format::Rasterizer ) m_fonts[ index ].meta.rasterizer;
}

void c_font_loader::clear_runtime_state( )
{
    m_fonts.clear( );
    m_atlas_pixels.clear( );
    m_atlas_w = 0;
    m_atlas_h = 0;
    m_uv_white_x = 0.0f;
    m_uv_white_y = 0.0f;
    m_shadow_valid = false;

    memset( m_shadow_rect_x, 0, sizeof( m_shadow_rect_x ) );
    memset( m_shadow_rect_y, 0, sizeof( m_shadow_rect_y ) );
    memset( m_shadow_rect_w, 0, sizeof( m_shadow_rect_w ) );
    memset( m_shadow_rect_h, 0, sizeof( m_shadow_rect_h ) );
    memset( m_shadow_rect_uvs, 0, sizeof( m_shadow_rect_uvs ) );
}

void c_font_loader::release( )
{
    if ( m_srv && alice::core && alice::core->backend )
        alice::core->backend->destroy_texture( m_srv );
    m_srv = 0;

    clear_runtime_state( );
}
