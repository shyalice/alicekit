#include "fonts.hpp"

#include "packed/font_atlas.hpp"
#include "packed/font_data.hpp"

#include <iostream>
#include <ostream>

using alice::c_fonts;
using alice::font_t;

font_t& c_fonts::get( std::string const& key, float scale )
{
    std::string scale_key = "_";

    if ( scale == 1.0f )
        scale_key += "100";
    else if ( scale == 1.5f )
        scale_key += "150";
    else if ( scale == 2.0f )
        scale_key += "200";

    if ( !m_list.contains( key + scale_key ) )
        return m_list[ key ];

    return m_list[ key + scale_key ];
}

void c_fonts::initialize( )
{
    m_loader = std::make_unique< c_font_loader >( );

    if ( !m_loader->load_from_memory( fonts::kFontAtlas, sizeof fonts::kFontAtlas, fonts::kFontData, sizeof fonts::kFontData ) )
    {
        std::cout << "[everness::alice] couldn't load font data" << std::endl;
        return;
    }

    ImGuiIO& io = ImGui::GetIO( );
    io.Fonts->TexGlyphPadding = 1;

    m_list[ std::string( "primary" ) ] = { m_loader->get( 0 ), m_loader->get( 0 )->FontSize };

    m_list[ std::string( "subtitle" ) ] = { m_loader->get( 1 ), m_loader->get( 1 )->FontSize };

    m_list[ std::string( "bold" ) ] = { m_loader->get( 2 ), m_loader->get( 2 )->FontSize };

    m_list[ std::string( "verdana" ) ] = { m_loader->get( 3 ), m_loader->get( 3 )->FontSize };

    m_list[ std::string( "undefeated" ) ] = { m_loader->get( 4 ), m_loader->get( 4 )->FontSize };

    m_list[ std::string( "smallest_pixel" ) ] = { m_loader->get( 5 ), m_loader->get( 5 )->FontSize };

    m_list[ std::string( "tahoma" ) ] = { m_loader->get( 6 ), m_loader->get( 6 )->FontSize };
    m_list[ std::string( "segoeui" ) ] = { m_loader->get( 7 ), m_loader->get( 7 )->FontSize };

    m_list[ std::string( "calibri" ) ] = { m_loader->get( 8 ), m_loader->get( 8 )->FontSize };

    m_list[ std::string( "small" ) ] = { m_loader->get( 9 ), m_loader->get( 9 )->FontSize };
}

alice::c_font_loader* c_fonts::get_loader( )
{
    return m_loader.get( );
}

void c_fonts::bind( )
{
    m_loader->bind_atlas( );
}