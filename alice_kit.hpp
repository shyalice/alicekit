//
// Created by rei on 4/8/2026.
//

#pragma once

#include "assets/assets.hpp"
#include "backends/backend.hpp"
#include "blur/blur.hpp"
#include "glow/glow.hpp"
#include "grayscale/grayscale.hpp"
#include "model_engine/model_engine.hpp"

#include <memory>
#include <utility>

namespace alice
{
    class c_ui;
    class c_fonts;
    class c_assets;
    class c_windows;

    class c_core
    {
    public:
        c_core( );
        ~c_core( );

        template < typename Backend, typename... Args >
        void init_backend( Args&&... args )
        {
            backend = std::make_unique< Backend >( std::forward< Args >( args )... );
            blur = backend->make_blur( );
            glow = backend->make_glow( );
            grayscale = backend->make_grayscale( );
            model_engine = backend->make_model_engine( );
        }

        std::unique_ptr< c_ui > ui;
        std::unique_ptr< c_fonts > fonts;
        std::unique_ptr< c_assets > assets;
        std::unique_ptr< i_backend > backend;
        std::unique_ptr< c_blur > blur;
        std::unique_ptr< c_glow > glow;
        std::unique_ptr< c_grayscale > grayscale;
        std::unique_ptr< c_model_engine > model_engine;
        std::unique_ptr< c_windows > windows;
    };

    extern std::unique_ptr< c_core > core;
} // namespace alice