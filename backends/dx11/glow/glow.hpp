#pragma once

#include <glow/glow.hpp>

#include <d3d11.h>

namespace alice
{
    class c_dx11_glow final : public c_glow
    {
    public:
        bool init( ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h );
        void destroy( );

        void resize( int w, int h ) override;

        void apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                    float radius, float intensity ) override;

        void render_original( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end ) override;

    private:
        struct glow_cmd_t
        {
            int vtx_start = 0;
            int vtx_end = 0;
            int idx_start = 0;
            int idx_end = 0;
            float radius = 8.f;
            float intensity = 2.f;
            ImVec4 clip_rect { };
        };

        struct rt_t
        {
            ID3D11Texture2D* tex = nullptr;
            ID3D11RenderTargetView* rtv = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;
            int w = 0;
            int h = 0;

            void release( );
        };

        struct callback_data_t
        {
            c_dx11_glow* renderer = nullptr;
            ImDrawList* draw_list = nullptr;
            glow_cmd_t cmd { };
        };

        struct saved_state_t
        {
            ID3D11RasterizerState* rs = nullptr;
            UINT scissor_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            D3D11_RECT scissors[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ] { };
            UINT vp_count = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
            D3D11_VIEWPORT vps[ D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE ] { };
            ID3D11BlendState* bs = nullptr;
            FLOAT blend_factor[ 4 ] { };
            UINT sample_mask = 0;
            ID3D11DepthStencilState* ds = nullptr;
            UINT stencil_ref = 0;
            ID3D11RenderTargetView* rtv = nullptr;
            ID3D11DepthStencilView* dsv = nullptr;
            D3D11_PRIMITIVE_TOPOLOGY topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
            ID3D11InputLayout* il = nullptr;
            ID3D11Buffer* vb = nullptr;
            UINT vb_stride = 0;
            UINT vb_offset = 0;
            ID3D11Buffer* ib = nullptr;
            DXGI_FORMAT ib_format = DXGI_FORMAT_UNKNOWN;
            UINT ib_offset = 0;
            ID3D11VertexShader* vs = nullptr;
            ID3D11Buffer* vs_cb = nullptr;
            UINT vs_inst_count = 256;
            ID3D11ClassInstance* vs_inst[ 256 ] { };
            ID3D11PixelShader* ps = nullptr;
            ID3D11Buffer* ps_cb = nullptr;
            ID3D11ShaderResourceView* ps_srv = nullptr;
            ID3D11SamplerState* ps_sampler = nullptr;
            UINT ps_inst_count = 256;
            ID3D11ClassInstance* ps_inst[ 256 ] { };
        };

        ID3D11Buffer* vb = nullptr;
        ID3D11Buffer* ib = nullptr;
        int vb_capacity = 0;
        int ib_capacity = 0;

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* ctx = nullptr;
        int width = 0;
        int height = 0;

        ID3D11VertexShader* vs_imgui = nullptr;
        ID3D11InputLayout* il_imgui = nullptr;
        ID3D11PixelShader* ps_imgui = nullptr;
        ID3D11VertexShader* vs_fs = nullptr;
        ID3D11PixelShader* ps_blur = nullptr;
        ID3D11PixelShader* ps_composite = nullptr;

        ID3D11Buffer* cb_proj = nullptr;
        ID3D11Buffer* cb_blur = nullptr;
        ID3D11Buffer* cb_composite = nullptr;

        ID3D11BlendState* bs_premul = nullptr;
        ID3D11BlendState* bs_additive = nullptr;
        ID3D11DepthStencilState* ds_off = nullptr;
        ID3D11RasterizerState* rs_scissor = nullptr;
        ID3D11RasterizerState* rs_no_scissor = nullptr;
        ID3D11SamplerState* sampler = nullptr;

        rt_t rt_src, rt_ping, rt_pong, rt_ping2, rt_pong2;

        saved_state_t saved { };

        template < typename T >
        static void safe_release( T*& p )
        {
            if ( p )
            {
                p->Release( );
                p = nullptr;
            }
        }

        static ImVec4 intersect_clips( ImVec4 const& a, ImVec4 const& b );
        static ID3DBlob* compile( char const* src, char const* entry, char const* target );
        static bool make_cb( ID3D11Device* dev, UINT size, ID3D11Buffer** out );

        template < typename T >
        void update_cb( ID3D11Buffer* cb, T const& data );

        void ensure_buffers( int vtx_count, int idx_count );
        void set_full_viewport( );

        bool create_shaders( );
        bool create_states( );
        bool create_rt( int w, int h, rt_t& rt );
        bool create_rts( int w, int h );
        void release_rts( );

        void blit_to_rt( glow_cmd_t const& cmd, ImDrawList* dl, ID3D11RenderTargetView* dst_rtv, bool clear );
        void blur_pass( ID3D11ShaderResourceView* src_srv, rt_t& rt_h, rt_t& rt_v, float radius );
        void composite( ID3D11ShaderResourceView* src_srv, ID3D11RenderTargetView* dst_rtv,
                        glow_cmd_t const& cmd, float intensity );
        void render_glow( glow_cmd_t const& cmd, ImDrawList* dl, ID3D11RenderTargetView* back_rtv );

        void save_state( );
        void restore_state( );

        static void original_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
        static void glow_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
    };
} // namespace alice
