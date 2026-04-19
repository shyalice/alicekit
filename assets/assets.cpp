//
// Created by rei on 4/12/2026.
//

#include "assets.hpp"

#include "premade/logo.hpp"
#include "premade/logo_alt.hpp"
#include "premade/banner.hpp"

using alice::c_assets;

c_assets::c_assets( ) = default;
c_assets::~c_assets( ) = default;

std::unordered_map< std::string, ImTextureID > c_assets::assets_list { };
std::function< void( void const* data, size_t data_size, ImTextureID* out, bool pixel_perfect ) > c_assets::loader_fn { };

ImTextureID c_assets::get( std::string const& key )
{
    return assets_list[ key ];
}

void c_assets::load_premade( )
{
    loader_fn( tex::k_logo_alt_png, sizeof tex::k_logo_alt_png, &assets_list[ "logo" ], true );
    loader_fn( tex::k_banner_png, sizeof tex::k_banner_png, &assets_list[ "banner" ], false );
}

void c_assets::init_loader( std::function< void( void const* data, size_t data_size, ImTextureID* out, bool pixel_perfect ) > const& load_fn )
{
    loader_fn = load_fn;
}