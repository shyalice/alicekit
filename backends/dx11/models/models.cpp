#include "models.hpp"

#include "model_engine/skeleton.hpp"
#include "model_engine/formats/loader.hpp"
#include "logger/logger.hpp"

#include <d3dcompiler.h>
#include <cmath>
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <array>
#include <unordered_map>
#include <utility>

#include "stb_image.hpp"

#include "imgui.h"

#pragma comment( lib, "d3dcompiler.lib" )

using alice::c_dx11_model;
using alice::c_dx11_model_engine;
using alice::c_skeleton;
using alice::matrix4x4_t;

c_dx11_model_engine::c_dx11_model_engine( ID3D11Device* dev, ID3D11DeviceContext* context )
    : device_( dev )
    , ctx_( context )
{
    init_pipeline( );
}

std::unique_ptr< alice::c_model > c_dx11_model_engine::make_model( )
{
    return std::make_unique< c_dx11_model >( this );
}

c_dx11_model_engine::~c_dx11_model_engine( )
{
    clear( );

    if ( vs ) vs->Release( );
    if ( ps ) ps->Release( );
    if ( input_layout ) input_layout->Release( );
    if ( cb_scene ) cb_scene->Release( );
    if ( cb_material ) cb_material->Release( );
    if ( cb_fx ) cb_fx->Release( );
    if ( sampler ) sampler->Release( );
    if ( sampler_clamp ) sampler_clamp->Release( );
    if ( white_srv ) white_srv->Release( );
    if ( default_toon_srv ) default_toon_srv->Release( );
    for ( auto*& s : shared_toon_srv )
        if ( s ) { s->Release( ); s = nullptr; }
    if ( rasterizer ) rasterizer->Release( );
    if ( depth_state ) depth_state->Release( );
    if ( depth_state_blend ) depth_state_blend->Release( );
    if ( blend_state ) blend_state->Release( );
}

bool c_dx11_model_engine::init_pipeline( )
{
    if ( pipeline_ready )
        return true;

    auto const* shader_code = R"(
        cbuffer cb_scene : register( b0 )
        {
            matrix wvp;
            matrix world_rot;
            float3 light_dir;
            float ambient;
            float3 view_dir;
            float rim_strength;
        };
        cbuffer cb_bones : register( b1 ) { row_major float4x4 bones[ 1024 ]; };
        cbuffer cb_material : register( b2 )
        {
            float4 diffuse_color;
            int sphere_mode;
            int has_toon;
            float alpha_cutoff;
            float _pad0;
        };
        cbuffer cb_fx : register( b3 )
        {
            int4   fx_mode_pack;
            float4 fx_main_color;
            float4 fx_rim_color;
            float4 fx_params;
        };

        struct vs_in
        {
            float3 pos      : POSITION;
            float3 nrm      : NORMAL;
            float2 uv       : TEXCOORD;
            uint4  joints   : BLENDINDICES;
            float4 weights  : BLENDWEIGHT;
        };

        struct ps_in
        {
            float4 pos     : SV_POSITION;
            float2 uv      : TEXCOORD0;
            float3 n_world : TEXCOORD1;
            float3 n_view  : TEXCOORD2;
        };

        Texture2D tex_diffuse : register( t0 );
        Texture2D tex_sphere  : register( t1 );
        Texture2D tex_toon    : register( t2 );
        SamplerState smp_wrap  : register( s0 );
        SamplerState smp_clamp : register( s1 );

        ps_in vs_main( vs_in i )
        {
            float4 p = float4( i.pos, 1.0f );
            float3 n = i.nrm;

            float4 skinned_p =
                mul( p, bones[ i.joints.x ] ) * i.weights.x +
                mul( p, bones[ i.joints.y ] ) * i.weights.y +
                mul( p, bones[ i.joints.z ] ) * i.weights.z +
                mul( p, bones[ i.joints.w ] ) * i.weights.w;

            float4 n4 = float4( n, 0.0f );
            float3 skinned_n =
                mul( n4, bones[ i.joints.x ] ).xyz * i.weights.x +
                mul( n4, bones[ i.joints.y ] ).xyz * i.weights.y +
                mul( n4, bones[ i.joints.z ] ).xyz * i.weights.z +
                mul( n4, bones[ i.joints.w ] ).xyz * i.weights.w;

            ps_in o;
            o.pos = mul( float4( skinned_p.xyz, 1.0f ), wvp );
            o.uv = i.uv;
            float4 sn4 = float4( skinned_n, 0.0f );
            o.n_world = mul( sn4, world_rot ).xyz;
            o.n_view = normalize( o.n_world );
            return o;
        }

        float3 hsv2rgb( float3 c )
        {
            float3 p = abs( frac( c.xxx + float3( 1.0, 2.0/3.0, 1.0/3.0 ) ) * 6.0 - 3.0 );
            return c.z * lerp( float3( 1, 1, 1 ), saturate( p - 1.0 ), c.y );
        }

        float4 apply_fx( int mode, float3 N, float3 L, float3 V, float T, float2 UV, float2 fc, float ndl, float3 mc, float ma, float3 rc, float ra, float rim_p )
        {
            if ( mode == 1 )
            {
                float3 col = mc * ( 0.3 + 0.7 * ndl );
                float rim = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 3.0 );
                col += rim * 0.15;
                return float4( col, ma );
            }
            if ( mode == 2 ) return float4( mc, ma );
            if ( mode == 3 || mode == 17 )
            {
                float ndv = max( 0.0, abs( dot( N, V ) ) );
                float fresnel = pow( 1.0 - ndv, max( 0.01, rim_p ) );
                float3 edge = mc * fresnel * 2.5;
                float3 body = mc * 0.06 * ndl;
                float3 H = normalize( L + V );
                float spec = pow( max( 0.0, dot( N, H ) ), 48.0 ) * fresnel * 0.6;
                return float4( edge + body + spec, ( fresnel * 0.9 + 0.03 ) * ma );
            }
            if ( mode == 4 )
            {
                float3 body = mc * ( 0.25 + 0.75 * ndl );
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), max( 0.01, rim_p ) );
                float3 rim = rc * fresnel * ra;
                float3 H = normalize( L + V );
                float spec = pow( max( 0.0, dot( N, H ) ), 24.0 ) * 0.2;
                return float4( body + rim + spec, ma );
            }
            if ( mode == 5 )
            {
                float scan = sin( fc.y * 1.5 + T * 8.0 ) * 0.5 + 0.5;
                scan = smoothstep( 0.25, 0.75, scan );
                scan = scan * 0.8 + ( sin( fc.y * 4.0 - T * 12.0 ) * 0.5 + 0.5 ) * 0.2;
                float flicker = 0.92 + 0.08 * sin( T * 37.0 ) * sin( T * 7.3 + 2.0 );
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.0 );
                float3 holo = mc;
                holo.r += fresnel * 0.25;
                holo.b += ( 1.0 - fresnel ) * 0.2;
                float3 col = holo * ( 0.4 + 0.6 * scan ) * flicker + fresnel * float3( 0.6, 0.8, 1.0 ) * 0.4;
                col += step( 0.97, sin( T * 53.0 + fc.y * 0.1 ) ) * 0.3;
                return float4( col, ( 0.25 + 0.75 * fresnel ) * scan * flicker * ma );
            }
            if ( mode == 6 )
            {
                float t = T * 0.3;
                float2 p = UV * 4.0 + N.xy * 0.5;
                float n1 = sin( p.x * 3.0 + t ) * cos( p.y * 2.5 - t * 0.7 );
                float n2 = sin( p.x * 5.0 - t * 1.3 + 2.0 ) * cos( p.y * 4.0 + t * 0.5 );
                float n3 = sin( ( p.x + p.y ) * 7.0 + t * 2.0 ) * 0.5;
                float n4 = sin( p.x * 11.0 + p.y * 9.0 - t * 1.7 ) * 0.3;
                float nebula = pow( ( n1 + n2 + n3 + n4 ) * 0.125 + 0.5, 1.3 );
                float stars = pow( max( 0.0, sin( p.x * 23.0 + t * 0.5 ) * sin( p.y * 19.0 - t * 0.3 ) ), 20.0 );
                stars += pow( max( 0.0, sin( p.x * 31.0 - t * 0.7 ) * sin( p.y * 29.0 + t * 0.4 ) ), 24.0 ) * 0.7;
                float3 space = lerp( mc, rc, nebula ) + stars * float3( 1.0, 0.95, 0.8 ) * 0.6;
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.5 );
                space += fresnel * rc * 0.4;
                space *= 0.55 + 0.45 * ndl;
                return float4( space, ma );
            }
            if ( mode == 7 )
            {
                float2 mcp = N.xy * 0.5 + 0.5;
                float chrome = sin( mcp.x * 6.283 ) * cos( mcp.y * 6.283 ) * 0.5 + 0.5;
                float chrome2 = sin( mcp.x * 12.566 + 1.0 ) * cos( mcp.y * 9.42 ) * 0.3 + 0.5;
                float3 H = normalize( L + V );
                float sp_sh = pow( max( 0.0, dot( N, H ) ), 80.0 ) * 2.0;
                float sp_br = pow( max( 0.0, dot( N, H ) ), 12.0 ) * 0.4;
                float3 H2 = normalize( float3( -L.x, L.y, -L.z ) + V );
                float sp_fl = pow( max( 0.0, dot( N, H2 ) ), 32.0 ) * 0.3;
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 3.5 );
                float3 col = mc * ( 0.15 + 0.85 * ndl );
                col = lerp( col, mc * 1.3, chrome * 0.35 + chrome2 * 0.15 );
                col += sp_sh + sp_br + sp_fl + fresnel * rc * 0.7;
                return float4( col, ma );
            }
            if ( mode == 8 )
            {
                float pulse = sin( T * 3.0 ) * 0.5 + 0.5;
                float pulse2 = sin( T * 3.0 + 1.5 ) * 0.5 + 0.5;
                float3 col = lerp( mc, rc, pulse ) * ( 0.3 + 0.7 * ndl );
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.5 );
                col += fresnel * rc * pulse2 * 0.7;
                col += ( sin( UV.y * 12.0 - T * 4.0 ) * 0.5 + 0.5 ) * rc * 0.1;
                col *= 0.82 + 0.18 * sin( T * 3.0 + 1.0 );
                return float4( col, ma );
            }
    )" R"(
            if ( mode == 9 )
            {
                float hue = frac( T * 0.15 + UV.y * 0.4 + UV.x * 0.2 + dot( N, V ) * 0.3 );
                float3 rgb = hsv2rgb( float3( hue, 0.9, 1.0 ) ) * ( 0.35 + 0.65 * ndl );
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.5 );
                rgb += fresnel * hsv2rgb( float3( frac( hue + 0.33 ), 0.7, 1.0 ) ) * 0.4;
                return float4( rgb, ma );
            }
            if ( mode == 10 )
            {
                float ndv = dot( N, V );
                float tf = abs( ndv );
                float hue = frac( tf * 2.5 + 0.6 );
                float3 film = hsv2rgb( float3( hue, 0.6 + 0.4 * ( 1.0 - tf ), 1.0 ) );
                float3 col = lerp( mc * ( 0.2 + 0.8 * ndl ), film, 0.6 + 0.4 * ( 1.0 - tf ) );
                float fresnel = pow( 1.0 - max( 0.0, abs( ndv ) ), 4.0 );
                col += fresnel * rc * 0.6;
                float3 H = normalize( L + V );
                col += pow( max( 0.0, dot( N, H ) ), 48.0 ) * 0.8 * hsv2rgb( float3( frac( hue + 0.15 ), 0.5, 1.0 ) );
                return float4( col, ma );
            }
            if ( mode == 11 )
            {
                float ndv = max( 0.0, abs( dot( N, V ) ) );
                float fresnel = pow( 1.0 - ndv, 2.0 );
                float3 core = mc * 1.5 * pow( ndv, 3.0 );
                float3 edge = rc * 2.0 * fresnel;
                float flicker = 0.95 + 0.05 * sin( T * 20.0 + UV.y * 10.0 );
                float3 col = ( core + edge ) * ( 0.5 + 0.5 * ndl ) * flicker;
                float3 H = normalize( L + V );
                col += pow( max( 0.0, dot( N, H ) ), 32.0 ) * lerp( mc, rc, 0.5 ) * 0.5;
                return float4( col, ( 0.5 + 0.5 * fresnel ) * ma );
            }
            if ( mode == 12 )
            {
                float3 bc = mc * ( 0.3 + 0.7 * ndl );
                float band = floor( fc.y / 3.0 );
                float seed = frac( sin( band * 43.7 + floor( T * 8.0 ) * 17.3 ) * 999.5 );
                float glitch_on = step( 0.92, seed );
                float strength = ( seed - 0.92 ) * 12.5;
                float3 col;
                col.r = bc.r + glitch_on * strength * 0.6;
                col.g = bc.g - glitch_on * strength * 0.2;
                col.b = bc.b + glitch_on * strength * 0.4;
                col *= frac( fc.y * 0.5 ) > 0.5 ? 0.92 : 1.0;
                col += step( 0.97, frac( sin( floor( T * 15.0 ) * 73.1 ) * 431.5 ) ) * mc * 0.6;
                float bs = frac( sin( floor( fc.y / 8.0 ) * 13.0 + floor( T * 5.0 ) * 7.0 ) * 777.0 );
                col = lerp( col, col.gbr, saturate( step( 0.96, bs ) * ( bs - 0.96 ) * 400.0 ) );
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.0 );
                col.r += fresnel * 0.25;
                col.b += fresnel * 0.15;
                return float4( floor( col * 24.0 + 0.5 ) / 24.0, ma );
            }
            if ( mode == 13 )
            {
                float2 flow = float2(
                    sin( N.x * 4.0 + T * 1.5 ) * cos( N.y * 3.0 - T ),
                    cos( N.x * 3.0 - T * 1.2 ) * sin( N.y * 4.0 + T * 0.8 )
                ) * 0.12;
                float2 mcp = N.xy * 0.5 + 0.5 + flow;
                float c1 = sin( mcp.x * 12.566 ) * cos( mcp.y * 9.42 ) * 0.5 + 0.5;
                float c2 = sin( mcp.x * 6.28 + 1.5 ) * cos( mcp.y * 12.566 - 0.7 ) * 0.5 + 0.5;
                float c3 = sin( ( mcp.x + mcp.y ) * 8.0 + T * 0.5 ) * 0.5 + 0.5;
                float c4 = sin( mcp.x * 18.0 - mcp.y * 14.0 + T * 0.3 ) * 0.5 + 0.5;
                float chrome = c1 * 0.3 + c2 * 0.3 + c3 * 0.2 + c4 * 0.2;
                float3 H = normalize( L + V );
                float sp1 = pow( max( 0.0, dot( N, H ) ), 128.0 ) * 3.0;
                float sp2 = pow( max( 0.0, dot( N, H ) ), 16.0 ) * 0.4;
                float3 H2 = normalize( float3( -L.x, L.y, -L.z ) + V );
                float sp_fl = pow( max( 0.0, dot( N, H2 ) ), 48.0 ) * 0.5;
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 3.5 );
                float3 col = lerp( mc, rc * 1.3, chrome ) * ( 0.2 + 0.8 * ndl );
                col += sp1 + sp2 + sp_fl + fresnel * rc * 0.9;
                return float4( col, ma );
            }
            if ( mode == 14 )
            {
                float ndv = max( 0.0, abs( dot( N, V ) ) );
                float fresnel = pow( 1.0 - ndv, 2.5 );
                float3 void_col = mc * 0.04 * pow( ndv, 2.5 );
                float2 p = UV * 8.0 + N.xy * 2.0;
                float s1 = sin( p.x * 2.0 + T * 3.0 + sin( p.y * 3.0 + T ) * 2.0 ) * 0.5 + 0.5;
                float s2 = sin( p.y * 2.5 - T * 2.5 + cos( p.x * 2.0 - T * 1.5 ) * 2.0 ) * 0.5 + 0.5;
                float energy = pow( ( s1 * s2 + ( sin( ( p.x - p.y ) * 3.0 + T * 4.0 ) * 0.5 + 0.5 ) * 0.3 ) * fresnel, 0.6 );
                float3 edge = rc * energy * 2.5 + fresnel * fresnel * rc * 2.0;
                float sp = pow( max( 0.0, sin( p.x * 17.0 + T * 7.0 ) * sin( p.y * 13.0 - T * 5.0 ) ), 20.0 );
                edge += sp * fresnel * float3( 1.0, 0.95, 0.9 ) * 3.0;
                edge *= sin( T * 2.0 ) * 0.15 + 0.85;
                return float4( void_col + edge, max( 0.08, max( fresnel * ma, energy * 0.9 ) ) );
            }
            if ( mode == 15 )
            {
                float3 bc = mc * ( 0.12 + 0.28 * ndl );
                float2 p = UV * 10.0 + N.xy * 3.0;
                float a1 = pow( 1.0 - abs( sin( p.x * 8.0 + T * 7.0 + sin( p.y * 5.0 + T * 3.0 ) * 3.0 ) ), 10.0 );
                float a2 = pow( 1.0 - abs( sin( p.y * 7.0 - T * 6.0 + cos( p.x * 4.0 - T * 2.5 ) * 2.5 ) ), 12.0 );
                float a3 = pow( 1.0 - abs( sin( ( p.x + p.y ) * 5.0 + T * 9.0 + sin( T * 4.0 ) * 5.0 ) ), 14.0 );
                float a4 = pow( 1.0 - abs( sin( ( p.x - p.y * 0.7 ) * 9.0 - T * 8.0 + cos( T * 2.0 + p.y ) * 3.0 ) ), 16.0 );
                float arcs = max( max( a1, a2 ), max( a3, a4 ) );
                float burst = step( 0.93, frac( sin( floor( T * 8.0 ) * 91.7 ) * 437.3 ) ) * ( 0.5 + 0.5 * sin( T * 50.0 ) );
                arcs = max( arcs, burst * 0.4 );
                float3 arc_col = rc * arcs * 3.0 + arcs * arcs * 0.5;
                float fresnel = pow( 1.0 - max( 0.0, abs( dot( N, V ) ) ), 2.0 );
                arc_col += fresnel * rc * 0.5;
                arc_col += pow( max( 0.0, sin( p.x * 30.0 + T * 20.0 ) * sin( p.y * 25.0 - T * 15.0 ) ), 8.0 ) * fresnel * rc * 1.5;
                return float4( bc + arc_col, ma );
            }
            if ( mode == 16 )
            {
                float ndv = max( 0.0, abs( dot( N, V ) ) );
                float fresnel = pow( 1.0 - ndv, 4.0 );
                float3 col = lerp( mc * 0.25 * ndv, rc, fresnel );
                float3 H = normalize( L + V );
                float sp1 = pow( max( 0.0, dot( N, H ) ), 256.0 ) * 3.5;
                float sp2 = pow( max( 0.0, dot( N, H ) ), 32.0 ) * 0.25;
                float3 L2 = normalize( float3( -0.5, 0.8, -0.3 ) );
                float3 H2 = normalize( L2 + V );
                float sp3 = pow( max( 0.0, dot( N, H2 ) ), 160.0 ) * 1.2;
                col += ( sp1 + sp2 + sp3 ) * float3( 1.0, 0.97, 0.93 );
                col += ( sin( N.x * 8.0 + T * 1.2 ) * sin( N.y * 6.0 - T * 0.8 ) * 0.5 + 0.5 ) * rc * 0.06;
                col += ( N.y * 0.5 + 0.5 ) * rc * fresnel * 0.25;
                return float4( col, ( 0.06 + fresnel * 0.75 + ( sp1 + sp3 ) * 0.25 ) * ma );
            }
            return float4( mc, ma );
        }

        float4 ps_main( ps_in i ) : SV_Target
        {
            int fx_mode = fx_mode_pack.x;

            if ( fx_mode == 0 )
            {
                float4 t = tex_diffuse.Sample( smp_wrap, i.uv );
                if ( t.a < 0.1f ) discard;

                float3 base = t.rgb * diffuse_color.rgb;
                float alpha = t.a;

                float3 n = normalize( i.n_world );
                float3 l = normalize( light_dir );
                float ndotl = dot( n, l );

                float3 shaded;
                if ( has_toon == 1 )
                {
                    float ramp = clamp( ndotl * 0.5f + 0.5f, 0.0f, 1.0f );
                    float3 toon = tex_toon.Sample( smp_clamp, float2( 0.5f, ramp ) ).rgb;
                    shaded = base * toon;
                }
                else
                {
                    float hl = ndotl * 0.5f + 0.5f;
                    float fill = max( 0.0f, dot( n, -l ) ) * 0.25f;
                    float shade = min( ambient + ( 1.0f - ambient ) * hl + fill, 1.15f );
                    shaded = base * shade;
                }

                if ( sphere_mode == 1 )
                {
                    float2 suv = i.n_view.xy * 0.5f + 0.5f;
                    shaded *= tex_sphere.Sample( smp_wrap, suv ).rgb;
                }
                else if ( sphere_mode == 2 )
                {
                    float2 suv = i.n_view.xy * 0.5f + 0.5f;
                    shaded += tex_sphere.Sample( smp_wrap, suv ).rgb;
                }

                float rim = 1.0f - max( 0.0f, dot( n, normalize( view_dir ) ) );
                rim = rim * rim * rim * rim_strength;

                float3 lit = shaded + float3( rim, rim, rim );
                float3 srgb = pow( max( lit, float3( 0.0f, 0.0f, 0.0f ) ), float3( 1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f ) );
                return float4( srgb, alpha );
            }

            float3 N = normalize( i.n_world );
            float3 L = normalize( light_dir );
            float3 V = normalize( view_dir );
            float T = fx_params.w;
            float ndl = dot( N, L ) * 0.5 + 0.5;
            return apply_fx( fx_mode, N, L, V, T, i.uv, i.pos.xy, ndl,
                             fx_main_color.rgb, fx_main_color.a, fx_rim_color.rgb, fx_rim_color.a, fx_params.x );
        }
    )";

    ID3DBlob* vs_blob = nullptr;
    ID3DBlob* ps_blob = nullptr;
    ID3DBlob* err_blob = nullptr;

    HRESULT hr = D3DCompile( shader_code, strlen( shader_code ), nullptr, nullptr, nullptr,
                             "vs_main", "vs_5_0", 0, 0, &vs_blob, &err_blob );
    if ( FAILED( hr ) )
    {
        if ( err_blob ) err_blob->Release( );
        return false;
    }

    hr = D3DCompile( shader_code, strlen( shader_code ), nullptr, nullptr, nullptr,
                     "ps_main", "ps_5_0", 0, 0, &ps_blob, &err_blob );
    if ( FAILED( hr ) )
    {
        vs_blob->Release( );
        if ( err_blob ) err_blob->Release( );
        return false;
    }

    device_->CreateVertexShader( vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), nullptr, &vs );
    device_->CreatePixelShader( ps_blob->GetBufferPointer( ), ps_blob->GetBufferSize( ), nullptr, &ps );

    D3D11_INPUT_ELEMENT_DESC layout_desc[] = {
        { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof( skinned_vertex_t, x ),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT,    0, offsetof( skinned_vertex_t, nx ),      D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,       0, offsetof( skinned_vertex_t, u ),       D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDINDICES", 0, DXGI_FORMAT_R16G16B16A16_UINT,  0, offsetof( skinned_vertex_t, joints ),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "BLENDWEIGHT",  0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof( skinned_vertex_t, weights ), D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };
    device_->CreateInputLayout( layout_desc, 5, vs_blob->GetBufferPointer( ), vs_blob->GetBufferSize( ), &input_layout );

    vs_blob->Release( );
    ps_blob->Release( );

    D3D11_BUFFER_DESC cb_desc { };
    cb_desc.ByteWidth = 160;
    cb_desc.Usage = D3D11_USAGE_DEFAULT;
    cb_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    device_->CreateBuffer( &cb_desc, nullptr, &cb_scene );

    cb_desc.ByteWidth = 32;
    device_->CreateBuffer( &cb_desc, nullptr, &cb_material );

    cb_desc.ByteWidth = 64;
    device_->CreateBuffer( &cb_desc, nullptr, &cb_fx );

    D3D11_SAMPLER_DESC samp_desc { };
    samp_desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    device_->CreateSamplerState( &samp_desc, &sampler );

    samp_desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samp_desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    device_->CreateSamplerState( &samp_desc, &sampler_clamp );

    uint32_t const white_pixel = 0xFFFFFFFFu;
    D3D11_TEXTURE2D_DESC white_desc { };
    white_desc.Width = 1;
    white_desc.Height = 1;
    white_desc.MipLevels = 1;
    white_desc.ArraySize = 1;
    white_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    white_desc.SampleDesc = { 1, 0 };
    white_desc.Usage = D3D11_USAGE_IMMUTABLE;
    white_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA white_data { };
    white_data.pSysMem = &white_pixel;
    white_data.SysMemPitch = 4;

    ID3D11Texture2D* white_tex = nullptr;
    if ( SUCCEEDED( device_->CreateTexture2D( &white_desc, &white_data, &white_tex ) ) && white_tex )
    {
        device_->CreateShaderResourceView( white_tex, nullptr, &white_srv );
        white_tex->Release( );
    }

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

    auto make_toon = [ this ]( std::array< uint8_t, 32 * 4 > const& pixels ) -> ID3D11ShaderResourceView* {
        D3D11_TEXTURE2D_DESC td { };
        td.Width = 1;
        td.Height = 32;
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc = { 1, 0 };
        td.Usage = D3D11_USAGE_IMMUTABLE;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA sd { };
        sd.pSysMem = pixels.data( );
        sd.SysMemPitch = 4;

        ID3D11Texture2D* tex = nullptr;
        ID3D11ShaderResourceView* srv = nullptr;
        if ( SUCCEEDED( device_->CreateTexture2D( &td, &sd, &tex ) ) && tex )
        {
            device_->CreateShaderResourceView( tex, nullptr, &srv );
            tex->Release( );
        }
        return srv;
    };

    default_toon_srv = make_toon( build_ramp( 0.6f, 1.0f, 0.35f ) );
    shared_toon_srv[ 0 ] = make_toon( build_ramp( 0.55f, 1.0f, 0.30f ) );
    shared_toon_srv[ 1 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.20f ) );
    shared_toon_srv[ 2 ] = make_toon( build_ramp( 0.45f, 1.0f, 0.15f ) );
    shared_toon_srv[ 3 ] = make_toon( build_ramp( 0.65f, 1.0f, 0.35f ) );
    shared_toon_srv[ 4 ] = make_toon( build_ramp( 0.70f, 1.0f, 0.40f ) );
    shared_toon_srv[ 5 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.25f ) );
    shared_toon_srv[ 6 ] = make_toon( build_ramp( 0.55f, 1.0f, 0.30f ) );
    shared_toon_srv[ 7 ] = make_toon( build_ramp( 0.60f, 1.0f, 0.35f ) );
    shared_toon_srv[ 8 ] = make_toon( build_ramp( 0.65f, 1.0f, 0.40f ) );
    shared_toon_srv[ 9 ] = make_toon( build_ramp( 0.50f, 1.0f, 0.10f ) );

    UINT quality_levels = 0;
    if ( SUCCEEDED( device_->CheckMultisampleQualityLevels( DXGI_FORMAT_R8G8B8A8_UNORM, k_msaa_samples, &quality_levels ) ) && quality_levels > 0 )
        msaa_quality = quality_levels - 1;
    else
        msaa_quality = 0;

    D3D11_RASTERIZER_DESC rast_desc { };
    rast_desc.FillMode = D3D11_FILL_SOLID;
    rast_desc.CullMode = D3D11_CULL_NONE;
    rast_desc.MultisampleEnable = TRUE;
    rast_desc.AntialiasedLineEnable = TRUE;
    device_->CreateRasterizerState( &rast_desc, &rasterizer );

    D3D11_DEPTH_STENCIL_DESC ds_desc { };
    ds_desc.DepthEnable = TRUE;
    ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    ds_desc.DepthFunc = D3D11_COMPARISON_LESS;
    device_->CreateDepthStencilState( &ds_desc, &depth_state );

    ds_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    device_->CreateDepthStencilState( &ds_desc, &depth_state_blend );

    D3D11_BLEND_DESC bl_desc { };
    bl_desc.RenderTarget[ 0 ].BlendEnable = TRUE;
    bl_desc.RenderTarget[ 0 ].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    bl_desc.RenderTarget[ 0 ].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    bl_desc.RenderTarget[ 0 ].BlendOp = D3D11_BLEND_OP_ADD;
    bl_desc.RenderTarget[ 0 ].SrcBlendAlpha = D3D11_BLEND_ONE;
    bl_desc.RenderTarget[ 0 ].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
    bl_desc.RenderTarget[ 0 ].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    bl_desc.RenderTarget[ 0 ].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    device_->CreateBlendState( &bl_desc, &blend_state );

    pipeline_ready = true;
    return true;
}

c_dx11_model::c_dx11_model( c_dx11_model_engine* owner )
    : engine( owner )
{
}

c_dx11_model::~c_dx11_model( )
{
    unload( );

    if ( rtv ) rtv->Release( );
    if ( msaa_tex ) msaa_tex->Release( );
    if ( dsv ) dsv->Release( );
    if ( rtt_srv ) rtt_srv->Release( );
    if ( resolve_tex ) resolve_tex->Release( );
}

bool c_dx11_model::create_rtt( int w, int h )
{
    if ( w < 1 || h < 1 || !engine )
        return false;

    if ( rtv ) { rtv->Release( ); rtv = nullptr; }
    if ( msaa_tex ) { msaa_tex->Release( ); msaa_tex = nullptr; }
    if ( dsv ) { dsv->Release( ); dsv = nullptr; }
    if ( rtt_srv ) { rtt_srv->Release( ); rtt_srv = nullptr; }
    if ( resolve_tex ) { resolve_tex->Release( ); resolve_tex = nullptr; }

    auto* dev = engine->device_;

    D3D11_TEXTURE2D_DESC msaa_desc { };
    msaa_desc.Width = static_cast< UINT >( w );
    msaa_desc.Height = static_cast< UINT >( h );
    msaa_desc.MipLevels = 1;
    msaa_desc.ArraySize = 1;
    msaa_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    msaa_desc.SampleDesc = { c_dx11_model_engine::k_msaa_samples, engine->msaa_quality };
    msaa_desc.Usage = D3D11_USAGE_DEFAULT;
    msaa_desc.BindFlags = D3D11_BIND_RENDER_TARGET;

    dev->CreateTexture2D( &msaa_desc, nullptr, &msaa_tex );
    dev->CreateRenderTargetView( msaa_tex, nullptr, &rtv );

    D3D11_TEXTURE2D_DESC resolve_desc = msaa_desc;
    resolve_desc.SampleDesc = { 1, 0 };
    resolve_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    dev->CreateTexture2D( &resolve_desc, nullptr, &resolve_tex );
    dev->CreateShaderResourceView( resolve_tex, nullptr, &rtt_srv );

    D3D11_TEXTURE2D_DESC ds_desc = msaa_desc;
    ds_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

    ID3D11Texture2D* ds = nullptr;
    dev->CreateTexture2D( &ds_desc, nullptr, &ds );
    dev->CreateDepthStencilView( ds, nullptr, &dsv );
    ds->Release( );

    rtt_width = w;
    rtt_height = h;
    return true;
}

bool c_dx11_model::load( std::string const& path )
{
    if ( !engine )
        return false;

    unload( );

    glb::scene_data_t scene;
    if ( !formats::parse( path, scene ) )
        return false;

    auto* dev = engine->device_;

    std::vector< ID3D11ShaderResourceView* > loaded_textures( scene.images.size( ), nullptr );
    for ( std::size_t i = 0; i < scene.images.size( ); ++i )
    {
        auto const& img = scene.images[ i ];
        if ( img.pixels.empty( ) )
            continue;

        int tw = 0, th = 0, tc = 0;
        auto* pixels = stbi_load_from_memory( img.pixels.data( ), static_cast< int >( img.pixels.size( ) ), &tw, &th, &tc, 4 );
        if ( !pixels )
            continue;

        D3D11_TEXTURE2D_DESC td { };
        td.Width = static_cast< UINT >( tw );
        td.Height = static_cast< UINT >( th );
        td.MipLevels = 1;
        td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        td.SampleDesc = { 1, 0 };
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

        D3D11_SUBRESOURCE_DATA isd { };
        isd.pSysMem = pixels;
        isd.SysMemPitch = static_cast< UINT >( tw * 4 );

        ID3D11Texture2D* tex = nullptr;
        if ( SUCCEEDED( dev->CreateTexture2D( &td, &isd, &tex ) ) && tex )
        {
            dev->CreateShaderResourceView( tex, nullptr, &loaded_textures[ i ] );
            tex->Release( );
        }
        stbi_image_free( pixels );
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

        D3D11_BUFFER_DESC vb_desc { };
        vb_desc.ByteWidth = static_cast< UINT >( vertices.size( ) * sizeof( skinned_vertex_t ) );
        vb_desc.Usage = D3D11_USAGE_DYNAMIC;
        vb_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vb_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        D3D11_SUBRESOURCE_DATA vb_data { };
        vb_data.pSysMem = vertices.data( );
        dev->CreateBuffer( &vb_desc, &vb_data, &group.vbo );

        if ( !md.indices.empty( ) )
        {
            D3D11_BUFFER_DESC ib_desc { };
            ib_desc.ByteWidth = static_cast< UINT >( md.indices.size( ) * sizeof( uint32_t ) );
            ib_desc.Usage = D3D11_USAGE_DEFAULT;
            ib_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA ib_data { };
            ib_data.pSysMem = md.indices.data( );
            dev->CreateBuffer( &ib_desc, &ib_data, &group.ibo );
            group.index_count = static_cast< int >( md.indices.size( ) );
        }

        if ( md.material_index >= 0 && md.material_index < static_cast< int >( scene.materials.size( ) ) )
        {
            auto const& mat = scene.materials[ md.material_index ];

            auto resolve = [ & ]( int tex_idx ) -> ID3D11ShaderResourceView* {
                if ( tex_idx < 0 )
                    return nullptr;
                int img_idx = tex_idx;
                if ( tex_idx < static_cast< int >( scene.texture_to_image.size( ) ) )
                    img_idx = scene.texture_to_image[ tex_idx ];
                if ( img_idx >= 0 && img_idx < static_cast< int >( loaded_textures.size( ) ) )
                    return loaded_textures[ img_idx ];
                return nullptr;
            };

            group.texture = resolve( mat.texture_index );
            if ( group.texture ) group.texture->AddRef( );

            group.sphere_tex = resolve( mat.sphere_texture_index );
            if ( group.sphere_tex ) group.sphere_tex->AddRef( );
            group.sphere_mode = mat.sphere_mode;

            if ( mat.shared_toon )
            {
                int const t = mat.toon_texture_index;
                group.toon_tex = ( t >= 0 && t < 10 ) ? engine->shared_toon_srv[ t ] : engine->default_toon_srv;
                if ( group.toon_tex ) group.toon_tex->AddRef( );
            }
            else if ( mat.toon_texture_index >= 0 )
            {
                auto* tex = resolve( mat.toon_texture_index );
                group.toon_tex = tex ? tex : engine->default_toon_srv;
                if ( group.toon_tex ) group.toon_tex->AddRef( );
            }

            group.diffuse_rgba = mat.diffuse_rgba;
            group.alpha_mode = static_cast< int >( mat.alpha_mode );
            group.alpha_cutoff = mat.alpha_cutoff;
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
    }

    for ( auto* t : loaded_textures )
        if ( t ) t->Release( );

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

        static constexpr int k_max_bones = 1024;

        D3D11_BUFFER_DESC buf_desc { };
        buf_desc.ByteWidth = static_cast< UINT >( k_max_bones * sizeof( matrix4x4_t ) );
        buf_desc.Usage = D3D11_USAGE_DEFAULT;
        buf_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

        dev->CreateBuffer( &buf_desc, nullptr, &bone_buffer );
    }

    log_info( "loaded '{}': {} mesh groups, {} textures, {} bones, {} morphs, {} physics bones, {} colliders",
              path, mesh_groups.size( ), loaded_textures.size( ),
              skel ? skel->joint_count( ) : 0, morphs.size( ), scene.physics_bones.size( ), scene.colliders.size( ) );

    return !mesh_groups.empty( );
}

void c_dx11_model::unload( )
{
    for ( auto& g : mesh_groups )
    {
        if ( g.vbo ) g.vbo->Release( );
        if ( g.ibo ) g.ibo->Release( );
        if ( g.texture ) g.texture->Release( );
        if ( g.sphere_tex ) g.sphere_tex->Release( );
        if ( g.toon_tex ) g.toon_tex->Release( );
    }
    mesh_groups.clear( );
    base_vertices.clear( );
    current_vertices.clear( );
    morphs.clear( );

    if ( bone_buffer ) { bone_buffer->Release( ); bone_buffer = nullptr; }
    skel.reset( );
    anim.reset( );
}

void c_dx11_model::render( int width, int height )
{
    if ( mesh_groups.empty( ) || !engine || !engine->pipeline_ready )
        return;

    auto* ctx = engine->ctx_;
    if ( !ctx )
        return;

    if ( width != rtt_width || height != rtt_height )
        create_rtt( width, height );

    if ( !rtv )
        return;

    float clear_color[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
    ctx->ClearRenderTargetView( rtv, clear_color );
    ctx->ClearDepthStencilView( dsv, D3D11_CLEAR_DEPTH, 1.f, 0 );

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
                D3D11_MAPPED_SUBRESOURCE mr { };
                if ( SUCCEEDED( ctx->Map( mesh_groups[ mg ].vbo, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr ) ) )
                {
                    std::memcpy( mr.pData, curr.data( ), curr.size( ) * sizeof( skinned_vertex_t ) );
                    ctx->Unmap( mesh_groups[ mg ].vbo, 0 );
                }
            }
        }
    }

    auto const world_rot = matrix4x4_t::rotation_y( yaw )
                           * matrix4x4_t::rotation_x( pitch );

    auto const world = matrix4x4_t::translation( 0.f, -center_y, 0.f )
                       * world_rot
                       * matrix4x4_t::scaling( 2.5f / model_height );

    auto const view = matrix4x4_t::translation( 0.f, 0.f, k_view_distance / current_zoom );
    auto const proj = matrix4x4_t::perspective( 45.f, static_cast< float >( width ) / static_cast< float >( height ), 0.1f, 100.f );

    cached_wvp = world * view * proj;
    auto const wvp_transposed = cached_wvp.transposed( );
    auto const world_rot_transposed = world_rot.transposed( );

    float const light_len = std::sqrt( 0.3f * 0.3f + 0.7f * 0.7f + 1.0f * 1.0f );

    struct cb_scene_t
    {
        matrix4x4_t wvp;
        matrix4x4_t world_rot;
        float light_dir[ 3 ];
        float ambient;
        float view_dir[ 3 ];
        float rim_strength;
    };
    cb_scene_t cb {
        wvp_transposed,
        world_rot_transposed,
        { 0.3f / light_len, 0.7f / light_len, 1.0f / light_len },
        0.7f,
        { 0.f, 0.f, -1.f },
        0.3f
    };
    ctx->UpdateSubresource( engine->cb_scene, 0, nullptr, &cb, 0, 0 );

    if ( skel && bone_buffer )
    {
        auto skin_matrices = skel->compute_skinning_matrices( );

        static constexpr int k_max_bones = 1024;
        std::array< matrix4x4_t, k_max_bones > padded { };
        auto const count = ( std::min )( static_cast< int >( skin_matrices.size( ) ), k_max_bones );
        std::memcpy( padded.data( ), skin_matrices.data( ), count * sizeof( matrix4x4_t ) );

        ctx->UpdateSubresource( bone_buffer, 0, nullptr, padded.data( ), 0, 0 );
    }

    D3D11_VIEWPORT vp { };
    vp.Width = static_cast< float >( width );
    vp.Height = static_cast< float >( height );
    vp.MaxDepth = 1.f;

    ctx->RSSetViewports( 1, &vp );
    ctx->RSSetState( engine->rasterizer );
    ctx->OMSetRenderTargets( 1, &rtv, dsv );
    ctx->OMSetDepthStencilState( engine->depth_state, 0 );

    float blend_factor[ 4 ] = { 0.f, 0.f, 0.f, 0.f };
    ctx->OMSetBlendState( engine->blend_state, blend_factor, 0xFFFFFFFF );

    ctx->IASetInputLayout( engine->input_layout );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->VSSetShader( engine->vs, nullptr, 0 );
    ctx->VSSetConstantBuffers( 0, 1, &engine->cb_scene );
    if ( bone_buffer )
        ctx->VSSetConstantBuffers( 1, 1, &bone_buffer );
    ctx->PSSetShader( engine->ps, nullptr, 0 );
    ID3D11SamplerState* samplers[ 2 ] = { engine->sampler, engine->sampler_clamp };
    ctx->PSSetSamplers( 0, 2, samplers );
    ctx->PSSetConstantBuffers( 0, 1, &engine->cb_scene );
    ctx->PSSetConstantBuffers( 2, 1, &engine->cb_material );

    struct fx_cbuf_t
    {
        int mode;
        int pad0;
        int pad1;
        int pad2;
        float main_color[ 4 ];
        float rim_color[ 4 ];
        float params[ 4 ];
    };
    fx_cbuf_t fx_cb { };
    auto const* fx_ptr = engine->fx_settings_ptr;
    if ( fx_ptr )
    {
        fx_cb.mode = static_cast< int >( fx_ptr->mode );
        fx_cb.main_color[ 0 ] = fx_ptr->main_color.x;
        fx_cb.main_color[ 1 ] = fx_ptr->main_color.y;
        fx_cb.main_color[ 2 ] = fx_ptr->main_color.z;
        fx_cb.main_color[ 3 ] = fx_ptr->main_color.w;
        fx_cb.rim_color[ 0 ] = fx_ptr->rim_color.x;
        fx_cb.rim_color[ 1 ] = fx_ptr->rim_color.y;
        fx_cb.rim_color[ 2 ] = fx_ptr->rim_color.z;
        fx_cb.rim_color[ 3 ] = fx_ptr->rim_color.w;
        fx_cb.params[ 0 ] = fx_ptr->rim_power;
        fx_cb.params[ 3 ] = static_cast< float >( ImGui::GetTime( ) );
    }
    ctx->UpdateSubresource( engine->cb_fx, 0, nullptr, &fx_cb, 0, 0 );
    ctx->PSSetConstantBuffers( 3, 1, &engine->cb_fx );

    UINT const stride = sizeof( skinned_vertex_t );
    UINT const offset = 0;

    struct cb_material_t
    {
        float diffuse_color[ 4 ];
        int sphere_mode;
        int has_toon;
        float alpha_cutoff;
        float pad;
    };

    auto draw_group = [ & ]( mesh_group_t const& g ) {
        if ( !g.vbo )
            return;

        cb_material_t const mat_cb {
            { g.diffuse_rgba[ 0 ], g.diffuse_rgba[ 1 ], g.diffuse_rgba[ 2 ], g.diffuse_rgba[ 3 ] },
            g.sphere_mode,
            g.toon_tex ? 1 : 0,
            g.alpha_cutoff,
            0.f
        };
        ctx->UpdateSubresource( engine->cb_material, 0, nullptr, &mat_cb, 0, 0 );

        ctx->IASetVertexBuffers( 0, 1, &g.vbo, &stride, &offset );

        ID3D11ShaderResourceView* srvs[ 3 ] = {
            g.texture ? g.texture : engine->white_srv,
            g.sphere_tex ? g.sphere_tex : engine->white_srv,
            g.toon_tex ? g.toon_tex : engine->default_toon_srv
        };
        ctx->PSSetShaderResources( 0, 3, srvs );

        if ( g.ibo && g.index_count > 0 )
        {
            ctx->IASetIndexBuffer( g.ibo, DXGI_FORMAT_R32_UINT, 0 );
            ctx->DrawIndexed( static_cast< UINT >( g.index_count ), 0, 0 );
        }
    };

    ctx->OMSetDepthStencilState( engine->depth_state, 0 );
    for ( auto const& g : mesh_groups )
        if ( g.alpha_mode != 2 )
            draw_group( g );

    ctx->OMSetDepthStencilState( engine->depth_state_blend, 0 );
    for ( auto const& g : mesh_groups )
        if ( g.alpha_mode == 2 )
            draw_group( g );

    if ( msaa_tex && resolve_tex )
        ctx->ResolveSubresource( resolve_tex, 0, msaa_tex, 0, DXGI_FORMAT_R8G8B8A8_UNORM );
}
