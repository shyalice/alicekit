#pragma once

#include "imgui.h"

#include <memory>

namespace alice
{
    class c_blur;
    class c_glow;
    class c_grayscale;
    class c_model_engine;

    class i_backend
    {
    public:
        virtual ~i_backend( ) = default;

        virtual std::unique_ptr< c_blur > make_blur( ) = 0;
        virtual std::unique_ptr< c_glow > make_glow( ) = 0;
        virtual std::unique_ptr< c_grayscale > make_grayscale( ) = 0;
        virtual std::unique_ptr< c_model_engine > make_model_engine( ) = 0;

        virtual ImTextureID create_texture_rgba( void const* rgba, int width, int height ) = 0;
        virtual void destroy_texture( ImTextureID id ) = 0;
    };
} // namespace alice
