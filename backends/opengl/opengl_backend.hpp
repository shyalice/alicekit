#pragma once

#include "backends/backend.hpp"

namespace alice
{
    class c_opengl_backend final : public i_backend
    {
    public:
        c_opengl_backend( int width, int height );

        std::unique_ptr< c_blur > make_blur( ) override;
        std::unique_ptr< c_glow > make_glow( ) override;
        std::unique_ptr< c_grayscale > make_grayscale( ) override;
        std::unique_ptr< c_model_engine > make_model_engine( ) override;

        ImTextureID create_texture_rgba( void const* rgba, int width, int height ) override;
        void destroy_texture( ImTextureID id ) override;

    private:
        int width = 0;
        int height = 0;
    };
} // namespace alice
