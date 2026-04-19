#include "grayscale.hpp"

#include <algorithm>
#include <cstring>
#include <vector>

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#else
#include <GL/gl.h>
#endif

using alice::c_opengl_grayscale;

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

static constexpr char const* k_grayscale_fs = R"glsl(
#version 150
uniform sampler2D u_tex;
uniform float u_strength;
in vec2 v_uv;
out vec4 frag_color;
void main( )
{
    vec4 c = texture( u_tex, v_uv );
    float a = max( c.a, 0.0001 );
    vec3 rgb = c.rgb / a;
    float luma = dot( rgb, vec3( 0.299, 0.587, 0.114 ) );
    float s = clamp( u_strength, 0.0, 1.0 );
    vec3 mixed = mix( rgb, vec3( luma ), s );
    frag_color = vec4( mixed * c.a, c.a );
}
)glsl";

static GLuint compile_shader( GLenum type, char const* src )
{
    GLuint s = glCreateShader( type );
    glShaderSource( s, 1, &src, nullptr );
    glCompileShader( s );
    GLint ok = 0;
    glGetShaderiv( s, GL_COMPILE_STATUS, &ok );
    if ( !ok ) { glDeleteShader( s ); return 0; }
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
    if ( !ok ) { glDeleteProgram( p ); return 0; }
    return p;
}

c_opengl_grayscale::~c_opengl_grayscale( )
{
    destroy( );
}

bool c_opengl_grayscale::init( int w, int h )
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

void c_opengl_grayscale::destroy( )
{
    release_fbos( );
    if ( prog_imgui ) { glDeleteProgram( prog_imgui ); prog_imgui = 0; }
    if ( prog_grayscale ) { glDeleteProgram( prog_grayscale ); prog_grayscale = 0; }
    if ( empty_vao ) { glDeleteVertexArrays( 1, &empty_vao ); empty_vao = 0; }
    if ( vao ) { glDeleteVertexArrays( 1, &vao ); vao = 0; }
    if ( vbo ) { glDeleteBuffers( 1, &vbo ); vbo = 0; }
    if ( ebo ) { glDeleteBuffers( 1, &ebo ); ebo = 0; }
    vbo_capacity = 0;
    ebo_capacity = 0;
    shaders_ready = false;
}

void c_opengl_grayscale::resize( int w, int h )
{
    if ( w == width && h == height )
        return;
    width = w;
    height = h;
    release_fbos( );
}

void c_opengl_grayscale::apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                                 float strength )
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
    payload->cmd.strength = strength;

    dl->AddCallback( &c_opengl_grayscale::grayscale_callback, payload );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_opengl_grayscale::add_image( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
                                     float strength, ImU32 col )
{
    if ( !dl || !tex )
        return;

    int const vtx_start = dl->VtxBuffer.Size;
    int const idx_start = dl->IdxBuffer.Size;
    dl->AddImage( tex, p_min, p_max, { 0, 0 }, { 1, 1 }, col );
    int const vtx_end = dl->VtxBuffer.Size;
    int const idx_end = dl->IdxBuffer.Size;

    if ( vtx_end > vtx_start && idx_end > idx_start )
        apply( dl, vtx_start, vtx_end, idx_start, idx_end, strength );
}

void c_opengl_grayscale::add_image_rounded( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
                                             float rounding, ImDrawFlags flags,
                                             float strength, ImU32 col )
{
    if ( !dl || !tex )
        return;

    int const vtx_start = dl->VtxBuffer.Size;
    int const idx_start = dl->IdxBuffer.Size;
    dl->AddImageRounded( tex, p_min, p_max, { 0, 0 }, { 1, 1 }, col, rounding, flags );
    int const vtx_end = dl->VtxBuffer.Size;
    int const idx_end = dl->IdxBuffer.Size;

    if ( vtx_end > vtx_start && idx_end > idx_start )
        apply( dl, vtx_start, vtx_end, idx_start, idx_end, strength );
}

ImVec4 c_opengl_grayscale::intersect_clips( ImVec4 const& a, ImVec4 const& b )
{
    return { std::max( a.x, b.x ), std::max( a.y, b.y ),
             std::min( a.z, b.z ), std::min( a.w, b.w ) };
}

bool c_opengl_grayscale::create_shaders( )
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
    GLuint fs_gs = compile_shader( GL_FRAGMENT_SHADER, k_grayscale_fs );
    if ( !vs_fs || !fs_gs )
        return false;
    prog_grayscale = link_program( vs_fs, fs_gs );
    glDeleteShader( vs_fs );
    glDeleteShader( fs_gs );
    if ( !prog_grayscale )
        return false;
    glUseProgram( prog_grayscale );
    glUniform1i( glGetUniformLocation( prog_grayscale, "u_tex" ), 0 );
    loc_gs_strength = glGetUniformLocation( prog_grayscale, "u_strength" );

    glUseProgram( 0 );
    return true;
}

bool c_opengl_grayscale::create_fbo( unsigned int& out_fbo, unsigned int& out_tex, int w, int h )
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

void c_opengl_grayscale::release_fbos( )
{
    if ( fbo_src ) { glDeleteFramebuffers( 1, &fbo_src ); fbo_src = 0; }
    if ( tex_src ) { glDeleteTextures( 1, &tex_src ); tex_src = 0; }
    fbo_w = 0;
    fbo_h = 0;
}

bool c_opengl_grayscale::ensure_fbos( int w, int h )
{
    if ( w == fbo_w && h == fbo_h && tex_src )
        return true;
    release_fbos( );
    fbo_w = w;
    fbo_h = h;
    return create_fbo( fbo_src, tex_src, w, h );
}

void c_opengl_grayscale::ensure_buffers( int vtx_count, int idx_count )
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

void c_opengl_grayscale::blit_to_fbo( gs_cmd_t const& cmd, ImDrawList* dl, unsigned int dst_fbo, int w, int h, bool clear )
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
                int const scy_top = static_cast<int>(std::max( 0.f, clip.y ));
                int const scx2 = static_cast<int>(std::min( static_cast<float>(w), clip.z ));
                int const scy2 = static_cast<int>(std::min( static_cast<float>(h), clip.w ));
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

void c_opengl_grayscale::composite_pass( unsigned int src_tex, gs_cmd_t const& cmd, int w, int h )
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
    glUseProgram( prog_grayscale );
    glUniform1f( loc_gs_strength, std::clamp( cmd.strength, 0.f, 1.f ) );
    glActiveTexture( GL_TEXTURE0 );
    glBindTexture( GL_TEXTURE_2D, src_tex );
    glDrawArrays( GL_TRIANGLES, 0, 3 );
    glBindVertexArray( 0 );
}

void c_opengl_grayscale::render_grayscale( gs_cmd_t const& cmd, ImDrawList* dl )
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
    composite_pass( tex_src, cmd, w, h );
}

void c_opengl_grayscale::grayscale_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer || !data->draw_list )
        return;

    auto* r = data->renderer;
    auto* dl = data->draw_list;
    gs_cmd_t cmd = data->cmd;
    cmd.clip_rect = draw_cmd->ClipRect;

    r->save_state( );
    r->render_grayscale( cmd, dl );
    r->restore_state( );

    for ( int i = cmd.vtx_start; i < cmd.vtx_end; ++i )
        dl->VtxBuffer.Data[ i ].col &= 0x00FFFFFF;

    delete data;
}

void c_opengl_grayscale::save_state( )
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

void c_opengl_grayscale::restore_state( )
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
