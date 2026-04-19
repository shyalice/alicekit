#include "opengl_backend.hpp"
#include "blur/blur.hpp"
#include "glow/glow.hpp"
#include "grayscale/grayscale.hpp"
#include "models/models.hpp"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

using alice::c_opengl_backend;

c_opengl_backend::c_opengl_backend( int w, int h )
    : width( w )
    , height( h )
{
}

std::unique_ptr< alice::c_blur > c_opengl_backend::make_blur( )
{
    auto blur = std::make_unique< c_ogl_blur >( );
    if ( !blur->init( width, height ) )
        return nullptr;
    return blur;
}

std::unique_ptr< alice::c_glow > c_opengl_backend::make_glow( )
{
    auto glow = std::make_unique< c_opengl_glow >( );
    if ( !glow->init( width, height ) )
        return nullptr;
    return glow;
}

std::unique_ptr< alice::c_grayscale > c_opengl_backend::make_grayscale( )
{
    auto gs = std::make_unique< c_opengl_grayscale >( );
    if ( !gs->init( width, height ) )
        return nullptr;
    return gs;
}

std::unique_ptr< alice::c_model_engine > c_opengl_backend::make_model_engine( )
{
    return std::make_unique< c_ogl_model_engine >( );
}

ImTextureID c_opengl_backend::create_texture_rgba( void const* rgba, int w, int h )
{
    unsigned int tex = 0;
    glGenTextures( 1, &tex );
    glBindTexture( GL_TEXTURE_2D, tex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba );
    return static_cast< ImTextureID >( tex );
}

void c_opengl_backend::destroy_texture( ImTextureID id )
{
    auto tex = static_cast< unsigned int >( ( uintptr_t ) id );
    if ( tex )
        glDeleteTextures( 1, &tex );
}
