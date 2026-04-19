//
// Created by rei on 4/12/2026.
//

#include "blur.hpp"

#include "core.hpp"
#include "cheat/cheat.hpp"
#include "graphics/graphics.hpp"
#include "renderer/renderer.hpp"

#include <d3dcompiler.h>
#include <dxgi1_2.h>
#include <algorithm>
#include <cstring>

#pragma comment( lib, "d3dcompiler.lib" )

using alice::c_dx11_blur;

static constexpr char const* k_bg_vs = R"hlsl(
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT main(uint id : SV_VertexID)
{
    float2 uv = float2((id == 1) ? 2.0 : 0.0,
                       (id == 2) ? 2.0 : 0.0);
    VS_OUT o;
    o.uv  = uv;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return o;
}
)hlsl";

static constexpr char const* k_blur_ps = R"hlsl(
Texture2D    u_tex  : register(t0);
SamplerState u_samp : register(s0);
cbuffer CB : register(b0) {
    float  u_radius;
    float  _p0;
    float2 u_rt_size;
    int    u_horiz;
    int3   _p1;
};
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
static const int HALF = 16;
float gauss(float x, float s) { return exp(-(x*x)/(2.0*s*s)); }
float4 main(PS_IN i) : SV_Target
{
    float2 dir   = u_horiz ? float2(1,0) : float2(0,1);
    float2 texel = 1.0 / u_rt_size;
    float  r     = max(u_radius, 1.0);
    float  sigma = r * 0.4;
    float4 acc   = 0;
    float  wsum  = 0.0;
    for (int k = -HALF; k <= HALF; ++k)
    {
        float x = float(k);
        float w = gauss(x, sigma);
        acc  += u_tex.Sample(u_samp, i.uv + dir * texel * (x * (r / float(HALF)))) * w;
        wsum += w;
    }
    return acc / max(wsum, 0.0001);
}
)hlsl";

static constexpr char const* k_composite_ps = R"hlsl(
Texture2D    u_tex  : register(t0);
SamplerState u_samp : register(s0);
cbuffer CB : register(b0) {
    float2 u_rect_min;
    float2 u_rect_max;
    float2 u_rt_size;
    float  u_rounding;
    float  u_alpha;
    float2 _pad;
};
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

float sdf_rounded_box(float2 p, float2 b, float r)
{
    float2 q = abs(p) - b + r;
    return length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - r;
}

float4 main(PS_IN i) : SV_Target
{
    float2 sp       = i.uv * u_rt_size;
    float2 center   = (u_rect_min + u_rect_max) * 0.5;
    float2 halfsize = (u_rect_max - u_rect_min) * 0.5;
    float  rounding = min(u_rounding, min(halfsize.x, halfsize.y));
    float  dist     = sdf_rounded_box(sp - center, halfsize, rounding);
    float  alpha    = (1.0 - smoothstep(-0.5, 0.0, dist)) * saturate(u_alpha);
    if (alpha <= 0.001)
        discard;
    float4 c = u_tex.Sample(u_samp, i.uv);
    return float4(c.rgb * alpha, alpha);
}
)hlsl";

static constexpr char const* k_composite_squircle_ps = R"hlsl(
Texture2D    u_tex  : register(t0);
SamplerState u_samp : register(s0);
cbuffer CB : register(b0) {
    float2 u_rect_min;
    float2 u_rect_max;
    float2 u_rt_size;
    float  u_rounding;
    float  u_alpha;
    float  u_exponent;
    float  u_arm_ratio;
};
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };

float sdf_squircle_box(float2 p, float2 b, float arm, float p_exp)
{
    float2 q = abs(p) - b + arm;
    float2 qp = max(q, 0.0);
    float lp = pow(pow(qp.x, p_exp) + pow(qp.y, p_exp), 1.0 / p_exp) - arm;
    float ln = min(max(q.x, q.y), 0.0);
    return lp + ln;
}

float4 main(PS_IN i) : SV_Target
{
    float2 sp       = i.uv * u_rt_size;
    float2 center   = (u_rect_min + u_rect_max) * 0.5;
    float2 halfsize = (u_rect_max - u_rect_min) * 0.5;
    float  half_min = min(halfsize.x, halfsize.y);
    float  arm      = min(u_rounding * max(u_arm_ratio, 0.01), half_min);
    float  p_exp    = max(u_exponent, 2.0);
    float  dist     = sdf_squircle_box(sp - center, halfsize, arm, p_exp);
    float  alpha    = (1.0 - smoothstep(-0.5, 0.0, dist)) * saturate(u_alpha);
    if (alpha <= 0.001)
        discard;
    float4 c = u_tex.Sample(u_samp, i.uv);
    return float4(c.rgb * alpha, alpha);
}
)hlsl";

static constexpr char const* k_merge_ps = R"hlsl(
Texture2D    u_desktop : register(t0);
Texture2D    u_overlay : register(t1);
SamplerState u_samp    : register(s0);
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PS_IN i) : SV_Target
{
    float4 bg = u_desktop.Sample(u_samp, i.uv);
    float4 fg = u_overlay.Sample(u_samp, i.uv);
    float3 merged = fg.rgb + bg.rgb * (1.0 - fg.a);
    return float4(merged, 1.0);
}
)hlsl";

void c_dx11_blur::rt_t::release( )
{
    if ( tex )
    {
        tex->Release( );
        tex = nullptr;
    }
    if ( rtv )
    {
        rtv->Release( );
        rtv = nullptr;
    }
    if ( srv )
    {
        srv->Release( );
        srv = nullptr;
    }
}

bool c_dx11_blur::init( ID3D11Device* dev, ID3D11DeviceContext* context, int w, int h )
{
    device = dev;
    ctx = context;
    width = w;
    height = h;

    if ( !create_shaders( ) )
        return false;
    return create_states( );
}

void c_dx11_blur::resize( int w, int h )
{
    if ( w <= 0 || h <= 0 )
        return;
    if ( w == width && h == height )
        return;
    release_rts( );
    width = w;
    height = h;
}

void c_dx11_blur::destroy( )
{
    destroy_duplication( );
    release_rts( );
    safe_release( vs );
    safe_release( ps_blur );
    safe_release( ps_composite );
    safe_release( ps_composite_squircle );
    safe_release( ps_merge );
    safe_release( cb_blur );
    safe_release( cb_composite );
    safe_release( bs_alpha );
    safe_release( ds_off );
    safe_release( rs );
    safe_release( rs_scissor );
    safe_release( sampler );
}

bool c_dx11_blur::init_duplication( )
{
    IDXGIDevice* dxgi_dev = nullptr;
    if ( FAILED( device->QueryInterface( __uuidof( IDXGIDevice ), ( void** ) &dxgi_dev ) ) )
        return false;

    IDXGIAdapter* adapter = nullptr;
    dxgi_dev->GetAdapter( &adapter );
    dxgi_dev->Release( );
    if ( !adapter )
        return false;

    IDXGIOutput* output = nullptr;
    if ( FAILED( adapter->EnumOutputs( 0, &output ) ) )
    {
        adapter->Release( );
        return false;
    }
    adapter->Release( );

    IDXGIOutput1* output1 = nullptr;
    if ( FAILED( output->QueryInterface( __uuidof( IDXGIOutput1 ), ( void** ) &output1 ) ) )
    {
        output->Release( );
        return false;
    }
    output->Release( );

    HRESULT hr = output1->DuplicateOutput( device, &duplication );
    output1->Release( );

    return SUCCEEDED( hr ) && duplication;
}

void c_dx11_blur::destroy_duplication( )
{
    safe_release( duplication );
    rt_desktop.release( );
    desktop_frame_valid = false;
}

void c_dx11_blur::capture_desktop( )
{
    if ( !duplication )
        return;

    DXGI_OUTDUPL_FRAME_INFO info { };
    IDXGIResource* resource = nullptr;

    HRESULT hr = duplication->AcquireNextFrame( 0, &info, &resource );

    if ( hr == DXGI_ERROR_WAIT_TIMEOUT )
        return;

    if ( hr == DXGI_ERROR_ACCESS_LOST )
    {
        safe_release( duplication );
        rt_desktop.release( );
        desktop_frame_valid = false;
        init_duplication( );
        return;
    }

    if ( FAILED( hr ) )
        return;

    ID3D11Texture2D* frame_tex = nullptr;
    resource->QueryInterface( __uuidof( ID3D11Texture2D ), ( void** ) &frame_tex );
    resource->Release( );

    if ( !frame_tex )
    {
        duplication->ReleaseFrame( );
        return;
    }

    if ( !rt_desktop.tex )
    {
        D3D11_TEXTURE2D_DESC desc { };
        frame_tex->GetDesc( &desc );
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        desc.MiscFlags = 0;

        if ( FAILED( device->CreateTexture2D( &desc, nullptr, &rt_desktop.tex ) ) ||
             FAILED( device->CreateShaderResourceView( rt_desktop.tex, nullptr, &rt_desktop.srv ) ) )
        {
            rt_desktop.release( );
            frame_tex->Release( );
            duplication->ReleaseFrame( );
            return;
        }
    }

    ctx->CopyResource( rt_desktop.tex, frame_tex );
    frame_tex->Release( );
    duplication->ReleaseFrame( );
    desktop_frame_valid = true;
}

void c_dx11_blur::prepare_frame( float radius, int passes )
{
    ID3D11RenderTargetView* back_rtv = nullptr;
    ctx->OMGetRenderTargets( 1, &back_rtv, nullptr );
    if ( !back_rtv )
        return;
    if ( !ensure_rts( back_rtv ) )
    {
        back_rtv->Release( );
        return;
    }

    int const frame = ImGui::GetFrameCount( );
    if ( frame == last_frame_hash )
        return;
    last_frame_hash = frame;

    save_state( );

    ID3D11ShaderResourceView* source = nullptr;

    if ( desktop_frame_valid && rt_desktop.srv )
    {
        source = rt_desktop.srv;
    } else if ( capture_backbuffer( back_rtv ) )
    {
        source = rt_capture.srv;
    }

    if ( source )
    {
        int const p = std::max( 1, std::min( passes, 4 ) );
        float const r = std::max( 1.f, radius / ( float ) k_downsample );

        blur_pass( source, rt_ping, rt_pong, r );

        for ( int i = 1; i < p; ++i )
            blur_pass( rt_pong.srv, rt_ping, rt_pong, r * ( float ) ( i + 1 ) * 0.6f );
    }

    restore_state( );
    back_rtv->Release( );
}

void c_dx11_blur::apply( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                         float radius, float rounding, int passes, float alpha )
{
    if ( !dl )
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
    p->cmd.shape = shape_e::rounded_box;
    p->cmd.run_pipeline = true;
    p->cmd.composite = true;

    dl->AddCallback( &c_dx11_blur::blur_callback, p );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_dx11_blur::apply_squircle( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                                  float radius, float rounding, float exponent, float arm_ratio,
                                  int passes, float alpha )
{
    if ( !dl )
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
    p->cmd.exponent = exponent;
    p->cmd.arm_ratio = arm_ratio;
    p->cmd.alpha = alpha;
    p->cmd.passes = std::max( 1, std::min( passes, 4 ) );
    p->cmd.shape = shape_e::squircle;
    p->cmd.run_pipeline = true;
    p->cmd.composite = true;

    dl->AddCallback( &c_dx11_blur::blur_callback, p );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_dx11_blur::uv_for( ImVec2 p_min, ImVec2 p_max, ImVec2& uv_min, ImVec2& uv_max ) const
{
    float const inv_w = width > 0 ? 1.f / static_cast< float >( width ) : 0.f;
    float const inv_h = height > 0 ? 1.f / static_cast< float >( height ) : 0.f;
    uv_min = { p_min.x * inv_w, p_min.y * inv_h };
    uv_max = { p_max.x * inv_w, p_max.y * inv_h };
}

void c_dx11_blur::add_image( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col ) const
{
    if ( !dl || !rt_pong.srv )
        return;
    ImVec2 uv_min, uv_max;
    uv_for( p_min, p_max, uv_min, uv_max );
    dl->AddImage( get_texture( ), p_min, p_max, uv_min, uv_max, col );
}

void c_dx11_blur::add_image_rounded( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                     ImDrawFlags flags, ImU32 col ) const
{
    if ( !dl || !rt_pong.srv )
        return;
    ImVec2 uv_min, uv_max;
    uv_for( p_min, p_max, uv_min, uv_max );
    dl->AddImageRounded( get_texture( ), p_min, p_max, uv_min, uv_max, col, rounding, flags );
}

void c_dx11_blur::add_image_squircle( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                      ImDrawFlags flags, float exponent, float arm_ratio,
                                      int segs_per_corner, ImU32 col ) const
{
    if ( !dl || !rt_pong.srv )
        return;
    if ( p_max.x <= p_min.x || p_max.y <= p_min.y )
        return;

    if ( rounding <= 0.f || ( flags & ImDrawFlags_RoundCornersAll ) == 0 )
    {
        add_image( dl, p_min, p_max, col );
        return;
    }

    if ( segs_per_corner <= 0 )
        segs_per_corner = std::max( 8, static_cast< int >( rounding * 0.8f ) );

    dl->_Path.Size = 0;
    renderer_t::build_squircle_path( dl, p_min, p_max, rounding, flags, exponent, arm_ratio, segs_per_corner );

    int const count = dl->_Path.Size;
    if ( count < 3 )
    {
        dl->_Path.Size = 0;
        return;
    }

    int const idx_count = ( count - 2 ) * 3;
    int const vtx_count = count;

    float const inv_w = width > 0 ? 1.f / static_cast< float >( width ) : 0.f;
    float const inv_h = height > 0 ? 1.f / static_cast< float >( height ) : 0.f;

    dl->PushTextureID( get_texture( ) );
    dl->PrimReserve( idx_count, vtx_count );

    auto const base_idx = dl->_VtxCurrentIdx;
    auto* vtx = dl->_VtxWritePtr;
    auto* idx = dl->_IdxWritePtr;
    auto const* points = dl->_Path.Data;

    for ( int i = 0; i < count; ++i )
    {
        vtx[ i ].pos = points[ i ];
        vtx[ i ].uv = { points[ i ].x * inv_w, points[ i ].y * inv_h };
        vtx[ i ].col = col;
    }

    for ( int i = 2; i < count; ++i )
    {
        idx[ 0 ] = static_cast< ImDrawIdx >( base_idx );
        idx[ 1 ] = static_cast< ImDrawIdx >( base_idx + i - 1 );
        idx[ 2 ] = static_cast< ImDrawIdx >( base_idx + i );
        idx += 3;
    }

    dl->_VtxWritePtr += vtx_count;
    dl->_IdxWritePtr += idx_count;
    dl->_VtxCurrentIdx += vtx_count;

    dl->PopTextureID( );
    dl->_Path.Size = 0;
}

void c_dx11_blur::begin( ImDrawList* dl, float radius, int passes )
{
    if ( !dl )
        return;

    int const frame = ImGui::GetFrameCount( );
    if ( frame == last_frame_hash )
        return;
    last_frame_hash = frame;

    auto* p = new callback_data_t { };
    p->renderer = this;
    p->cmd.radius = radius;
    p->cmd.passes = std::max( 1, std::min( passes, 4 ) );
    p->cmd.run_pipeline = true;
    p->cmd.composite = false;

    dl->AddCallback( &c_dx11_blur::blur_callback, p );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

ID3DBlob* c_dx11_blur::compile( char const* src, char const* entry, char const* target )
{
    ID3DBlob* code = nullptr;
    ID3DBlob* error = nullptr;
    HRESULT hr = D3DCompile( src, strlen( src ), nullptr, nullptr, nullptr,
                             entry, target,
                             D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3,
                             0, &code, &error );
    if ( FAILED( hr ) )
    {
        if ( error )
            error->Release( );
        return nullptr;
    }
    if ( error )
        error->Release( );
    return code;
}

bool c_dx11_blur::make_cb( ID3D11Device* dev, UINT size, ID3D11Buffer** out )
{
    D3D11_BUFFER_DESC d { };
    d.ByteWidth = ( size + 15u ) & ~15u;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED( dev->CreateBuffer( &d, nullptr, out ) );
}

template < typename T >
void c_dx11_blur::update_cb( ID3D11Buffer* cb, T const& data )
{
    D3D11_MAPPED_SUBRESOURCE mr { };
    if ( SUCCEEDED( ctx->Map( cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr ) ) )
    {
        memcpy( mr.pData, &data, sizeof( T ) );
        ctx->Unmap( cb, 0 );
    }
}

void c_dx11_blur::set_viewport( int w, int h )
{
    D3D11_VIEWPORT vp { };
    vp.Width = ( float ) w;
    vp.Height = ( float ) h;
    vp.MaxDepth = 1.f;
    ctx->RSSetViewports( 1, &vp );
}

bool c_dx11_blur::create_shaders( )
{
    {
        auto* b = compile( k_bg_vs, "main", "vs_4_0" );
        if ( !b )
            return false;
        bool ok = SUCCEEDED( device->CreateVertexShader( b->GetBufferPointer( ), b->GetBufferSize( ), nullptr, &vs ) );
        b->Release( );
        if ( !ok )
            return false;
    }

    {
        auto* b = compile( k_blur_ps, "main", "ps_4_0" );
        if ( !b )
            return false;
        bool ok = SUCCEEDED( device->CreatePixelShader( b->GetBufferPointer( ), b->GetBufferSize( ), nullptr, &ps_blur ) );
        b->Release( );
        if ( !ok )
            return false;
    }

    {
        auto* b = compile( k_composite_ps, "main", "ps_4_0" );
        if ( !b )
            return false;
        bool ok = SUCCEEDED( device->CreatePixelShader( b->GetBufferPointer( ), b->GetBufferSize( ), nullptr, &ps_composite ) );
        b->Release( );
        if ( !ok )
            return false;
    }

    {
        auto* b = compile( k_composite_squircle_ps, "main", "ps_4_0" );
        if ( !b )
            return false;
        bool ok = SUCCEEDED( device->CreatePixelShader( b->GetBufferPointer( ), b->GetBufferSize( ), nullptr, &ps_composite_squircle ) );
        b->Release( );
        if ( !ok )
            return false;
    }

    {
        auto* b = compile( k_merge_ps, "main", "ps_4_0" );
        if ( !b )
            return false;
        bool ok = SUCCEEDED( device->CreatePixelShader( b->GetBufferPointer( ), b->GetBufferSize( ), nullptr, &ps_merge ) );
        b->Release( );
        if ( !ok )
            return false;
    }

    struct blur_buf_t
    {
        float radius, p0, rt_w, rt_h;
        int horiz, p1[ 3 ];
    };
    struct comp_buf_t
    {
        float rmin_x, rmin_y, rmax_x, rmax_y, rt_w, rt_h, rounding, alpha, exponent, arm_ratio, pad0, pad1;
    };

    if ( !make_cb( device, sizeof( blur_buf_t ), &cb_blur ) )
        return false;
    return make_cb( device, sizeof( comp_buf_t ), &cb_composite );
}

bool c_dx11_blur::create_states( )
{
    {
        D3D11_BLEND_DESC d { };
        auto& rt = d.RenderTarget[ 0 ];
        rt.BlendEnable = TRUE;
        rt.SrcBlend = D3D11_BLEND_ONE;
        rt.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOp = D3D11_BLEND_OP_ADD;
        rt.SrcBlendAlpha = D3D11_BLEND_ONE;
        rt.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
        rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        if ( FAILED( device->CreateBlendState( &d, &bs_alpha ) ) )
            return false;
    }

    {
        D3D11_DEPTH_STENCIL_DESC d { };
        d.DepthEnable = FALSE;
        d.StencilEnable = FALSE;
        if ( FAILED( device->CreateDepthStencilState( &d, &ds_off ) ) )
            return false;
    }

    {
        D3D11_RASTERIZER_DESC d { };
        d.FillMode = D3D11_FILL_SOLID;
        d.CullMode = D3D11_CULL_NONE;
        d.DepthClipEnable = FALSE;
        d.ScissorEnable = FALSE;
        if ( FAILED( device->CreateRasterizerState( &d, &rs ) ) )
            return false;
        d.ScissorEnable = TRUE;
        if ( FAILED( device->CreateRasterizerState( &d, &rs_scissor ) ) )
            return false;
    }

    {
        D3D11_SAMPLER_DESC d { };
        d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        d.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        d.MaxLOD = D3D11_FLOAT32_MAX;
        if ( FAILED( device->CreateSamplerState( &d, &sampler ) ) )
            return false;
    }

    return true;
}

bool c_dx11_blur::create_rt( int w, int h, DXGI_FORMAT fmt, rt_t& rt )
{
    D3D11_TEXTURE2D_DESC td { };
    td.Width = ( UINT ) w;
    td.Height = ( UINT ) h;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if ( FAILED( device->CreateTexture2D( &td, nullptr, &rt.tex ) ) )
        return false;
    if ( FAILED( device->CreateRenderTargetView( rt.tex, nullptr, &rt.rtv ) ) )
        return false;
    return SUCCEEDED( device->CreateShaderResourceView( rt.tex, nullptr, &rt.srv ) );
}

void c_dx11_blur::release_rts( )
{
    rt_capture.release( );
    rt_ping.release( );
    rt_pong.release( );
    rt_merged.release( );
}

bool c_dx11_blur::ensure_rts( ID3D11RenderTargetView* back_rtv )
{
    ID3D11Resource* res = nullptr;
    back_rtv->GetResource( &res );
    if ( !res )
        return false;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = res->QueryInterface( __uuidof( ID3D11Texture2D ), ( void** ) &tex );
    res->Release( );
    if ( FAILED( hr ) || !tex )
        return false;

    D3D11_TEXTURE2D_DESC desc { };
    tex->GetDesc( &desc );
    tex->Release( );

    if ( ( int ) desc.Width != width || ( int ) desc.Height != height || !rt_ping.tex )
    {
        release_rts( );
        width = ( int ) desc.Width;
        height = ( int ) desc.Height;
        small_w = std::max( 1, width / k_downsample );
        small_h = std::max( 1, height / k_downsample );

        if ( !create_rt( width, height, desc.Format, rt_capture ) )
            return false;
        if ( !create_rt( small_w, small_h, DXGI_FORMAT_R16G16B16A16_FLOAT, rt_ping ) )
            return false;
        if ( !create_rt( small_w, small_h, DXGI_FORMAT_R16G16B16A16_FLOAT, rt_pong ) )
            return false;
        if ( !create_rt( width, height, DXGI_FORMAT_R16G16B16A16_FLOAT, rt_merged ) )
            return false;
    }

    return true;
}

bool c_dx11_blur::capture_backbuffer( ID3D11RenderTargetView* back_rtv )
{
    ID3D11Resource* res = nullptr;
    back_rtv->GetResource( &res );
    if ( !res )
        return false;

    ID3D11Texture2D* tex = nullptr;
    HRESULT hr = res->QueryInterface( __uuidof( ID3D11Texture2D ), ( void** ) &tex );
    res->Release( );
    if ( FAILED( hr ) || !tex )
        return false;

    ctx->CopyResource( rt_capture.tex, tex );
    tex->Release( );
    return true;
}

void c_dx11_blur::merge_pass( )
{
    static constexpr float k_black[ 4 ] { };

    ctx->VSSetShader( vs, nullptr, 0 );
    ctx->PSSetShader( ps_merge, nullptr, 0 );
    ctx->IASetInputLayout( nullptr );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->OMSetBlendState( nullptr, nullptr, 0xffffffff );
    ctx->OMSetDepthStencilState( ds_off, 0 );
    ctx->RSSetState( rs );
    ctx->PSSetSamplers( 0, 1, &sampler );
    set_viewport( width, height );

    ctx->ClearRenderTargetView( rt_merged.rtv, k_black );
    ctx->OMSetRenderTargets( 1, &rt_merged.rtv, nullptr );

    ID3D11ShaderResourceView* srvs[ 2 ] = { rt_desktop.srv, rt_capture.srv };
    ctx->PSSetShaderResources( 0, 2, srvs );

    ctx->Draw( 3, 0 );

    ID3D11ShaderResourceView* null_srvs[ 2 ] = { nullptr, nullptr };
    ctx->PSSetShaderResources( 0, 2, null_srvs );
}

void c_dx11_blur::blur_pass( ID3D11ShaderResourceView* src, rt_t& h, rt_t& v, float radius )
{
    struct blur_buf_t
    {
        float radius, p0, rt_w, rt_h;
        int horiz, p1[ 3 ];
    };

    static constexpr float k_black[ 4 ] { };

    ctx->VSSetShader( vs, nullptr, 0 );
    ctx->PSSetShader( ps_blur, nullptr, 0 );
    ctx->IASetInputLayout( nullptr );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->OMSetBlendState( nullptr, nullptr, 0xffffffff );
    ctx->OMSetDepthStencilState( ds_off, 0 );
    ctx->RSSetState( rs );
    ctx->PSSetSamplers( 0, 1, &sampler );
    set_viewport( small_w, small_h );

    ctx->ClearRenderTargetView( h.rtv, k_black );
    ctx->OMSetRenderTargets( 1, &h.rtv, nullptr );
    ctx->PSSetShaderResources( 0, 1, &src );
    {
        blur_buf_t b { radius, 0.f, ( float ) small_w, ( float ) small_h, 1, { } };
        update_cb( cb_blur, b );
    }
    ctx->PSSetConstantBuffers( 0, 1, &cb_blur );
    ctx->Draw( 3, 0 );

    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources( 0, 1, &null_srv );

    ctx->ClearRenderTargetView( v.rtv, k_black );
    ctx->OMSetRenderTargets( 1, &v.rtv, nullptr );
    ctx->PSSetShaderResources( 0, 1, &h.srv );
    {
        blur_buf_t b { radius, 0.f, ( float ) small_w, ( float ) small_h, 0, { } };
        update_cb( cb_blur, b );
    }
    ctx->PSSetConstantBuffers( 0, 1, &cb_blur );
    ctx->Draw( 3, 0 );

    ctx->PSSetShaderResources( 0, 1, &null_srv );
}

void c_dx11_blur::composite( ID3D11RenderTargetView* back_rtv, blur_cmd_t const& cmd )
{
    struct comp_buf_t
    {
        float rmin_x, rmin_y, rmax_x, rmax_y, rt_w, rt_h, rounding, alpha, exponent, arm_ratio, pad0, pad1;
    };

    D3D11_RECT sr;
    sr.left = ( LONG ) std::max( 0, ( int ) cmd.clip_min.x );
    sr.top = ( LONG ) std::max( 0, ( int ) cmd.clip_min.y );
    sr.right = ( LONG ) std::min( width, ( int ) cmd.clip_max.x );
    sr.bottom = ( LONG ) std::min( height, ( int ) cmd.clip_max.y );
    if ( sr.right <= sr.left || sr.bottom <= sr.top )
        return;

    ctx->OMSetRenderTargets( 1, &back_rtv, nullptr );
    ctx->OMSetBlendState( bs_alpha, nullptr, 0xffffffff );
    ctx->OMSetDepthStencilState( ds_off, 0 );
    ctx->RSSetState( rs_scissor );
    ctx->RSSetScissorRects( 1, &sr );
    set_viewport( width, height );

    ctx->VSSetShader( vs, nullptr, 0 );
    ctx->PSSetShader( cmd.shape == shape_e::squircle ? ps_composite_squircle : ps_composite, nullptr, 0 );
    ctx->IASetInputLayout( nullptr );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->PSSetSamplers( 0, 1, &sampler );
    ctx->PSSetShaderResources( 0, 1, &rt_pong.srv );

    comp_buf_t cb {
        cmd.min.x, cmd.min.y, cmd.max.x, cmd.max.y,
        ( float ) width, ( float ) height,
        cmd.rounding, cmd.alpha,
        cmd.exponent, cmd.arm_ratio, 0.f, 0.f };
    update_cb( cb_composite, cb );
    ctx->PSSetConstantBuffers( 0, 1, &cb_composite );
    ctx->Draw( 3, 0 );

    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources( 0, 1, &null_srv );
}

void c_dx11_blur::render_blur( blur_cmd_t const& cmd, ID3D11RenderTargetView* back_rtv )
{
    if ( !ensure_rts( back_rtv ) )
        return;

    if ( cmd.run_pipeline )
    {
        if ( !capture_backbuffer( back_rtv ) )
            return;

        ID3D11ShaderResourceView* source = rt_capture.srv;

        if ( desktop_frame_valid && rt_desktop.srv )
        {
            merge_pass( );
            source = rt_merged.srv;
        }

        float const r = std::max( 1.f, cmd.radius / ( float ) k_downsample );

        blur_pass( source, rt_ping, rt_pong, r );

        for ( int i = 1; i < cmd.passes; ++i )
            blur_pass( rt_pong.srv, rt_ping, rt_pong, r * ( float ) ( i + 1 ) * 0.6f );
    }

    if ( cmd.composite )
        composite( back_rtv, cmd );
}

void c_dx11_blur::blur_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    if ( !everness::core->graphics->blur_effects )
        return;

    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer )
        return;

    auto* r = data->renderer;
    data->cmd.clip_min = { draw_cmd->ClipRect.x, draw_cmd->ClipRect.y };
    data->cmd.clip_max = { draw_cmd->ClipRect.z, draw_cmd->ClipRect.w };

    ID3D11RenderTargetView* back_rtv = nullptr;
    ID3D11DepthStencilView* back_dsv = nullptr;
    r->ctx->OMGetRenderTargets( 1, &back_rtv, &back_dsv );

    if ( back_rtv )
    {
        r->save_state( );
        r->render_blur( data->cmd, back_rtv );
        r->restore_state( );
        back_rtv->Release( );
    }

    if ( back_dsv )
        back_dsv->Release( );

    delete data;
}

void c_dx11_blur::save_state( )
{
    auto& s = saved;
    s.scissor_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    s.vp_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
    s.vs_inst_count = 256;
    s.ps_inst_count = 256;

    ctx->RSGetScissorRects( &s.scissor_count, s.scissors );
    ctx->RSGetViewports( &s.vp_count, s.vps );
    ctx->RSGetState( &s.rs );
    ctx->OMGetBlendState( &s.bs, s.blend_factor, &s.sample_mask );
    ctx->OMGetDepthStencilState( &s.ds, &s.stencil_ref );
    ctx->OMGetRenderTargets( 1, &s.rtv, &s.dsv );
    ctx->IAGetPrimitiveTopology( &s.topology );
    ctx->IAGetInputLayout( &s.il );
    ctx->VSGetShader( &s.vs, s.vs_inst, &s.vs_inst_count );
    ctx->VSGetConstantBuffers( 0, 1, &s.vs_cb );
    ctx->PSGetShader( &s.ps, s.ps_inst, &s.ps_inst_count );
    ctx->PSGetConstantBuffers( 0, 1, &s.ps_cb );
    ctx->PSGetShaderResources( 0, 1, &s.ps_srv );
    ctx->PSGetSamplers( 0, 1, &s.ps_sampler );
}

void c_dx11_blur::restore_state( )
{
    auto& s = saved;
    ctx->RSSetScissorRects( s.scissor_count, s.scissors );
    ctx->RSSetViewports( s.vp_count, s.vps );
    ctx->RSSetState( s.rs );
    safe_release( s.rs );
    ctx->OMSetBlendState( s.bs, s.blend_factor, s.sample_mask );
    safe_release( s.bs );
    ctx->OMSetDepthStencilState( s.ds, s.stencil_ref );
    safe_release( s.ds );
    ctx->OMSetRenderTargets( 1, &s.rtv, s.dsv );
    safe_release( s.rtv );
    safe_release( s.dsv );
    ctx->IASetPrimitiveTopology( s.topology );
    ctx->IASetInputLayout( s.il );
    safe_release( s.il );
    ctx->VSSetShader( s.vs, s.vs_inst, s.vs_inst_count );
    safe_release( s.vs );
    ctx->VSSetConstantBuffers( 0, 1, &s.vs_cb );
    safe_release( s.vs_cb );
    ctx->PSSetShader( s.ps, s.ps_inst, s.ps_inst_count );
    safe_release( s.ps );
    ctx->PSSetConstantBuffers( 0, 1, &s.ps_cb );
    safe_release( s.ps_cb );
    ctx->PSSetShaderResources( 0, 1, &s.ps_srv );
    safe_release( s.ps_srv );
    ctx->PSSetSamplers( 0, 1, &s.ps_sampler );
    safe_release( s.ps_sampler );
    for ( UINT i = 0; i < s.vs_inst_count; ++i )
        safe_release( s.vs_inst[ i ] );
    for ( UINT i = 0; i < s.ps_inst_count; ++i )
        safe_release( s.ps_inst[ i ] );
}
