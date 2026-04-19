//
// Created by rei on 4/12/2026.
//

#pragma once

#include "imgui.h"

#include <functional>
#include <string>

namespace alice
{
    class c_assets
    {
        static std::unordered_map< std::string, ImTextureID > assets_list;
        static std::function< void( void const* data, size_t data_size, ImTextureID* out, bool pixel_perfect ) > loader_fn;

    public:
        c_assets( );
        ~c_assets( );

        ImTextureID get( std::string const& key );

        void load_premade( );
        void init_loader( std::function< void( void const* data, size_t data_size, ImTextureID* out, bool pixel_perfect ) > const& load_fn );
    };
} // namespace alice