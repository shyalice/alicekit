#pragma once

#include <blur/blur.hpp>

#include <d3d11.h>
#include <dxgi1_2.h>

namespace alice
{
    class c_dx11_blur final : public c_blur
    {
    public:
        bool init( ID3D11Device* dev, ID3D11DeviceContext* ctx, int w, int h );
        void destroy( );

        void resize( int w, int h ) override;

        bool init_duplication( ) override;
        void capture_desktop( ) override;

        void prepare_frame( float radius = 12.f, int passes = 2 ) override;
        void begin( ImDrawList* dl, float radius = 12.f, int passes = 2 ) override;

        void apply( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                    float radius = 12.f, float rounding = 0.f, int passes = 2, float alpha = 1.f ) override;
        void apply_squircle( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                              float radius = 12.f, float rounding = 0.f,
                              float exponent = 5.f, float arm_ratio = 1.f,
                              int passes = 2, float alpha = 1.f ) override;

        [[nodiscard]] ImTextureID get_texture( ) const override { return reinterpret_cast< ImTextureID >( rt_pong.srv ); }

        void uv_for( ImVec2 p_min, ImVec2 p_max, ImVec2& uv_min, ImVec2& uv_max ) const override;
        void add_image( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col = IM_COL32_WHITE ) const override;
        void add_image_rounded( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                 ImDrawFlags flags = ImDrawFlags_RoundCornersAll, ImU32 col = IM_COL32_WHITE ) const override;
        void add_image_squircle( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                  ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                                  float exponent = 5.f, float arm_ratio = 1.f,
                                  int segs_per_corner = 0, ImU32 col = IM_COL32_WHITE ) const override;

        bool ensure_rts( ID3D11RenderTargetView* back_rtv );

    private:
        enum class shape_e
        {
            rounded_box,
            squircle,
        };

        struct blur_cmd_t
        {
            ImVec2 min { };
            ImVec2 max { };
            ImVec2 clip_min { };
            ImVec2 clip_max { };
            float radius = 12.f;
            float rounding = 0.f;
            float exponent = 5.f;
            float arm_ratio = 1.f;
            float alpha = 1.f;
            int passes = 2;
            shape_e shape = shape_e::rounded_box;
            bool run_pipeline = true;
            bool composite = true;
        };

        struct rt_t
        {
            ID3D11Texture2D* tex = nullptr;
            ID3D11RenderTargetView* rtv = nullptr;
            ID3D11ShaderResourceView* srv = nullptr;

            void release( );
        };

        struct callback_data_t
        {
            c_dx11_blur* renderer = nullptr;
            blur_cmd_t cmd { };
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
            ID3D11VertexShader* vs = nullptr;
            UINT vs_inst_count = 256;
            ID3D11ClassInstance* vs_inst[ 256 ] { };
            ID3D11PixelShader* ps = nullptr;
            UINT ps_inst_count = 256;
            ID3D11ClassInstance* ps_inst[ 256 ] { };
            ID3D11Buffer* vs_cb = nullptr;
            ID3D11Buffer* ps_cb = nullptr;
            ID3D11ShaderResourceView* ps_srv = nullptr;
            ID3D11SamplerState* ps_sampler = nullptr;
        };

        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* ctx = nullptr;
        int width = 0;
        int height = 0;
        int small_w = 0;
        int small_h = 0;
        static constexpr int k_downsample = 2;

        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps_blur = nullptr;
        ID3D11PixelShader* ps_composite = nullptr;
        ID3D11PixelShader* ps_composite_squircle = nullptr;
        ID3D11PixelShader* ps_merge = nullptr;
        ID3D11Buffer* cb_blur = nullptr;
        ID3D11Buffer* cb_composite = nullptr;

        int last_frame_hash = -1;
        ID3D11BlendState* bs_alpha = nullptr;
        ID3D11DepthStencilState* ds_off = nullptr;
        ID3D11RasterizerState* rs = nullptr;
        ID3D11RasterizerState* rs_scissor = nullptr;
        ID3D11SamplerState* sampler = nullptr;

        rt_t rt_capture, rt_ping, rt_pong, rt_merged;
        saved_state_t saved { };

        IDXGIOutputDuplication* duplication = nullptr;
        rt_t rt_desktop;
        bool desktop_frame_valid = false;

        void destroy_duplication( );

        template < typename T >
        static void safe_release( T*& p )
        {
            if ( p )
            {
                p->Release( );
                p = nullptr;
            }
        }

        static ID3DBlob* compile( char const* src, char const* entry, char const* target );
        static bool make_cb( ID3D11Device* dev, UINT size, ID3D11Buffer** out );

        template < typename T >
        void update_cb( ID3D11Buffer* cb, T const& data );

        bool create_shaders( );
        bool create_states( );
        bool create_rt( int w, int h, DXGI_FORMAT fmt, rt_t& rt );
        void release_rts( );
        bool capture_backbuffer( ID3D11RenderTargetView* back_rtv );

        void set_viewport( int w, int h );
        void merge_pass( );
        void blur_pass( ID3D11ShaderResourceView* src, rt_t& h, rt_t& v, float radius );
        void composite( ID3D11RenderTargetView* back_rtv, blur_cmd_t const& cmd );
        void render_blur( blur_cmd_t const& cmd, ID3D11RenderTargetView* back_rtv );

        void save_state( );
        void restore_state( );

        static void blur_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
    };
} // namespace alice
