#include "loader.hpp"
#include "pmx/pmx.hpp"

#include <filesystem>
#include <algorithm>
#include <cctype>

namespace alice::formats
{
    static std::string lower_ext( std::string const& path )
    {
        auto ext = std::filesystem::path( path ).extension( ).string( );
        std::ranges::transform( ext, ext.begin( ), []( unsigned char c ) { return static_cast< char >( std::tolower( c ) ); } );
        return ext;
    }

    bool parse( std::string const& path, glb::scene_data_t& out )
    {
        auto const ext = lower_ext( path );
        if ( ext == ".pmx" )
            return pmx::parse( path, out );
        if ( ext == ".glb" || ext == ".gltf" )
            return glb::parse( path, out );
        return false;
    }

    bool is_tpose_format( std::string const& path )
    {
        return lower_ext( path ) == ".pmx";
    }
} // namespace alice::formats
