//
// font_loader.hpp
//

#pragma once

#include "font_data_format.hpp"

#include <cstdint>
#include <vector>

#include "imgui.h"

struct ImFont;
struct ImVec4;

namespace alice
{
    class c_font_loader
    {
    public:
        bool load( char const* atlas_path, char const* data_path );

        bool load_from_memory(
            void const* atlas_data, size_t atlas_size,
            void const* font_data, size_t font_data_size );

        void bind_atlas( );

        ImFont* get( int index ) const;

        ImFont* get( char const* family, int size,
                     int32_t weight = 400,
                     uint8_t charset = 0,
                     uint32_t quality = 0,
                     font_format::Rasterizer rasterizer = font_format::Rasterizer( -1 ) ) const;

        char const* name( int index ) const;
        int font_count( ) const { return ( int ) m_fonts.size( ); }

        font_format::Rasterizer rasterizer( int index ) const;

        void release( );

    private:
        struct memory_reader_t
        {
            uint8_t const* data = nullptr;
            size_t size = 0;
            size_t offset = 0;

            bool read( void* dst, size_t bytes );
            bool skip( size_t bytes );
            size_t remaining( ) const;
        };

        int m_shadow_rect_x[ 2 ] { };
        int m_shadow_rect_y[ 2 ] { };
        int m_shadow_rect_w[ 2 ] { };
        int m_shadow_rect_h[ 2 ] { };
        ImVec4 m_shadow_rect_uvs[ 10 ] { };
        bool m_shadow_valid = false;

        struct loaded_font_t
        {
            font_format::FontEntry meta;
            ImFont* font = nullptr;
            ImFontConfig source_cfg { };
        };

        bool load_atlas( char const* path );
        bool load_data( char const* path );

        bool load_atlas_from_memory( void const* data, size_t size );
        bool load_data_from_memory( void const* data, size_t size );

        bool parse_atlas( memory_reader_t& rd );
        bool parse_data( memory_reader_t& rd );

        bool create_gpu_atlas( );

        void apply_atlas_postprocess( );
        void clear_runtime_state( );

        ImTextureID m_srv = 0;

        std::vector< loaded_font_t > m_fonts;
        std::vector< uint32_t > m_atlas_pixels;
        uint32_t m_atlas_w = 0;
        uint32_t m_atlas_h = 0;
        float m_uv_white_x = 0.0f;
        float m_uv_white_y = 0.0f;
    };
} // namespace alice
