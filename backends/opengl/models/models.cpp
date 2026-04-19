#include "models.hpp"
#include "model_engine/skeleton.hpp"
#include "model_engine/formats/loader.hpp"
#include "logger/logger.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <utility>

#include "stb_image.hpp"

#ifdef __APPLE__
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#endif

using alice::matrix4x4_t;
using alice::c_ogl_model;
using alice::c_ogl_model_engine;
using alice::c_skeleton;

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

static constexpr char const* k_vs_src = R"glsl(
#version 150

in vec3 a_position;
in vec3 a_normal;
in vec2 a_texcoord;
in uvec4 a_joints;
in vec4 a_weights;

uniform mat4 u_wvp;
uniform mat4 u_world_rot;
uniform samplerBuffer u_bones;

out vec2 v_uv;
out vec3 v_normal_world;
out vec3 v_normal_view;

mat4 fetch_bone( uint i )
{
    int base = int( i ) * 4;
    return mat4(
        texelFetch( u_bones, base + 0 ),
        texelFetch( u_bones, base + 1 ),
        texelFetch( u_bones, base + 2 ),
        texelFetch( u_bones, base + 3 )
    );
}

void main( )
{
    mat4 b0 = fetch_bone( a_joints.x );
    mat4 b1 = fetch_bone( a_joints.y );
    mat4 b2 = fetch_bone( a_joints.z );
    mat4 b3 = fetch_bone( a_joints.w );

    vec4 p = vec4( a_position, 1.0 );
    vec4 skinned =
        b0 * p * a_weights.x +
        b1 * p * a_weights.y +
        b2 * p * a_weights.z +
        b3 * p * a_weights.w;

    vec3 n = a_normal;
    vec3 skinned_n =
        mat3( b0 ) * n * a_weights.x +
        mat3( b1 ) * n * a_weights.y +
        mat3( b2 ) * n * a_weights.z +
        mat3( b3 ) * n * a_weights.w;

    gl_Position = u_wvp * vec4( skinned.xyz, 1.0 );
    v_uv = a_texcoord;
    v_normal_world = mat3( u_world_rot ) * skinned_n;
    v_normal_view = normalize( mat3( u_world_rot ) * skinned_n );
}
)glsl";

static constexpr char const* k_fs_src = R"glsl(
#version 150

uniform sampler2D u_texture;
uniform sampler2D u_sphere_tex;
uniform sampler2D u_toon_tex;
uniform vec3 u_light_dir;
uniform vec3 u_view_dir;
uniform float u_ambient;
uniform float u_rim_strength;
uniform vec4 u_diffuse_color;
uniform int u_sphere_mode;
uniform int u_has_toon;

in vec2 v_uv;
in vec3 v_normal_world;
in vec3 v_normal_view;
out vec4 frag_color;

void main( )
{
    vec4 tex = texture( u_texture, v_uv );
    if ( tex.a < 0.1 ) discard;

    vec3 base_rgb = tex.rgb * u_diffuse_color.rgb;
    float out_alpha = tex.a;

    vec3 n = normalize( v_normal_world );
    vec3 l = normalize( u_light_dir );
    float ndotl = dot( n, l );

    vec3 shaded;
    if ( u_has_toon == 1 )
    {
        float ramp_u = clamp( ndotl * 0.5 + 0.5, 0.0, 1.0 );
        vec3 toon = texture( u_toon_tex, vec2( 0.5, ramp_u ) ).rgb;
        shaded = base_rgb * toon;
    }
    else
    {
        float half_lambert = ndotl * 0.5 + 0.5;
        float fill = max( 0.0, dot( n, -l ) ) * 0.25;
        float shade = min( u_ambient + ( 1.0 - u_ambient ) * half_lambert + fill, 1.15 );
        shaded = base_rgb * shade;
    }

    if ( u_sphere_mode > 0 )
    {
        vec2 sph_uv = v_normal_view.xy * 0.5 + 0.5;
        vec3 sph = texture( u_sphere_tex, sph_uv ).rgb;
        if ( u_sphere_mode == 1 )
            shaded *= sph;
        else if ( u_sphere_mode == 2 )
            shaded += sph;
    }

    float rim = 1.0 - max( 0.0, dot( n, normalize( u_view_dir ) ) );
    rim = rim * rim * rim * u_rim_strength;

    vec3 lit = shaded + vec3( rim );
    vec3 srgb = pow( max( lit, vec3( 0.0 ) ), vec3( 1.0 / 2.2 ) );

    frag_color = vec4( srgb, out_alpha );
}
)glsl";

c_ogl_model_engine::c_ogl_model_engine( ) = default;

c_ogl_model_engine::~c_ogl_model_engine( )
{
    clear( );
    if ( program ) glDeleteProgram( program );
    if ( white_tex ) glDeleteTextures( 1, &white_tex );
    if ( default_toon ) glDeleteTextures( 1, &default_toon );
    for ( auto& t : shared_toon )
        if ( t ) glDeleteTextures( 1, &t );
}

std::unique_ptr< alice::c_model > c_ogl_model_engine::make_model( )
{
    init_pipeline( );
    return std::make_unique< c_ogl_model >( this );
}

bool c_ogl_model_engine::init_pipeline( )
{
    if ( pipeline_ready )
        return true;

    GLuint vs = compile_shader( GL_VERTEX_SHADER, k_vs_src );
    if ( !vs )
        return false;

    GLuint fs = compile_shader( GL_FRAGMENT_SHADER, k_fs_src );
    if ( !fs )
    {
        glDeleteShader( vs );
        return false;
    }

    program = glCreateProgram( );
    glAttachShader( program, vs );
    glAttachShader( program, fs );

    glBindAttribLocation( program, 0, "a_position" );
    glBindAttribLocation( program, 1, "a_normal" );
    glBindAttribLocation( program, 2, "a_texcoord" );
    glBindAttribLocation( program, 3, "a_joints" );
    glBindAttribLocation( program, 4, "a_weights" );

    glLinkProgram( program );
    glDeleteShader( vs );
    glDeleteShader( fs );

    GLint link_ok = 0;
    glGetProgramiv( program, GL_LINK_STATUS, &link_ok );
    if ( !link_ok )
    {
        glDeleteProgram( program );
        program = 0;
        return false;
    }

    loc_wvp = glGetUniformLocation( program, "u_wvp" );
    loc_world_rot = glGetUniformLocation( program, "u_world_rot" );
    loc_light_dir = glGetUniformLocation( program, "u_light_dir" );
    loc_view_dir = glGetUniformLocation( program, "u_view_dir" );
    loc_ambient = glGetUniformLocation( program, "u_ambient" );
    loc_rim_strength = glGetUniformLocation( program, "u_rim_strength" );
    loc_diffuse_color = glGetUniformLocation( program, "u_diffuse_color" );
    loc_sphere_mode = glGetUniformLocation( program, "u_sphere_mode" );
    loc_has_toon = glGetUniformLocation( program, "u_has_toon" );

    glUseProgram( program );
    glUniform1i( glGetUniformLocation( program, "u_texture" ), 0 );
    glUniform1i( glGetUniformLocation( program, "u_bones" ), 1 );
    glUniform1i( glGetUniformLocation( program, "u_sphere_tex" ), 2 );
    glUniform1i( glGetUniformLocation( program, "u_toon_tex" ), 3 );
    glUseProgram( 0 );

    uint32_t const white_pixel = 0xFFFFFFFF;
    glGenTextures( 1, &white_tex );
    glBindTexture( GL_TEXTURE_2D, white_tex );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white_pixel );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );

    auto make_toon = []( std::array< uint8_t, 32 * 4 > const& pixels ) {
        unsigned int tex = 0;
        glGenTextures( 1, &tex );
        glBindTexture( GL_TEXTURE_2D, tex );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, 1, 32, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data( ) );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
        return tex;
    };

    auto build_ramp = []( float shadow_min, float shadow_max, float edge ) {
        std::array< uint8_t, 32 * 4 > pix { };
        for ( int i = 0; i < 32; ++i )
        {
            float const t = static_cast< float >( i ) / 31.f;
            float const s = std::clamp( ( t - ( 1.f - edge ) ) / edge, 0.f, 1.f );
            float const v = shadow_min + ( shadow_max - shadow_min ) * s;
            uint8_t const b = static_cast< uint8_t >( std::clamp( v * 255.f, 0.f, 255.f ) );
            pix[ i * 4 + 0 ] = b;
            pix[ i * 4 + 1 ] = b;
            pix[ i * 4 + 2 ] = b;
            pix[ i * 4 + 3 ] = 255;
        }
        return pix;
    };

    default_toon = make_toon( build_ramp( 0.6f, 1.0f, 0.35f ) );
    shared_toon[ 0 ] = make_toon( build_ramp( 0.55f, 1.0f, 0.30f ) );
    shared_toon[ 1 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.20f ) );
    shared_toon[ 2 ] = make_toon( build_ramp( 0.45f, 1.0f, 0.15f ) );
    shared_toon[ 3 ] = make_toon( build_ramp( 0.65f, 1.0f, 0.35f ) );
    shared_toon[ 4 ] = make_toon( build_ramp( 0.70f, 1.0f, 0.40f ) );
    shared_toon[ 5 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.25f ) );
    shared_toon[ 6 ] = make_toon( build_ramp( 0.55f, 1.0f, 0.30f ) );
    shared_toon[ 7 ] = make_toon( build_ramp( 0.60f, 1.0f, 0.35f ) );
    shared_toon[ 8 ] = make_toon( build_ramp( 0.65f, 1.0f, 0.40f ) );
    shared_toon[ 9 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.10f ) );

    pipeline_ready = true;
    return true;
}

c_ogl_model::c_ogl_model( c_ogl_model_engine* owner )
    : engine( owner )
{
}

c_ogl_model::~c_ogl_model( )
{
    unload( );
    if ( fbo ) glDeleteFramebuffers( 1, &fbo );
    if ( rtt_tex ) glDeleteTextures( 1, &rtt_tex );
    if ( depth_rbo ) glDeleteRenderbuffers( 1, &depth_rbo );
}

bool c_ogl_model::create_rtt( int w, int h )
{
    if ( w < 1 || h < 1 )
        return false;

    if ( fbo ) { glDeleteFramebuffers( 1, &fbo ); fbo = 0; }
    if ( rtt_tex ) { glDeleteTextures( 1, &rtt_tex ); rtt_tex = 0; }
    if ( depth_rbo ) { glDeleteRenderbuffers( 1, &depth_rbo ); depth_rbo = 0; }

    glGenTextures( 1, &rtt_tex );
    glBindTexture( GL_TEXTURE_2D, rtt_tex );
    glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glGenerateMipmap( GL_TEXTURE_2D );

    glGenRenderbuffers( 1, &depth_rbo );
    glBindRenderbuffer( GL_RENDERBUFFER, depth_rbo );
    glRenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h );

    glGenFramebuffers( 1, &fbo );
    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rtt_tex, 0 );
    glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth_rbo );

    bool const ok = glCheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer( GL_FRAMEBUFFER, 0 );

    rtt_width = w;
    rtt_height = h;
    return ok;
}

bool c_ogl_model::load( std::string const& path )
{
    if ( !engine || !engine->init_pipeline( ) )
        return false;

    unload( );

    glb::scene_data_t scene;
    if ( !formats::parse( path, scene ) )
        return false;

    std::vector< GLuint > loaded_textures( scene.images.size( ), 0 );
    for ( std::size_t i = 0; i < scene.images.size( ); ++i )
    {
        auto const& img = scene.images[ i ];
        if ( img.pixels.empty( ) )
            continue;

        int tw = 0, th = 0, tc = 0;
        auto* pixels = stbi_load_from_memory( img.pixels.data( ), static_cast< int >( img.pixels.size( ) ), &tw, &th, &tc, 4 );
        if ( !pixels )
            continue;

        GLuint tex = 0;
        glGenTextures( 1, &tex );
        glBindTexture( GL_TEXTURE_2D, tex );
        glTexImage2D( GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

        stbi_image_free( pixels );
        loaded_textures[ i ] = tex;
    }

    float min_y = 1e10f, max_y = -1e10f;

    for ( auto const& md : scene.meshes )
    {
        int const vert_count = static_cast< int >( md.positions.size( ) / 3 );
        if ( vert_count == 0 )
            continue;

        std::vector< skinned_vertex_t > vertices( vert_count );
        for ( int vi = 0; vi < vert_count; ++vi )
        {
            auto& v = vertices[ vi ];
            v.x = md.positions[ vi * 3 ];
            v.y = md.positions[ vi * 3 + 1 ];
            v.z = md.positions[ vi * 3 + 2 ];

            min_y = ( std::min )( min_y, v.y );
            max_y = ( std::max )( max_y, v.y );

            if ( static_cast< std::size_t >( vi * 3 + 2 ) < md.normals.size( ) )
            {
                v.nx = md.normals[ vi * 3 ];
                v.ny = md.normals[ vi * 3 + 1 ];
                v.nz = md.normals[ vi * 3 + 2 ];
            }
            else
            {
                v.nx = 0.f;
                v.ny = 1.f;
                v.nz = 0.f;
            }

            if ( static_cast< std::size_t >( vi * 2 + 1 ) < md.texcoords.size( ) )
            {
                v.u = md.texcoords[ vi * 2 ];
                v.v = md.texcoords[ vi * 2 + 1 ];
            }

            if ( static_cast< std::size_t >( vi * 4 + 3 ) < md.joints.size( ) )
            {
                v.joints[ 0 ] = md.joints[ vi * 4 ];
                v.joints[ 1 ] = md.joints[ vi * 4 + 1 ];
                v.joints[ 2 ] = md.joints[ vi * 4 + 2 ];
                v.joints[ 3 ] = md.joints[ vi * 4 + 3 ];
            }

            if ( static_cast< std::size_t >( vi * 4 + 3 ) < md.weights.size( ) )
            {
                v.weights[ 0 ] = md.weights[ vi * 4 ];
                v.weights[ 1 ] = md.weights[ vi * 4 + 1 ];
                v.weights[ 2 ] = md.weights[ vi * 4 + 2 ];
                v.weights[ 3 ] = md.weights[ vi * 4 + 3 ];
            }
            else
            {
                v.weights[ 0 ] = 1.f;
                v.weights[ 1 ] = 0.f;
                v.weights[ 2 ] = 0.f;
                v.weights[ 3 ] = 0.f;
            }
        }

        mesh_group_t group { };

        glGenVertexArrays( 1, &group.vao );
        glBindVertexArray( group.vao );

        glGenBuffers( 1, &group.vbo );
        glBindBuffer( GL_ARRAY_BUFFER, group.vbo );
        glBufferData( GL_ARRAY_BUFFER,
                      static_cast< GLsizeiptr >( vertices.size( ) * sizeof( skinned_vertex_t ) ),
                      vertices.data( ), GL_STATIC_DRAW );

        GLsizei const stride = sizeof( skinned_vertex_t );

        glEnableVertexAttribArray( 0 );
        glVertexAttribPointer( 0, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast< void const* >( offsetof( skinned_vertex_t, x ) ) );

        glEnableVertexAttribArray( 1 );
        glVertexAttribPointer( 1, 3, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast< void const* >( offsetof( skinned_vertex_t, nx ) ) );

        glEnableVertexAttribArray( 2 );
        glVertexAttribPointer( 2, 2, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast< void const* >( offsetof( skinned_vertex_t, u ) ) );

        glEnableVertexAttribArray( 3 );
        glVertexAttribIPointer( 3, 4, GL_UNSIGNED_SHORT, stride,
                                reinterpret_cast< void const* >( offsetof( skinned_vertex_t, joints ) ) );

        glEnableVertexAttribArray( 4 );
        glVertexAttribPointer( 4, 4, GL_FLOAT, GL_FALSE, stride,
                               reinterpret_cast< void const* >( offsetof( skinned_vertex_t, weights ) ) );

        if ( !md.indices.empty( ) )
        {
            glGenBuffers( 1, &group.ebo );
            glBindBuffer( GL_ELEMENT_ARRAY_BUFFER, group.ebo );
            glBufferData( GL_ELEMENT_ARRAY_BUFFER,
                          static_cast< GLsizeiptr >( md.indices.size( ) * sizeof( uint32_t ) ),
                          md.indices.data( ), GL_STATIC_DRAW );
            group.index_count = static_cast< int >( md.indices.size( ) );
        }

        glBindVertexArray( 0 );

        if ( md.material_index >= 0 && md.material_index < static_cast< int >( scene.materials.size( ) ) )
        {
            auto const& mat = scene.materials[ md.material_index ];

            auto resolve = [ & ]( int tex_idx ) -> unsigned int {
                if ( tex_idx < 0 )
                    return 0;
                int img_idx = tex_idx;
                if ( tex_idx < static_cast< int >( scene.texture_to_image.size( ) ) )
                    img_idx = scene.texture_to_image[ tex_idx ];
                if ( img_idx >= 0 && img_idx < static_cast< int >( loaded_textures.size( ) ) )
                    return loaded_textures[ img_idx ];
                return 0;
            };

            group.texture = resolve( mat.texture_index );
            group.sphere_tex = resolve( mat.sphere_texture_index );
            group.sphere_mode = mat.sphere_mode;

            if ( mat.shared_toon )
            {
                int const t = mat.toon_texture_index;
                group.toon_tex = ( t >= 0 && t < 10 ) ? engine->shared_toon[ t ] : engine->default_toon;
            }
            else if ( mat.toon_texture_index >= 0 )
            {
                auto const tex = resolve( mat.toon_texture_index );
                group.toon_tex = tex ? tex : engine->default_toon;
            }
            else
            {
                group.toon_tex = 0;
            }

            group.diffuse_rgba[ 0 ] = mat.diffuse_rgba[ 0 ];
            group.diffuse_rgba[ 1 ] = mat.diffuse_rgba[ 1 ];
            group.diffuse_rgba[ 2 ] = mat.diffuse_rgba[ 2 ];
            group.diffuse_rgba[ 3 ] = mat.diffuse_rgba[ 3 ];
        }
        else
        {
            group.toon_tex = 0;
        }

        mesh_groups.push_back( group );
        base_vertices.push_back( vertices );
        current_vertices.push_back( std::move( vertices ) );
    }

    if ( !scene.morphs.empty( ) && !scene.meshes.empty( ) )
    {
        std::unordered_multimap< std::uint32_t, std::pair< unsigned int, unsigned int > > src_map;
        for ( std::size_t mg = 0; mg < scene.meshes.size( ); ++mg )
        {
            auto const& md = scene.meshes[ mg ];
            for ( std::size_t li = 0; li < md.src_vertex_indices.size( ); ++li )
                src_map.emplace( md.src_vertex_indices[ li ],
                                 std::pair{ static_cast< unsigned int >( mg ), static_cast< unsigned int >( li ) } );
        }

        morphs.reserve( scene.morphs.size( ) );
        for ( auto const& mph : scene.morphs )
        {
            morph_info_t info;
            info.name = mph.name;
            info.applies.reserve( mph.vertex_offsets.size( ) * 2 );
            for ( auto const& off : mph.vertex_offsets )
            {
                auto range = src_map.equal_range( off.vertex_index );
                for ( auto it = range.first; it != range.second; ++it )
                {
                    info.applies.push_back( {
                        it->second.first, it->second.second,
                        off.dx, off.dy, off.dz
                    } );
                }
            }
            morphs.push_back( std::move( info ) );
        }

        log_info( "loaded {} morphs", morphs.size( ) );
    }

    center_y = ( min_y + max_y ) / 2.f;
    model_height = max_y - min_y;
    if ( model_height < 0.001f )
        model_height = 1.f;

    if ( !scene.skin.joints.empty( ) )
    {
        skel = std::make_unique< c_skeleton >( );
        skel->build( scene.skin );
        skel->set_physics_bones( scene.physics_bones );
        skel->set_colliders( scene.colliders );

        int const bone_count = static_cast< int >( scene.skin.joints.size( ) );

        glGenBuffers( 1, &bone_buffer );
        glBindBuffer( GL_TEXTURE_BUFFER, bone_buffer );
        glBufferData( GL_TEXTURE_BUFFER,
                      static_cast< GLsizeiptr >( bone_count * sizeof( matrix4x4_t ) ),
                      nullptr, GL_DYNAMIC_DRAW );
        glBindBuffer( GL_TEXTURE_BUFFER, 0 );

        glGenTextures( 1, &bone_tex );
        glBindTexture( GL_TEXTURE_BUFFER, bone_tex );
        glTexBuffer( GL_TEXTURE_BUFFER, GL_RGBA32F, bone_buffer );
        glBindTexture( GL_TEXTURE_BUFFER, 0 );
    }

    std::vector< bool > tex_used( loaded_textures.size( ), false );
    for ( auto const& g : mesh_groups )
        for ( std::size_t i = 0; i < loaded_textures.size( ); ++i )
            if ( ( g.texture == loaded_textures[ i ] || g.sphere_tex == loaded_textures[ i ] || g.toon_tex == loaded_textures[ i ] ) && loaded_textures[ i ] != 0 )
                tex_used[ i ] = true;

    for ( std::size_t i = 0; i < loaded_textures.size( ); ++i )
        if ( loaded_textures[ i ] && !tex_used[ i ] )
            glDeleteTextures( 1, &loaded_textures[ i ] );

    log_info( "loaded '{}': {} mesh groups, {} textures, {} bones, {} morphs, {} physics bones, {} colliders",
              path, mesh_groups.size( ), loaded_textures.size( ),
              skel ? skel->joint_count( ) : 0, morphs.size( ), scene.physics_bones.size( ), scene.colliders.size( ) );

    return !mesh_groups.empty( );
}

void c_ogl_model::unload( )
{
    for ( auto& g : mesh_groups )
    {
        if ( g.vao ) glDeleteVertexArrays( 1, &g.vao );
        if ( g.vbo ) glDeleteBuffers( 1, &g.vbo );
        if ( g.ebo ) glDeleteBuffers( 1, &g.ebo );
        if ( g.texture ) glDeleteTextures( 1, &g.texture );
    }
    mesh_groups.clear( );
    base_vertices.clear( );
    current_vertices.clear( );
    morphs.clear( );

    if ( bone_tex ) { glDeleteTextures( 1, &bone_tex ); bone_tex = 0; }
    if ( bone_buffer ) { glDeleteBuffers( 1, &bone_buffer ); bone_buffer = 0; }
    skel.reset( );
    anim.reset( );
}

void c_ogl_model::render( int width, int height )
{
    if ( mesh_groups.empty( ) || !engine || !engine->pipeline_ready )
        return;

    if ( width != rtt_width || height != rtt_height )
        create_rtt( width, height );

    if ( !fbo )
        return;

    if ( !morphs.empty( ) && anim )
    {
        auto const& weights = anim->morph_weights( );

        std::vector< bool > group_dirty( mesh_groups.size( ), false );
        bool any_changed = false;
        for ( auto& m : morphs )
        {
            auto it = weights.find( m.name );
            float const target = ( it == weights.end( ) ) ? 0.f : it->second;
            if ( std::fabs( target - m.applied_weight ) > 0.001f )
            {
                any_changed = true;
                for ( auto const& a : m.applies )
                    group_dirty[ a.mesh_group_idx ] = true;
                m.applied_weight = target;
            }
        }

        if ( any_changed )
        {
            for ( std::size_t mg = 0; mg < mesh_groups.size( ); ++mg )
            {
                if ( !group_dirty[ mg ] )
                    continue;
                auto& curr = current_vertices[ mg ];
                auto const& base = base_vertices[ mg ];
                std::memcpy( curr.data( ), base.data( ), base.size( ) * sizeof( skinned_vertex_t ) );
            }

            for ( auto const& m : morphs )
            {
                if ( m.applied_weight <= 0.001f )
                    continue;
                float const w = m.applied_weight;
                for ( auto const& a : m.applies )
                {
                    auto& v = current_vertices[ a.mesh_group_idx ][ a.local_vertex_idx ];
                    v.x += a.dx * w;
                    v.y += a.dy * w;
                    v.z += a.dz * w;
                }
            }

            for ( std::size_t mg = 0; mg < mesh_groups.size( ); ++mg )
            {
                if ( !group_dirty[ mg ] || !mesh_groups[ mg ].vbo )
                    continue;
                auto const& curr = current_vertices[ mg ];
                glBindBuffer( GL_ARRAY_BUFFER, mesh_groups[ mg ].vbo );
                glBufferSubData( GL_ARRAY_BUFFER, 0,
                                 static_cast< GLsizeiptr >( curr.size( ) * sizeof( skinned_vertex_t ) ),
                                 curr.data( ) );
            }
        }
    }

    GLint prev_fbo = 0;
    glGetIntegerv( GL_FRAMEBUFFER_BINDING, &prev_fbo );

    glBindFramebuffer( GL_FRAMEBUFFER, fbo );
    glViewport( 0, 0, width, height );

    glClearColor( 0.f, 0.f, 0.f, 0.f );
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    auto const world_rot = matrix4x4_t::rotation_y( yaw )
                           * matrix4x4_t::rotation_x( pitch );

    auto const world = matrix4x4_t::translation( 0.f, -center_y, 0.f )
                       * world_rot
                       * matrix4x4_t::scaling( 2.5f / model_height );

    auto const view = matrix4x4_t::translation( 0.f, 0.f, 4.f / current_zoom );
    auto const proj = matrix4x4_t::perspective( 45.f, static_cast< float >( width ) / static_cast< float >( height ), 0.1f, 100.f );

    cached_wvp = world * view * proj;

    glUseProgram( engine->program );
    glUniformMatrix4fv( engine->loc_wvp, 1, GL_FALSE, &cached_wvp.m[ 0 ][ 0 ] );
    glUniformMatrix4fv( engine->loc_world_rot, 1, GL_FALSE, &world_rot.m[ 0 ][ 0 ] );

    float const light_len = std::sqrt( 0.3f * 0.3f + 0.7f * 0.7f + 1.0f * 1.0f );
    glUniform3f( engine->loc_light_dir, 0.3f / light_len, 0.7f / light_len, 1.0f / light_len );
    glUniform3f( engine->loc_view_dir, 0.f, 0.f, -1.f );
    glUniform1f( engine->loc_ambient, 0.7f );
    glUniform1f( engine->loc_rim_strength, 0.3f );

    if ( skel && bone_buffer )
    {
        auto const& skin_matrices = skel->compute_skinning_matrices_cached( );
        auto const bytes = static_cast< GLsizeiptr >( skin_matrices.size( ) * sizeof( matrix4x4_t ) );

        glBindBuffer( GL_TEXTURE_BUFFER, bone_buffer );

        if ( void* mapped = glMapBufferRange( GL_TEXTURE_BUFFER, 0, bytes,
                                              GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT ) )
        {
            std::memcpy( mapped, skin_matrices.data( ), static_cast< std::size_t >( bytes ) );
            glUnmapBuffer( GL_TEXTURE_BUFFER );
        }

        glActiveTexture( GL_TEXTURE1 );
        glBindTexture( GL_TEXTURE_BUFFER, bone_tex );
    }

    glEnable( GL_DEPTH_TEST );
    glDepthFunc( GL_LESS );
    glDepthMask( GL_TRUE );

    glEnable( GL_BLEND );
    glBlendFuncSeparate( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA );

    glDisable( GL_CULL_FACE );

    unsigned int last_diffuse_tex = 0;
    unsigned int last_sphere_tex = 0;
    unsigned int last_toon_tex = 0;
    float last_diffuse[ 4 ] = { -1.f, -1.f, -1.f, -1.f };
    int last_sphere_mode = -1;
    int last_has_toon = -1;

    for ( auto& g : mesh_groups )
    {
        if ( !g.vao )
            continue;

        unsigned int const diffuse_tex = g.texture ? g.texture : engine->white_tex;
        unsigned int const sphere_tex_id = g.sphere_tex ? g.sphere_tex : engine->white_tex;
        unsigned int const toon_tex_id = g.toon_tex ? g.toon_tex : engine->default_toon;
        int const sphere_mode = g.sphere_tex ? g.sphere_mode : 0;
        int const has_toon = g.toon_tex ? 1 : 0;

        if ( diffuse_tex != last_diffuse_tex )
        {
            glActiveTexture( GL_TEXTURE0 );
            glBindTexture( GL_TEXTURE_2D, diffuse_tex );
            last_diffuse_tex = diffuse_tex;
        }

        if ( sphere_tex_id != last_sphere_tex )
        {
            glActiveTexture( GL_TEXTURE2 );
            glBindTexture( GL_TEXTURE_2D, sphere_tex_id );
            last_sphere_tex = sphere_tex_id;
        }

        if ( toon_tex_id != last_toon_tex )
        {
            glActiveTexture( GL_TEXTURE3 );
            glBindTexture( GL_TEXTURE_2D, toon_tex_id );
            last_toon_tex = toon_tex_id;
        }

        if ( last_diffuse[ 0 ] != g.diffuse_rgba[ 0 ] || last_diffuse[ 1 ] != g.diffuse_rgba[ 1 ]
             || last_diffuse[ 2 ] != g.diffuse_rgba[ 2 ] || last_diffuse[ 3 ] != g.diffuse_rgba[ 3 ] )
        {
            glUniform4fv( engine->loc_diffuse_color, 1, g.diffuse_rgba );
            std::memcpy( last_diffuse, g.diffuse_rgba, sizeof( last_diffuse ) );
        }

        if ( sphere_mode != last_sphere_mode )
        {
            glUniform1i( engine->loc_sphere_mode, sphere_mode );
            last_sphere_mode = sphere_mode;
        }

        if ( has_toon != last_has_toon )
        {
            glUniform1i( engine->loc_has_toon, has_toon );
            last_has_toon = has_toon;
        }

        glBindVertexArray( g.vao );

        if ( g.ebo && g.index_count > 0 )
            glDrawElements( GL_TRIANGLES, g.index_count, GL_UNSIGNED_INT, nullptr );
    }

    glActiveTexture( GL_TEXTURE0 );

    glBindVertexArray( 0 );
    glUseProgram( 0 );
    glBindFramebuffer( GL_FRAMEBUFFER, prev_fbo );

    glBindTexture( GL_TEXTURE_2D, rtt_tex );
    glGenerateMipmap( GL_TEXTURE_2D );
    glBindTexture( GL_TEXTURE_2D, 0 );
}
