//
// Created by rei on 4/8/2026.
//

#include "alice_kit.hpp"

#include "blur/blur.hpp"
#include "glow/glow.hpp"
#include "grayscale/grayscale.hpp"
#include "fonts/fonts.hpp"
#include "logger/logger.hpp"
#include "model_engine/model_engine.hpp"
#include "ui/ui.hpp"
#include "assets/assets.hpp"
#include "windows/windows.hpp"

#include "imgui.h"

using alice::c_core;

std::unique_ptr< c_core > alice::core;

c_core::c_core( )
{
    log_info( "alicekit 2.0 - alina.pw / @evernesseu" );

    ui = std::make_unique< c_ui >( );
    fonts = std::make_unique< c_fonts >( );
    assets = std::make_unique< c_assets >( );
    windows = std::make_unique< c_windows >( );
}

c_core::~c_core( ) = default;