#include "blur.hpp"

#include <algorithm>
#include <cstring>
#include <cmath>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

using alice::c_ogl_blur;

static constexpr char const* k_fullscreen_vs = R"glsl(
#version 150
out vec2 v_uv;
void main( )
{
    float x = ( gl_VertexID == 1 ) ? 2.0 : 0.0;
    float y = ( gl_VertexID == 2 ) ? 2.0 : 0.0;
    v_uv = vec2( x, y );
    gl_Position = vec4( x * 2.0 - 1.0, y * 2.0 - 1.0, 0.0, 1.0 );
}
)glsl";

static constexpr char const* k_blur_fs = R"glsl(
#version 150
uniform sampler2D u_tex;
uniform float u_radius;
uniform vec2 u_rt_size;
uniform int u_horiz;
in vec2 v_uv;
out vec4 frag_color;
const int HALF = 32;
float gauss( float x, float s ) { return exp( -( x * x ) / ( 2.0 * s * s ) ); }
void main( )
{
    vec2 dir = ( u_horiz != 0 ) ? vec2( 1, 0 ) : vec2( 0, 1 );
    vec2 texel = 1.0 / u_rt_size;
    float r = max( u_radius, 1.0 );
    float sigma = r * 0.4;
    vec4 acc = vec4( 0 );
    float wsum = 0.0;
    for ( int k = -HALF; k <= HALF; ++k )
    {
        float x = float( k );
        float w = gauss( x, sigma );
        acc += texture( u_tex, v_uv + dir * texel * ( x * ( r / float( HALF ) ) ) ) * w;
        wsum += w;
    }
    frag_color = acc / max( wsum, 0.0001 );
}
)glsl";

static constexpr char const* k_composite_fs = R"glsl(
#version 150
uniform sampler2D u_tex;
uniform vec2 u_rect_min;
uniform vec2 u_rect_max;
uniform vec2 u_rt_size;
uniform float u_rounding;
uniform float u_alpha;
in vec2 v_uv;
out vec4 frag_color;

float sdf_rounded_box( vec2 p, vec2 b, float r )
{
    vec2 q = abs( p ) - b + r;
    return length( max( q, 0.0 ) ) + min( max( q.x, q.y ), 0.0 ) - r;
}

void main( )
{
    vec2 sp = vec2( v_uv.x, 1.0 - v_uv.y ) * u_rt_size;
    vec2 center = ( u_rect_min + u_rect_max ) * 0.5;
    vec2 halfsize = ( u_rect_max - u_rect_min ) * 0.5;
    float rounding = min( u_rounding, min( halfsize.x, halfsize.y ) );
    float dist = sdf_rounded_box( sp - center, halfsize, rounding );
    float alpha = ( 1.0 - smoothstep( -0.5, 0.0, dist ) ) * clamp( u_alpha, 0.0, 1.0 );
    if ( alpha <= 0.001 )
        discard;
    vec4 c = texture( u_tex, v_uv );
    frag_color = vec4( c.rgb * alpha, alpha );
}
)glsl";

static GLuint compile_blur_shader( GLenum type, char const* src )
{
    GLuint s = glCreateShader( type );
    glShaderSource( s, 1, &src, nullptr );
    glCompileShader( s );
    GLint ok = 0;
    glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
    if ( !ok )
    {
        glDeleteShader( s );
        return 0;
    }
    return s;
}

static GLuint link_blur_program( GLuint vs, GLuint fs )
{
    GLuint p = glCreateProgram( );
    glAttachShader( p, vs );
    glAttachShader( p, fs );
    glLinkProgram( p );
    GLint ok = 0;
    glGetProgramiv( p, GL_LINK_STATUS, &ok );
    if ( !ok )
    {
        glDeleteProgram( p );
        return 0;
    }
    return p;
}

c_ogl_blur::~c_ogl_blur( )
{
    destroy( );
}

bool c_ogl_blur::init( int w, int h )
{
    width = w;
    height = h;

    if ( shaders_ready )
        return true;
    if ( !create_shaders( ) )
        return false;

    glGenVertexArrays( 1, &empty_vao );

    shaders_ready = true;
    return true;
}

void c_ogl_blur::resize( int w, int h )
{
    if ( w == width && h == height )
        return;
    width = w;
    height = h;
    release_fbos( );
}

void c_ogl_blur::prepare_frame( float, int ) { }

void c_ogl_blur::begin( ImDrawList*, float, int ) { }

void c_ogl_blur::apply_squircle( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                                  float radius, float rounding,
                                  float, float, int passes, float alpha )
{
    apply( dl, rect_min, rect_max, radius, rounding, passes, alpha );
}

ImTextureID c_ogl_blur::get_texture( ) const
{
    return static_cast< ImTextureID >( tex_pong );
}

void c_ogl_blur::uv_for( ImVec2 p_min, ImVec2 p_max, ImVec2& uv_min, ImVec2& uv_max ) const
{
    float const inv_w = width > 0 ? 1.f / static_cast< float >( width ) : 0.f;
    float const inv_h = height > 0 ? 1.f / static_cast< float >( height ) : 0.f;
    uv_min = { p_min.x * inv_w, p_min.y * inv_h };
    uv_max = { p_max.x * inv_w, p_max.y * inv_h };
}

void c_ogl_blur::add_image( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col ) const
{
    if ( !dl || tex_pong == 0 )
        return;
    ImVec2 uv_min, uv_max;
    uv_for( p_min, p_max, uv_min, uv_max );
    dl->AddImage( get_texture( ), p_min, p_max, uv_min, uv_max, col );
}

void c_ogl_blur::add_image_rounded( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                     ImDrawFlags flags, ImU32 col ) const
{
    if ( !dl || tex_pong == 0 )
        return;
    ImVec2 uv_min, uv_max;
    uv_for( p_min, p_max, uv_min, uv_max );
    dl->AddImageRounded( get_texture( ), p_min, p_max, uv_min, uv_max, col, rounding, flags );
}

void c_ogl_blur::add_image_squircle( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                      ImDrawFlags flags, float, float, int, ImU32 col ) const
{
    add_image_rounded( dl, p_min, p_max, rounding, flags, col );
}

void c_ogl_blur::destroy( )
{
    release_fbos( );
    if ( prog_blur ) { glDeleteProgram( prog_blur ); prog_blur = 0; }
    if ( prog_composite ) { glDeleteProgram( prog_composite ); prog_composite = 0; }
    if ( empty_vao ) { glDeleteVertexArrays( 1, &empty_vao ); empty_vao = 0; }
    shaders_ready = false;
}

bool c_ogl_blur::create_shaders( )
{
    GLuint vs = compile_blur_shader( GL_VERTEX_SHADER, k_fullscreen_vs );
    if ( !vs )
        return false;

    GLuint fs_blur = compile_blur_shader( GL_FRAGMENT_SHADER, k_blur_fs );
    if ( !fs_blur )
    {
        glDeleteShader( vs );
        return false;
    }

    prog_blur = link_blur_program( vs, fs_blur );
    glDeleteShader( fs_blur );

    if ( !prog_blur )
    {
        glDeleteShader( vs );
        return false;
    }

    glUseProgram( prog_blur );
    glUniform1i( glGetUniformLocation( prog_blur, "u_tex" ), 0 );
    loc_blur_radius = glGetUniformLocation( prog_blur, "u_radius" );
    loc_blur_rt_size = glGetUniformLocation( prog_blur, "u_rt_size" );
    loc_blur_horiz = glGetUniformLocation( prog_blur, "u_horiz" );

    GLuint fs_comp = compile_blur_shader( GL_FRAGMENT_SHADER, k_composite_fs );
    if ( !fs_comp )
    {
        glDeleteShader( vs );
        return false;
    }

    prog_composite = link_blur_program( vs, fs_comp );
    glDeleteShader( vs );
    glDeleteShader( fs_comp );

    if ( !prog_composite )
        return false;

    glUseProgram( prog_composite );
    glUniform1i( glGetUniformLocation( prog_composite, "u_tex" ), 0 );
    loc_comp_rect_min = glGetUniformLocation( prog_composite, "u_rect_min" );
    loc_comp_rect_max = glGetUniformLocation( prog_composite, "u_rect_max" );
    loc_comp_rt_size = glGetUniformLocation( prog_composite, "u_rt_size" );
    loc_comp_rounding = glGetUniformLocation( prog_composite, "u_rounding" );
    loc_comp_alpha = glGetUniformLocation( prog_composite, "u_alpha" );

    glUseProgram( 0 );
    return true;
}

bool c_ogl_blur::create_fbo( unsigned int& out_fbo, unsigned int& out_tex, int w, int h )
{
    glGenTextures( 1, &out_tex );
    glBindTexture( GL_TEXTURE_2D, out_tex );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );

    glGenFramebuffers( 1, &out_fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, out_fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, out_tex, 0 );

    bool const ok = glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
    return ok;
}

void c_ogl_blur::release_fbos( )
{
    auto release = [ ]( unsigned int& f, unsigned int& t ) {
        if ( f ) { glDeleteFramebuffers( 1, &f ); f = 0; }
        if ( t ) { glDeleteTextures( 1, &t ); t = 0; }
    };
    release( fbo_capture, tex_capture );
    release( fbo_ping, tex_ping );
    release( fbo_pong, tex_pong );
    width = 0;
    height = 0;
}

bool c_ogl_blur::ensure_fbos( int w, int h )
{
    if ( w == width && h == height && tex_ping )
        return true;

    release_fbos( );
    width = w;
    height = h;

    if ( !create_fbo( fbo_capture, tex_capture, w, h ) )
        return false;
    if ( !create_fbo( fbo_ping, tex_ping, w, h ) )
        return false;
    if ( !create_fbo( fbo_pong, tex_pong, w, h ) )
        return false;

    return true;
}

void c_ogl_blur::apply( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                    float radius, float rounding, int passes, float alpha )
{
    if ( !dl || !shaders_ready )
        return;
    if ( rect_max.x <= rect_min.x || rect_max.y <= rect_min.y )
        return;
    if ( alpha <= 0.001f )
        return;

    auto* p = new callback_data_t { };
    p->renderer = this;
    p->cmd.min = rect_min;
    p->cmd.max = rect_max;
    p->cmd.radius = radius;
    p->cmd.rounding = rounding;
    p->cmd.alpha = alpha;
    p->cmd.passes = std::max( 1, std::min( passes, 4 ) );

    dl->AddCallback( &c_ogl_blur::blur_callback, p );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_ogl_blur::save_state( )
{
    glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &saved.draw_fbo );
    glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &saved.read_fbo );
    glGetIntegerv( GL_CURRENT_PROGRAM, &saved.program );
    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &saved.vao );
    glGetIntegerv( GL_ACTIVE_TEXTURE, &saved.active_texture );
    glActiveTexture( GL_TEXTURE0 );
    glGetIntegerv( GL_TEXTURE_BINDING_2D, &saved.texture_2d );
    glGetIntegerv( GL_VIEWPORT, saved.viewport );
    glGetIntegerv( GL_SCISSOR_BOX, saved.scissor_box );
    saved.blend = glIsEnabled( GL_BLEND );
    saved.depth_test = glIsEnabled( GL_DEPTH_TEST );
    saved.scissor_test = glIsEnabled( GL_SCISSOR_TEST );
    saved.cull_face = glIsEnabled( GL_CULL_FACE );
    glGetIntegerv( GL_BLEND_SRC_RGB, &saved.blend_src_rgb );
    glGetIntegerv( GL_BLEND_DST_RGB, &saved.blend_dst_rgb );
    glGetIntegerv( GL_BLEND_SRC_ALPHA, &saved.blend_src_alpha );
    glGetIntegerv( GL_BLEND_DST_ALPHA, &saved.blend_dst_alpha );
    glGetIntegerv( GL_BLEND_EQUATION_RGB, &saved.blend_eq_rgb );
    glGetIntegerv( GL_BLEND_EQUATION_ALPHA, &saved.blend_eq_alpha );
}

void c_ogl_blur::restore_state( )
{
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, saved.draw_fbo );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, saved.read_fbo );
    glUseProgram( saved.program );
    glBindVertexArray( saved.vao );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, saved.texture_2d );
    glActiveTexture( saved.active_texture );
    glViewport( saved.viewport[ 0 ], saved.viewport[ 1 ], saved.viewport[ 2 ], saved.viewport[ 3 ] );
    glScissor( saved.scissor_box[ 0 ], saved.scissor_box[ 1 ], saved.scissor_box[ 2 ], saved.scissor_box[ 3 ] );
    if ( saved.blend ) glEnable( GL_BLEND ); else glDisable( GL_BLEND );
    if ( saved.depth_test ) glEnable( GL_DEPTH_TEST ); else glDisable( GL_DEPTH_TEST );
    if ( saved.scissor_test ) glEnable( GL_SCISSOR_TEST ); else glDisable( GL_SCISSOR_TEST );
    if ( saved.cull_face ) glEnable( GL_CULL_FACE ); else glDisable( GL_CULL_FACE );
    glBlendFuncSeparate( saved.blend_src_rgb, saved.blend_dst_rgb, saved.blend_src_alpha, saved.blend_dst_alpha );
    glBlendEquationSeparate( saved.blend_eq_rgb, saved.blend_eq_alpha );
}

void c_ogl_blur::render_blur( blur_cmd_t const& cmd )
{
    GLint vp[ 4 ];
    glGetIntegerv( GL_VIEWPORT, vp );
    int const w = vp[ 2 ];
    int const h = vp[ 3 ];

    if ( w <= 0 || h <= 0 )
        return;
    if ( !ensure_fbos( w, h ) )
        return;

    auto const& io = ImGui::GetIO( );
    float const sx = io.DisplayFramebufferScale.x;
    float const sy = io.DisplayFramebufferScale.y;

    float const fb_min_x = cmd.min.x * sx;
    float const fb_min_y = cmd.min.y * sy;
    float const fb_max_x = cmd.max.x * sx;
    float const fb_max_y = cmd.max.y * sy;
    float const fb_clip_min_x = cmd.clip_min.x * sx;
    float const fb_clip_min_y = cmd.clip_min.y * sy;
    float const fb_clip_max_x = cmd.clip_max.x * sx;
    float const fb_clip_max_y = cmd.clip_max.y * sy;

    glBindFramebuffer( GL_READ_FRAMEBUFFER, saved.draw_fbo );
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo_capture );
    glBlitFramebuffer( 0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST );

    glDisable( GL_DEPTH_TEST );
    glDisable( GL_BLEND );
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_CULL_FACE );
    glViewport( 0, 0, w, h );
    glBindVertexArray( empty_vao );

    glUseProgram( prog_blur );
    glUniform1f( loc_blur_radius, cmd.radius * sx );
    glUniform2f( loc_blur_rt_size, static_cast< float >( w ), static_cast< float >( h ) );

    glActiveTexture( GL_TEXTURE0 );

    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo_ping );
    glUniform1i( loc_blur_horiz, 1 );
    glBindTexture( GL_TEXTURE_2D, tex_capture );
    glDrawArrays( GL_TRIANGLES, 0, 3 );

    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo_pong );
    glUniform1i( loc_blur_horiz, 0 );
    glBindTexture( GL_TEXTURE_2D, tex_ping );
    glDrawArrays( GL_TRIANGLES, 0, 3 );

    for ( int i = 1; i < cmd.passes; ++i )
    {
        float const r = cmd.radius * sx * static_cast< float >( i + 1 ) * 0.6f;
        glUniform1f( loc_blur_radius, r );

        glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo_ping );
        glUniform1i( loc_blur_horiz, 1 );
        glBindTexture( GL_TEXTURE_2D, tex_pong );
        glDrawArrays( GL_TRIANGLES, 0, 3 );

        glBindFramebuffer( GL_DRAW_FRAMEBUFFER, fbo_pong );
        glUniform1i( loc_blur_horiz, 0 );
        glBindTexture( GL_TEXTURE_2D, tex_ping );
        glDrawArrays( GL_TRIANGLES, 0, 3 );
    }

    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, saved.draw_fbo );
    glEnable( GL_BLEND );
    glBlendFunc( GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

    glEnable( GL_SCISSOR_TEST );
    int const scx = static_cast< int >( std::max( 0.f, fb_clip_min_x ) );
    int const scx2 = static_cast< int >( std::min( static_cast< float >( w ), fb_clip_max_x ) );
    int const scy2 = static_cast< int >( std::min( static_cast< float >( h ), fb_clip_max_y ) );
    int const scy_gl = h - scy2;
    int const sch = static_cast< int >( fb_clip_max_y - fb_clip_min_y );
    glScissor( scx, scy_gl, scx2 - scx, sch );

    glUseProgram( prog_composite );
    glUniform2f( loc_comp_rect_min, fb_min_x, fb_min_y );
    glUniform2f( loc_comp_rect_max, fb_max_x, fb_max_y );
    glUniform2f( loc_comp_rt_size, static_cast< float >( w ), static_cast< float >( h ) );
    glUniform1f( loc_comp_rounding, cmd.rounding * sx );
    glUniform1f( loc_comp_alpha, cmd.alpha );

    glBindTexture( GL_TEXTURE_2D, tex_pong );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
}

void c_ogl_blur::blur_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer )
        return;

    auto* r = data->renderer;
    data->cmd.clip_min = { draw_cmd->ClipRect.x, draw_cmd->ClipRect.y };
    data->cmd.clip_max = { draw_cmd->ClipRect.z, draw_cmd->ClipRect.w };

    r->save_state( );
    r->render_blur( data->cmd );
    r->restore_state( );

    delete data;
}

