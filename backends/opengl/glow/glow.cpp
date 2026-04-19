#include "glow.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

using alice::c_opengl_glow;

static constexpr char const* k_imgui_vs = R"glsl(
#version 150
uniform mat4 u_proj;
in vec2 a_pos;
in vec2 a_uv;
in vec4 a_col;
out vec2 v_uv;
out vec4 v_col;
void main( )
{
    v_uv = a_uv;
    v_col = a_col;
    gl_Position = u_proj * vec4( a_pos, 0.0, 1.0 );
}
)glsl";

static constexpr char const* k_imgui_fs = R"glsl(
#version 150
uniform sampler2D u_tex;
in vec2 v_uv;
in vec4 v_col;
out vec4 frag_color;
void main( )
{
    vec4 c = v_col * texture( u_tex, v_uv );
    frag_color = vec4( c.rgb * c.a, c.a );
}
)glsl";

static constexpr char const* k_fs_vs = R"glsl(
#version 150
out vec2 v_uv;
void main( )
{
    float x = ( gl_VertexID == 1 ) ? 2.0 : 0.0;
    float y = ( gl_VertexID == 2 ) ? 2.0 : 0.0;
    v_uv = vec2( x, y );
    gl_Position = vec4( x * 2.0 - 1.0, 1.0 - y * 2.0, 0.0, 1.0 );
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
const int HALF = 24;
float gauss( float x, float s ) { return exp( -( x * x ) / ( 2.0 * s * s ) ); }
void main( )
{
    vec2 dir = ( u_horiz != 0 ) ? vec2( 1, 0 ) : vec2( 0, 1 );
    vec2 texel = 1.0 / u_rt_size;
    float r = max( u_radius, 1.0 );
    float sigma = max( r * 0.5, 1.0 );
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
uniform float u_intensity;
in vec2 v_uv;
out vec4 frag_color;
void main( )
{
    vec4 c = texture( u_tex, v_uv );
    frag_color = vec4( c.rgb * u_intensity, c.a * u_intensity );
}
)glsl";

static GLuint compile_shader( GLenum type, char const* src )
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

static GLuint link_program( GLuint vs, GLuint fs )
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

c_opengl_glow::~c_opengl_glow( )
{
    destroy( );
}

bool c_opengl_glow::init( int w, int h )
{
    width = w;
    height = h;
    if ( shaders_ready )
        return true;
    if ( !create_shaders( ) )
        return false;

    glGenVertexArrays( 1, &empty_vao );

    glGenVertexArrays( 1, &vao );
    glGenBuffers( 1, &vbo );
    glGenBuffers( 1, &ebo );

    glBindVertexArray( vao );
    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ebo );

    GLint loc_pos = glGetAttribLocation( prog_imgui, "a_pos" );
    GLint loc_uv = glGetAttribLocation( prog_imgui, "a_uv" );
    GLint loc_col = glGetAttribLocation( prog_imgui, "a_col" );
    if ( loc_pos >= 0 )
    {
        glEnableVertexAttribArray( loc_pos );
        glVertexAttribPointer( loc_pos, 2, GL_FLOAT, GL_FALSE, sizeof( ImDrawVert ),
                               reinterpret_cast<void*>(static_cast<intptr_t>(offsetof( ImDrawVert, pos ))) );
    }
    if ( loc_uv >= 0 )
    {
        glEnableVertexAttribArray( loc_uv );
        glVertexAttribPointer( loc_uv, 2, GL_FLOAT, GL_FALSE, sizeof( ImDrawVert ),
                               reinterpret_cast<void*>(static_cast<intptr_t>(offsetof( ImDrawVert, uv ))) );
    }
    if ( loc_col >= 0 )
    {
        glEnableVertexAttribArray( loc_col );
        glVertexAttribPointer( loc_col, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof( ImDrawVert ),
                               reinterpret_cast<void*>(static_cast<intptr_t>(offsetof( ImDrawVert, col ))) );
    }
    glBindVertexArray( 0 );

    shaders_ready = true;
    return true;
}

void c_opengl_glow::destroy( )
{
    release_fbos( );
    if ( prog_imgui ) { glDeleteProgram( prog_imgui ); prog_imgui = 0; }
    if ( prog_blur ) { glDeleteProgram( prog_blur ); prog_blur = 0; }
    if ( prog_composite ) { glDeleteProgram( prog_composite ); prog_composite = 0; }
    if ( empty_vao ) { glDeleteVertexArrays( 1, &empty_vao ); empty_vao = 0; }
    if ( vao ) { glDeleteVertexArrays( 1, &vao ); vao = 0; }
    if ( vbo ) { glDeleteBuffers( 1, &vbo ); vbo = 0; }
    if ( ebo ) { glDeleteBuffers( 1, &ebo ); ebo = 0; }
    vbo_capacity = 0;
    ebo_capacity = 0;
    shaders_ready = false;
}

void c_opengl_glow::resize( int w, int h )
{
    if ( w == width && h == height )
        return;
    width = w;
    height = h;
    release_fbos( );
}

void c_opengl_glow::apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                            float radius, float intensity )
{
    if ( !dl || !shaders_ready )
        return;
    if ( vtx_end <= vtx_start || idx_end <= idx_start )
        return;

    auto* payload = new callback_data_t { };
    payload->renderer = this;
    payload->draw_list = dl;
    payload->cmd.vtx_start = vtx_start;
    payload->cmd.vtx_end = vtx_end;
    payload->cmd.idx_start = idx_start;
    payload->cmd.idx_end = idx_end;
    payload->cmd.radius = radius;
    payload->cmd.intensity = intensity;

    dl->AddCallback( &c_opengl_glow::glow_callback, payload );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_opengl_glow::render_original( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end )
{
    if ( !dl || !shaders_ready )
        return;

    auto* payload = new callback_data_t { };
    payload->renderer = this;
    payload->draw_list = dl;
    payload->cmd.vtx_start = vtx_start;
    payload->cmd.vtx_end = vtx_end;
    payload->cmd.idx_start = idx_start;
    payload->cmd.idx_end = idx_end;
    payload->cmd.clip_rect = { 0.f, 0.f, static_cast<float>(width), static_cast<float>(height) };

    dl->AddCallback( &c_opengl_glow::original_callback, payload );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

ImVec4 c_opengl_glow::intersect_clips( ImVec4 const& a, ImVec4 const& b )
{
    return { std::max( a.x, b.x ), std::max( a.y, b.y ),
             std::min( a.z, b.z ), std::min( a.w, b.w ) };
}

bool c_opengl_glow::create_shaders( )
{
    GLuint vs_imgui = compile_shader( GL_VERTEX_SHADER, k_imgui_vs );
    GLuint fs_imgui = compile_shader( GL_FRAGMENT_SHADER, k_imgui_fs );
    if ( !vs_imgui || !fs_imgui )
        return false;
    prog_imgui = link_program( vs_imgui, fs_imgui );
    glDeleteShader( vs_imgui );
    glDeleteShader( fs_imgui );
    if ( !prog_imgui )
        return false;
    glUseProgram( prog_imgui );
    glUniform1i( glGetUniformLocation( prog_imgui, "u_tex" ), 0 );
    loc_imgui_proj = glGetUniformLocation( prog_imgui, "u_proj" );

    GLuint vs_fs = compile_shader( GL_VERTEX_SHADER, k_fs_vs );
    GLuint fs_blur = compile_shader( GL_FRAGMENT_SHADER, k_blur_fs );
    if ( !vs_fs || !fs_blur )
        return false;
    prog_blur = link_program( vs_fs, fs_blur );
    glDeleteShader( fs_blur );
    if ( !prog_blur )
    {
        glDeleteShader( vs_fs );
        return false;
    }
    glUseProgram( prog_blur );
    glUniform1i( glGetUniformLocation( prog_blur, "u_tex" ), 0 );
    loc_blur_radius = glGetUniformLocation( prog_blur, "u_radius" );
    loc_blur_rt_size = glGetUniformLocation( prog_blur, "u_rt_size" );
    loc_blur_horiz = glGetUniformLocation( prog_blur, "u_horiz" );

    GLuint fs_comp = compile_shader( GL_FRAGMENT_SHADER, k_composite_fs );
    if ( !fs_comp )
    {
        glDeleteShader( vs_fs );
        return false;
    }
    prog_composite = link_program( vs_fs, fs_comp );
    glDeleteShader( vs_fs );
    glDeleteShader( fs_comp );
    if ( !prog_composite )
        return false;
    glUseProgram( prog_composite );
    glUniform1i( glGetUniformLocation( prog_composite, "u_tex" ), 0 );
    loc_comp_intensity = glGetUniformLocation( prog_composite, "u_intensity" );

    glUseProgram( 0 );
    return true;
}

bool c_opengl_glow::create_fbo( unsigned int& out_fbo, unsigned int& out_tex, int w, int h )
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

void c_opengl_glow::release_fbos( )
{
    auto release = [ ]( unsigned int& f, unsigned int& t ) {
        if ( f ) { glDeleteFramebuffers( 1, &f ); f = 0; }
        if ( t ) { glDeleteTextures( 1, &t ); t = 0; }
    };
    release( fbo_src, tex_src );
    release( fbo_ping, tex_ping );
    release( fbo_pong, tex_pong );
    release( fbo_ping2, tex_ping2 );
    release( fbo_pong2, tex_pong2 );
    fbo_w = 0;
    fbo_h = 0;
    small_w = 0;
    small_h = 0;
}

bool c_opengl_glow::ensure_fbos( int w, int h )
{
    if ( w == fbo_w && h == fbo_h && tex_src )
        return true;

    release_fbos( );
    fbo_w = w;
    fbo_h = h;
    small_w = std::max( 1, w / 2 );
    small_h = std::max( 1, h / 2 );

    if ( !create_fbo( fbo_src, tex_src, w, h ) )
        return false;
    if ( !create_fbo( fbo_ping, tex_ping, small_w, small_h ) )
        return false;
    if ( !create_fbo( fbo_pong, tex_pong, small_w, small_h ) )
        return false;
    if ( !create_fbo( fbo_ping2, tex_ping2, small_w, small_h ) )
        return false;
    if ( !create_fbo( fbo_pong2, tex_pong2, small_w, small_h ) )
        return false;
    return true;
}

void c_opengl_glow::ensure_buffers( int vtx_count, int idx_count )
{
    int const vtx_bytes = vtx_count * static_cast<int>(sizeof( ImDrawVert ));
    int const idx_bytes = idx_count * static_cast<int>(sizeof( ImDrawIdx ));

    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    if ( vtx_bytes > vbo_capacity )
    {
        vbo_capacity = vtx_bytes + 1024 * static_cast<int>(sizeof( ImDrawVert ));
        glBufferData( GL_ARRAY_BUFFER, vbo_capacity, nullptr, GL_STREAM_DRAW );
    }

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ebo );
    if ( idx_bytes > ebo_capacity )
    {
        ebo_capacity = idx_bytes + 2048 * static_cast<int>(sizeof( ImDrawIdx ));
        glBufferData( GL_ELEMENT_ARRAY_BUFFER, ebo_capacity, nullptr, GL_STREAM_DRAW );
    }
}

void c_opengl_glow::blit_to_fbo( glow_cmd_t const& cmd, ImDrawList* dl, unsigned int dst_fbo, int w, int h, bool clear )
{
    int const vtx_count = cmd.vtx_end - cmd.vtx_start;
    int const idx_count = cmd.idx_end - cmd.idx_start;
    if ( vtx_count <= 0 || idx_count <= 0 )
        return;

    ensure_buffers( vtx_count, idx_count );

    glBindBuffer( GL_ARRAY_BUFFER, vbo );
    glBufferSubData( GL_ARRAY_BUFFER, 0, vtx_count * static_cast<GLsizeiptr>(sizeof( ImDrawVert )),
                     dl->VtxBuffer.Data + cmd.vtx_start );

    std::vector< ImDrawIdx > remapped( static_cast<size_t>(idx_count) );
    for ( int i = 0; i < idx_count; ++i )
        remapped[ i ] = dl->IdxBuffer.Data[ cmd.idx_start + i ] - static_cast<ImDrawIdx>(cmd.vtx_start);

    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, ebo );
    glBufferSubData( GL_ELEMENT_ARRAY_BUFFER, 0, idx_count * static_cast<GLsizeiptr>(sizeof( ImDrawIdx )),
                     remapped.data( ) );

    glBindFramebuffer( GL_FRAMEBUFFER, dst_fbo );
    glViewport( 0, 0, w, h );
    if ( clear )
    {
        glClearColor( 0, 0, 0, 0 );
        glClear( GL_COLOR_BUFFER_BIT );
    }

    glDisable( GL_DEPTH_TEST );
    glDisable( GL_CULL_FACE );
    glEnable( GL_BLEND );
    glBlendEquationSeparate( GL_FUNC_ADD, GL_FUNC_ADD );
    glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );
    glEnable( GL_SCISSOR_TEST );

    glUseProgram( prog_imgui );

    float const L = 0.f, R = static_cast<float>(w);
    float const T = 0.f, B = static_cast<float>(h);
    float const proj[ 16 ] = {
        2.f / ( R - L ), 0, 0, 0,
        0, 2.f / ( T - B ), 0, 0,
        0, 0, -1.f, 0,
        ( R + L ) / ( L - R ), ( T + B ) / ( B - T ), 0, 1
    };
    glUniformMatrix4fv( loc_imgui_proj, 1, GL_FALSE, proj );

    glActiveTexture( GL_TEXTURE0 );
    glBindVertexArray( vao );

    int running_idx = 0;
    for ( int ci = 0; ci < dl->CmdBuffer.Size; ++ci )
    {
        ImDrawCmd const& dc = dl->CmdBuffer[ ci ];

        if ( dc.UserCallback != nullptr )
        {
            running_idx += static_cast<int>(dc.ElemCount);
            continue;
        }

        int const draw_start = running_idx;
        int const draw_end = running_idx + static_cast<int>(dc.ElemCount);
        int const overlap_s = std::max( draw_start, cmd.idx_start );
        int const overlap_e = std::min( draw_end, cmd.idx_end );

        if ( overlap_e > overlap_s )
        {
            ImVec4 const clip = intersect_clips( cmd.clip_rect, dc.ClipRect );
            if ( clip.x < clip.z && clip.y < clip.w )
            {
                int const scx = static_cast<int>(std::max( 0.f, clip.x ));
                int const scy2 = static_cast<int>(std::min( static_cast<float>(h), clip.w ));
                int const scx2 = static_cast<int>(std::min( static_cast<float>(w), clip.z ));
                int const scy_top = static_cast<int>(std::max( 0.f, clip.y ));
                int const scw = scx2 - scx;
                int const sch = scy2 - scy_top;
                if ( scw > 0 && sch > 0 )
                {
                    glScissor( scx, h - scy2, scw, sch );
                    auto tex = static_cast<GLuint>(dc.TextureId);
                    glBindTexture( GL_TEXTURE_2D, tex );
                    int const local_off = overlap_s - cmd.idx_start;
                    int const local_count = overlap_e - overlap_s;
                    glDrawElements( GL_TRIANGLES, local_count,
                                    sizeof( ImDrawIdx ) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT,
                                    reinterpret_cast<void*>(static_cast<intptr_t>(local_off * sizeof( ImDrawIdx ))) );
                }
            }
        }

        running_idx += static_cast<int>(dc.ElemCount);
    }

    glBindVertexArray( 0 );
}

void c_opengl_glow::blur_pass( unsigned int src_tex, unsigned int fbo_h_, unsigned int tex_h_,
                                unsigned int fbo_v_, unsigned int tex_v_, int w, int h, float radius )
{
    glDisable( GL_SCISSOR_TEST );
    glDisable( GL_BLEND );
    glViewport( 0, 0, w, h );
    glBindVertexArray( empty_vao );
    glUseProgram( prog_blur );
    glUniform1f( loc_blur_radius, radius );
    glUniform2f( loc_blur_rt_size, static_cast<float>(w), static_cast<float>(h) );

    glActiveTexture( GL_TEXTURE0 );

    glBindFramebuffer( GL_FRAMEBUFFER, fbo_h_ );
    glClearColor( 0, 0, 0, 0 );
    glClear( GL_COLOR_BUFFER_BIT );
    glUniform1i( loc_blur_horiz, 1 );
    glBindTexture( GL_TEXTURE_2D, src_tex );
    glDrawArrays( GL_TRIANGLES, 0, 3 );

    glBindFramebuffer( GL_FRAMEBUFFER, fbo_v_ );
    glClear( GL_COLOR_BUFFER_BIT );
    glUniform1i( loc_blur_horiz, 0 );
    glBindTexture( GL_TEXTURE_2D, tex_h_ );
    glDrawArrays( GL_TRIANGLES, 0, 3 );

    ( void ) tex_v_;
    glBindVertexArray( 0 );
}

void c_opengl_glow::composite_pass( unsigned int src_tex, glow_cmd_t const& cmd, int w, int h, float intensity )
{
    glBindFramebuffer( GL_FRAMEBUFFER, saved.draw_fbo );
    glViewport( 0, 0, w, h );

    glEnable( GL_BLEND );
    glBlendEquationSeparate( GL_FUNC_ADD, GL_FUNC_ADD );
    glBlendFuncSeparate( GL_ONE, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

    int const scx = static_cast<int>(std::max( 0.f, cmd.clip_rect.x ));
    int const scy_top = static_cast<int>(std::max( 0.f, cmd.clip_rect.y ));
    int const scx2 = static_cast<int>(std::min( static_cast<float>(w), cmd.clip_rect.z ));
    int const scy2 = static_cast<int>(std::min( static_cast<float>(h), cmd.clip_rect.w ));
    int const scw = scx2 - scx;
    int const sch = scy2 - scy_top;
    if ( scw <= 0 || sch <= 0 )
        return;

    glEnable( GL_SCISSOR_TEST );
    glScissor( scx, h - scy2, scw, sch );

    glBindVertexArray( empty_vao );
    glUseProgram( prog_composite );
    glUniform1f( loc_comp_intensity, intensity );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, src_tex );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    glBindVertexArray( 0 );
}

void c_opengl_glow::render_glow( glow_cmd_t const& cmd, ImDrawList* dl )
{
    GLint vp[ 4 ];
    glGetIntegerv( GL_VIEWPORT, vp );
    int const w = vp[ 2 ];
    int const h = vp[ 3 ];
    if ( w <= 0 || h <= 0 )
        return;

    if ( !ensure_fbos( w, h ) )
        return;

    blit_to_fbo( cmd, dl, fbo_src, w, h, true );
    blur_pass( tex_src, fbo_ping, tex_ping, fbo_pong, tex_pong, small_w, small_h, cmd.radius * 0.45f );
    blur_pass( tex_pong, fbo_ping2, tex_ping2, fbo_pong2, tex_pong2, small_w, small_h, cmd.radius );

    composite_pass( tex_pong, cmd, w, h, cmd.intensity * 0.35f );
    composite_pass( tex_pong2, cmd, w, h, cmd.intensity * 0.85f );

    blit_to_fbo( cmd, dl, fbo_src, w, h, true );
    composite_pass( tex_src, cmd, w, h, 1.f );
}

void c_opengl_glow::original_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer || !data->draw_list )
        return;

    auto* r = data->renderer;
    auto* dl = data->draw_list;
    glow_cmd_t cmd = data->cmd;
    cmd.clip_rect = draw_cmd->ClipRect;

    r->save_state( );

    GLint vp[ 4 ];
    glGetIntegerv( GL_VIEWPORT, vp );
    int const w = vp[ 2 ];
    int const h = vp[ 3 ];
    r->blit_to_fbo( cmd, dl, r->saved.draw_fbo, w, h, false );

    r->restore_state( );
    delete data;
}

void c_opengl_glow::glow_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer || !data->draw_list )
        return;

    auto* r = data->renderer;
    auto* dl = data->draw_list;
    glow_cmd_t cmd = data->cmd;
    cmd.clip_rect = draw_cmd->ClipRect;

    r->save_state( );
    r->render_glow( cmd, dl );
    r->restore_state( );

    for ( int i = cmd.vtx_start; i < cmd.vtx_end; ++i )
        dl->VtxBuffer.Data[ i ].col &= 0x00FFFFFF;

    delete data;
}

void c_opengl_glow::save_state( )
{
    glGetIntegerv( GL_DRAW_FRAMEBUFFER_BINDING, &saved.draw_fbo );
    glGetIntegerv( GL_READ_FRAMEBUFFER_BINDING, &saved.read_fbo );
    glGetIntegerv( GL_CURRENT_PROGRAM, &saved.program );
    glGetIntegerv( GL_VERTEX_ARRAY_BINDING, &saved.vao );
    glGetIntegerv( GL_ARRAY_BUFFER_BINDING, &saved.array_buffer );
    glGetIntegerv( GL_ELEMENT_ARRAY_BUFFER_BINDING, &saved.element_buffer );
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

void c_opengl_glow::restore_state( )
{
    glBindFramebuffer( GL_DRAW_FRAMEBUFFER, saved.draw_fbo );
    glBindFramebuffer( GL_READ_FRAMEBUFFER, saved.read_fbo );
    glUseProgram( saved.program );
    glBindVertexArray( saved.vao );
    glBindBuffer( GL_ARRAY_BUFFER, saved.array_buffer );
    glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, saved.element_buffer );
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
