#include "grayscale.hpp"

#include <d3dcompiler.h>
#include <algorithm>
#include <cstring>
#include <vector>

#pragma comment( lib, "d3dcompiler.lib" )

using alice::c_dx11_grayscale;

static constexpr char const* k_imgui_vs = R"hlsl(
cbuffer cb_proj : register(b0)
{
    float4x4 u_proj;
};
struct VS_IN  { float2 pos : POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
VS_OUT main(VS_IN v)
{
    VS_OUT o;
    o.pos = mul(u_proj, float4(v.pos, 0.0, 1.0));
    o.uv  = v.uv;
    o.col = v.col;
    return o;
}
)hlsl";

static constexpr char const* k_imgui_ps = R"hlsl(
Texture2D    u_tex  : register(t0);
SamplerState u_samp : register(s0);
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; float4 col : COLOR0; };
float4 main(PS_IN i) : SV_Target
{
    float4 c = i.col * u_tex.Sample(u_samp, i.uv);
    return float4(c.rgb * c.a, c.a);
}
)hlsl";

static constexpr char const* k_fs_vs = R"hlsl(
struct VS_OUT { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
VS_OUT main(uint id : SV_VertexID)
{
    float2 uv = float2( (id == 1) ? 2.0 : 0.0,
                        (id == 2) ? 2.0 : 0.0 );
    VS_OUT o;
    o.uv  = uv;
    o.pos = float4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, 0.0, 1.0);
    return o;
}
)hlsl";

static constexpr char const* k_grayscale_ps = R"hlsl(
Texture2D    u_tex  : register(t0);
SamplerState u_samp : register(s0);
cbuffer cb_gs : register(b0) { float u_strength; float3 _pad; };
struct PS_IN { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };
float4 main(PS_IN i) : SV_Target
{
    float4 c = u_tex.Sample(u_samp, i.uv);
    float a = max(c.a, 0.0001);
    float3 rgb = c.rgb / a;
    float luma = dot(rgb, float3(0.299, 0.587, 0.114));
    float s = saturate(u_strength);
    float3 mixed = lerp(rgb, float3(luma, luma, luma), s);
    return float4(mixed * c.a, c.a);
}
)hlsl";

void c_dx11_grayscale::rt_t::release( )
{
    if ( tex ) { tex->Release( ); tex = nullptr; }
    if ( rtv ) { rtv->Release( ); rtv = nullptr; }
    if ( srv ) { srv->Release( ); srv = nullptr; }
}

bool c_dx11_grayscale::init( ID3D11Device* dev, ID3D11DeviceContext* context, int w, int h )
{
    device = dev;
    ctx = context;
    width = w;
    height = h;

    if ( !create_shaders( ) )
        return false;
    if ( !create_states( ) )
        return false;
    return create_rt_src( w, h );
}

void c_dx11_grayscale::resize( int w, int h )
{
    if ( w <= 0 || h <= 0 )
        return;
    if ( w == width && h == height )
        return;
    release_rt_src( );
    width = w;
    height = h;
    create_rt_src( w, h );
}

void c_dx11_grayscale::destroy( )
{
    release_rt_src( );
    safe_release( vb );
    safe_release( ib );
    safe_release( vs_imgui );
    safe_release( il_imgui );
    safe_release( ps_imgui );
    safe_release( vs_fs );
    safe_release( ps_grayscale );
    safe_release( cb_proj );
    safe_release( cb_gs );
    safe_release( bs_premul );
    safe_release( bs_alpha );
    safe_release( ds_off );
    safe_release( rs_scissor );
    safe_release( rs_no_scissor );
    safe_release( sampler );
}

void c_dx11_grayscale::apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                               float strength )
{
    if ( !dl )
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

    dl->AddCallback( &c_dx11_grayscale::grayscale_callback, payload );
    dl->AddCallback( ImDrawCallback_ResetRenderState, nullptr );
}

void c_dx11_grayscale::add_image( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
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

void c_dx11_grayscale::add_image_rounded( ImDrawList* dl, ImTextureID tex, ImVec2 p_min, ImVec2 p_max,
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

ImVec4 c_dx11_grayscale::intersect_clips( ImVec4 const& a, ImVec4 const& b )
{
    return { std::max( a.x, b.x ), std::max( a.y, b.y ),
             std::min( a.z, b.z ), std::min( a.w, b.w ) };
}

ID3DBlob* c_dx11_grayscale::compile( char const* src, char const* entry, char const* target )
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

bool c_dx11_grayscale::make_cb( ID3D11Device* dev, UINT size, ID3D11Buffer** out )
{
    D3D11_BUFFER_DESC d { };
    d.ByteWidth = ( size + 15u ) & ~15u;
    d.Usage = D3D11_USAGE_DYNAMIC;
    d.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    d.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    return SUCCEEDED( dev->CreateBuffer( &d, nullptr, out ) );
}

template < typename T >
void c_dx11_grayscale::update_cb( ID3D11Buffer* cb, T const& data )
{
    D3D11_MAPPED_SUBRESOURCE mr { };
    if ( SUCCEEDED( ctx->Map( cb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr ) ) )
    {
        memcpy( mr.pData, &data, sizeof( T ) );
        ctx->Unmap( cb, 0 );
    }
}

void c_dx11_grayscale::set_full_viewport( )
{
    D3D11_VIEWPORT vp { };
    vp.Width = static_cast<float>(width);
    vp.Height = static_cast<float>(height);
    vp.MaxDepth = 1.f;
    ctx->RSSetViewports( 1, &vp );
}

bool c_dx11_grayscale::create_shaders( )
{
    {
        auto* blob = compile( k_imgui_vs, "main", "vs_4_0" );
        if ( !blob )
            return false;
        HRESULT hr = device->CreateVertexShader( blob->GetBufferPointer( ), blob->GetBufferSize( ), nullptr, &vs_imgui );
        if ( FAILED( hr ) )
        {
            blob->Release( );
            return false;
        }

        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof( ImDrawVert, pos )), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, static_cast<UINT>(offsetof( ImDrawVert, uv )), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, static_cast<UINT>(offsetof( ImDrawVert, col )), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };
        hr = device->CreateInputLayout( layout, 3, blob->GetBufferPointer( ), blob->GetBufferSize( ), &il_imgui );
        blob->Release( );
        if ( FAILED( hr ) )
            return false;
    }

    {
        auto* blob = compile( k_imgui_ps, "main", "ps_4_0" );
        if ( !blob )
            return false;
        HRESULT hr = device->CreatePixelShader( blob->GetBufferPointer( ), blob->GetBufferSize( ), nullptr, &ps_imgui );
        blob->Release( );
        if ( FAILED( hr ) )
            return false;
    }

    {
        auto* blob = compile( k_fs_vs, "main", "vs_4_0" );
        if ( !blob )
            return false;
        HRESULT hr = device->CreateVertexShader( blob->GetBufferPointer( ), blob->GetBufferSize( ), nullptr, &vs_fs );
        blob->Release( );
        if ( FAILED( hr ) )
            return false;
    }

    {
        auto* blob = compile( k_grayscale_ps, "main", "ps_4_0" );
        if ( !blob )
            return false;
        HRESULT hr = device->CreatePixelShader( blob->GetBufferPointer( ), blob->GetBufferSize( ), nullptr, &ps_grayscale );
        blob->Release( );
        if ( FAILED( hr ) )
            return false;
    }

    struct proj_buf_t { float m[ 4 ][ 4 ]; };
    struct gs_buf_t { float strength; float pad[ 3 ]; };

    if ( !make_cb( device, sizeof( proj_buf_t ), &cb_proj ) )
        return false;
    return make_cb( device, sizeof( gs_buf_t ), &cb_gs );
}

bool c_dx11_grayscale::create_states( )
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
        if ( FAILED( device->CreateBlendState( &d, &bs_premul ) ) )
            return false;
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
        d.ScissorEnable = TRUE;
        if ( FAILED( device->CreateRasterizerState( &d, &rs_scissor ) ) )
            return false;
        d.ScissorEnable = FALSE;
        if ( FAILED( device->CreateRasterizerState( &d, &rs_no_scissor ) ) )
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

bool c_dx11_grayscale::create_rt_src( int w, int h )
{
    rt_src.w = w;
    rt_src.h = h;
    D3D11_TEXTURE2D_DESC td { };
    td.Width = static_cast<UINT>(w);
    td.Height = static_cast<UINT>(h);
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    if ( FAILED( device->CreateTexture2D( &td, nullptr, &rt_src.tex ) ) )
        return false;
    if ( FAILED( device->CreateRenderTargetView( rt_src.tex, nullptr, &rt_src.rtv ) ) )
        return false;
    return SUCCEEDED( device->CreateShaderResourceView( rt_src.tex, nullptr, &rt_src.srv ) );
}

void c_dx11_grayscale::release_rt_src( )
{
    rt_src.release( );
}

void c_dx11_grayscale::ensure_buffers( int vtx_count, int idx_count )
{
    if ( vtx_count > vb_capacity )
    {
        safe_release( vb );
        vb_capacity = vtx_count + 1024;
        D3D11_BUFFER_DESC bd { };
        bd.ByteWidth = static_cast<UINT>(vb_capacity * sizeof( ImDrawVert ));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer( &bd, nullptr, &vb );
    }
    if ( idx_count > ib_capacity )
    {
        safe_release( ib );
        ib_capacity = idx_count + 2048;
        D3D11_BUFFER_DESC bd { };
        bd.ByteWidth = static_cast<UINT>(ib_capacity * sizeof( ImDrawIdx ));
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        device->CreateBuffer( &bd, nullptr, &ib );
    }
}

void c_dx11_grayscale::blit_to_rt( gs_cmd_t const& cmd, ImDrawList* dl, ID3D11RenderTargetView* dst_rtv, bool clear )
{
    int const vtx_count = cmd.vtx_end - cmd.vtx_start;
    int const idx_count = cmd.idx_end - cmd.idx_start;
    if ( vtx_count <= 0 || idx_count <= 0 )
        return;

    ensure_buffers( vtx_count, idx_count );

    D3D11_MAPPED_SUBRESOURCE mr { };
    if ( SUCCEEDED( ctx->Map( vb, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr ) ) )
    {
        memcpy( mr.pData, dl->VtxBuffer.Data + cmd.vtx_start, vtx_count * sizeof( ImDrawVert ) );
        ctx->Unmap( vb, 0 );
    }

    std::vector< ImDrawIdx > remapped( static_cast<size_t>(idx_count) );
    for ( int i = 0; i < idx_count; ++i )
        remapped[ i ] = dl->IdxBuffer.Data[ cmd.idx_start + i ] - static_cast<ImDrawIdx>(cmd.vtx_start);

    if ( SUCCEEDED( ctx->Map( ib, 0, D3D11_MAP_WRITE_DISCARD, 0, &mr ) ) )
    {
        memcpy( mr.pData, remapped.data( ), idx_count * sizeof( ImDrawIdx ) );
        ctx->Unmap( ib, 0 );
    }

    static constexpr float k_clear[ 4 ] { };
    if ( clear )
        ctx->ClearRenderTargetView( dst_rtv, k_clear );

    ctx->OMSetRenderTargets( 1, &dst_rtv, nullptr );
    ctx->OMSetDepthStencilState( ds_off, 0 );
    ctx->OMSetBlendState( bs_premul, nullptr, 0xffffffff );
    set_full_viewport( );

    struct proj_buf_t { float m[ 4 ][ 4 ]; };
    {
        float const L = 0.f, R = static_cast<float>(width);
        float const T = 0.f, B = static_cast<float>(height);
        proj_buf_t pb { };
        pb.m[ 0 ][ 0 ] = 2.f / ( R - L );
        pb.m[ 1 ][ 1 ] = 2.f / ( T - B );
        pb.m[ 2 ][ 2 ] = 0.5f;
        pb.m[ 3 ][ 0 ] = ( R + L ) / ( L - R );
        pb.m[ 3 ][ 1 ] = ( T + B ) / ( B - T );
        pb.m[ 3 ][ 2 ] = 0.5f;
        pb.m[ 3 ][ 3 ] = 1.f;
        update_cb( cb_proj, pb );
    }

    ctx->VSSetShader( vs_imgui, nullptr, 0 );
    ctx->VSSetConstantBuffers( 0, 1, &cb_proj );
    ctx->PSSetShader( ps_imgui, nullptr, 0 );
    ctx->PSSetSamplers( 0, 1, &sampler );
    ctx->IASetInputLayout( il_imgui );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );

    UINT stride = sizeof( ImDrawVert ), offset = 0;
    ctx->IASetVertexBuffers( 0, 1, &vb, &stride, &offset );
    ctx->IASetIndexBuffer( ib, sizeof( ImDrawIdx ) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT, 0 );

    ctx->RSSetState( rs_scissor );

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
                D3D11_RECT sr;
                sr.left = static_cast<LONG>(std::max( 0.f, clip.x ));
                sr.top = static_cast<LONG>(std::max( 0.f, clip.y ));
                sr.right = static_cast<LONG>(std::min( static_cast<float>(width), clip.z ));
                sr.bottom = static_cast<LONG>(std::min( static_cast<float>(height), clip.w ));

                if ( sr.right > sr.left && sr.bottom > sr.top )
                {
                    ctx->RSSetScissorRects( 1, &sr );
                    auto* srv = reinterpret_cast<ID3D11ShaderResourceView*>(static_cast<intptr_t>(dc.TextureId));
                    ctx->PSSetShaderResources( 0, 1, &srv );
                    int const local_off = overlap_s - cmd.idx_start;
                    int const local_count = overlap_e - overlap_s;
                    ctx->DrawIndexed( static_cast<UINT>(local_count), static_cast<UINT>(local_off), 0 );
                }
            }
        }

        running_idx += static_cast<int>(dc.ElemCount);
    }
}

void c_dx11_grayscale::composite( ID3D11ShaderResourceView* src_srv, ID3D11RenderTargetView* dst_rtv,
                                   gs_cmd_t const& cmd )
{
    ctx->OMSetRenderTargets( 1, &dst_rtv, nullptr );
    ctx->OMSetBlendState( bs_alpha, nullptr, 0xffffffff );
    ctx->OMSetDepthStencilState( ds_off, 0 );

    {
        D3D11_RECT sr;
        sr.left = static_cast<LONG>(std::max( 0.f, cmd.clip_rect.x ));
        sr.top = static_cast<LONG>(std::max( 0.f, cmd.clip_rect.y ));
        sr.right = static_cast<LONG>(std::min( static_cast<float>(width), cmd.clip_rect.z ));
        sr.bottom = static_cast<LONG>(std::min( static_cast<float>(height), cmd.clip_rect.w ));
        if ( sr.right <= sr.left || sr.bottom <= sr.top )
            return;
        ctx->RSSetState( rs_scissor );
        ctx->RSSetScissorRects( 1, &sr );
    }

    set_full_viewport( );

    ctx->VSSetShader( vs_fs, nullptr, 0 );
    ctx->PSSetShader( ps_grayscale, nullptr, 0 );
    ctx->IASetInputLayout( nullptr );
    ctx->IASetPrimitiveTopology( D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    ctx->PSSetSamplers( 0, 1, &sampler );
    ctx->PSSetShaderResources( 0, 1, &src_srv );

    struct gs_buf_t { float strength; float pad[ 3 ]; };
    gs_buf_t cb { std::clamp( cmd.strength, 0.f, 1.f ), { } };
    update_cb( cb_gs, cb );
    ctx->PSSetConstantBuffers( 0, 1, &cb_gs );
    ctx->Draw( 3, 0 );

    ID3D11ShaderResourceView* null_srv = nullptr;
    ctx->PSSetShaderResources( 0, 1, &null_srv );
}

void c_dx11_grayscale::render_grayscale( gs_cmd_t const& cmd, ImDrawList* dl, ID3D11RenderTargetView* back_rtv )
{
    if ( width <= 0 || height <= 0 )
        return;

    blit_to_rt( cmd, dl, rt_src.rtv, true );
    composite( rt_src.srv, back_rtv, cmd );
}

void c_dx11_grayscale::grayscale_callback( ImDrawList const*, ImDrawCmd const* draw_cmd )
{
    auto* data = static_cast< callback_data_t* >( draw_cmd->UserCallbackData );
    if ( !data || !data->renderer || !data->draw_list )
        return;

    auto* r = data->renderer;
    auto* dl = data->draw_list;
    gs_cmd_t cmd = data->cmd;
    cmd.clip_rect = draw_cmd->ClipRect;

    ID3D11RenderTargetView* back_rtv = nullptr;
    ID3D11DepthStencilView* back_dsv = nullptr;
    r->ctx->OMGetRenderTargets( 1, &back_rtv, &back_dsv );

    if ( back_rtv )
    {
        r->save_state( );
        r->render_grayscale( cmd, dl, back_rtv );
        r->restore_state( );
        back_rtv->Release( );
    }

    for ( int i = cmd.vtx_start; i < cmd.vtx_end; ++i )
        dl->VtxBuffer.Data[ i ].col &= 0x00FFFFFF;

    if ( back_dsv )
        back_dsv->Release( );

    delete data;
}

void c_dx11_grayscale::save_state( )
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
    ctx->IAGetVertexBuffers( 0, 1, &s.vb, &s.vb_stride, &s.vb_offset );
    ctx->IAGetIndexBuffer( &s.ib, &s.ib_format, &s.ib_offset );
    ctx->VSGetShader( &s.vs, s.vs_inst, &s.vs_inst_count );
    ctx->VSGetConstantBuffers( 0, 1, &s.vs_cb );
    ctx->PSGetShader( &s.ps, s.ps_inst, &s.ps_inst_count );
    ctx->PSGetConstantBuffers( 0, 1, &s.ps_cb );
    ctx->PSGetShaderResources( 0, 1, &s.ps_srv );
    ctx->PSGetSamplers( 0, 1, &s.ps_sampler );
}

void c_dx11_grayscale::restore_state( )
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
    ctx->IASetVertexBuffers( 0, 1, &s.vb, &s.vb_stride, &s.vb_offset );
    safe_release( s.vb );
    ctx->IASetIndexBuffer( s.ib, s.ib_format, s.ib_offset );
    safe_release( s.ib );
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
