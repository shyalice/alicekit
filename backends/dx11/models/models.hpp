#pragma once

#include "model_engine/model_engine.hpp"
#include "renderer_fx/renderer_fx.hpp"

#include <array>
#include <d3d11.h>
#include <string>
#include <vector>

namespace alice
{
    class c_dx11_model_engine;

    class c_dx11_model final : public c_model
    {
        friend class c_dx11_model_engine;

    public:
        explicit c_dx11_model( c_dx11_model_engine* owner );
        ~c_dx11_model( ) override;

        c_dx11_model( c_dx11_model const& ) = delete;
        c_dx11_model& operator=( c_dx11_model const& ) = delete;

        bool load( std::string const& path ) override;
        void unload( ) override;

        void render( int width, int height ) override;

        [[nodiscard]] ImTextureID srv( ) const override { return reinterpret_cast< ImTextureID >( rtt_srv ); }
        [[nodiscard]] bool loaded( ) const override { return !mesh_groups.empty( ); }

    private:
        struct mesh_group_t
        {
            ID3D11Buffer* vbo = nullptr;
            ID3D11Buffer* ibo = nullptr;
            ID3D11ShaderResourceView* texture = nullptr;
            ID3D11ShaderResourceView* sphere_tex = nullptr;
            ID3D11ShaderResourceView* toon_tex = nullptr;
            int sphere_mode = 0;
            std::array< float, 4 > diffuse_rgba { 1.f, 1.f, 1.f, 1.f };
            int index_count = 0;
            int alpha_mode = 0;
            float alpha_cutoff = 0.1f;
        };

        struct morph_apply_t
        {
            unsigned int mesh_group_idx = 0;
            unsigned int local_vertex_idx = 0;
            float dx = 0.f;
            float dy = 0.f;
            float dz = 0.f;
        };

        struct morph_info_t
        {
            std::string name;
            std::vector< morph_apply_t > applies;
            float applied_weight = 0.f;
        };

        bool create_rtt( int w, int h );

        c_dx11_model_engine* engine = nullptr;

        ID3D11Buffer* bone_buffer = nullptr;

        ID3D11Texture2D* msaa_tex = nullptr;
        ID3D11RenderTargetView* rtv = nullptr;
        ID3D11DepthStencilView* dsv = nullptr;

        ID3D11Texture2D* resolve_tex = nullptr;
        ID3D11ShaderResourceView* rtt_srv = nullptr;

        int rtt_width = 0;
        int rtt_height = 0;

        std::vector< mesh_group_t > mesh_groups;
        std::vector< std::vector< skinned_vertex_t > > base_vertices;
        std::vector< std::vector< skinned_vertex_t > > current_vertices;
        std::vector< morph_info_t > morphs;
    };

    class c_dx11_model_engine final : public c_model_engine
    {
        friend class c_dx11_model;

    public:
        c_dx11_model_engine( ID3D11Device* device, ID3D11DeviceContext* ctx );
        ~c_dx11_model_engine( ) override;

        c_dx11_model_engine( c_dx11_model_engine const& ) = delete;
        c_dx11_model_engine& operator=( c_dx11_model_engine const& ) = delete;

        [[nodiscard]] ID3D11Device* device( ) const { return device_; }
        [[nodiscard]] ID3D11DeviceContext* context( ) const { return ctx_; }

        void set_fx( fx_settings_t const* p ) override { fx_settings_ptr = p; }
        [[nodiscard]] fx_settings_t const* fx( ) const { return fx_settings_ptr; }

    protected:
        std::unique_ptr< c_model > make_model( ) override;

    private:
        bool init_pipeline( );

        ID3D11Device* device_ = nullptr;
        ID3D11DeviceContext* ctx_ = nullptr;

        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11InputLayout* input_layout = nullptr;
        ID3D11Buffer* cb_scene = nullptr;
        ID3D11Buffer* cb_material = nullptr;
        ID3D11Buffer* cb_fx = nullptr;
        fx_settings_t const* fx_settings_ptr = nullptr;
        ID3D11SamplerState* sampler = nullptr;
        ID3D11SamplerState* sampler_clamp = nullptr;
        ID3D11ShaderResourceView* white_srv = nullptr;
        ID3D11ShaderResourceView* default_toon_srv = nullptr;
        std::array< ID3D11ShaderResourceView*, 10 > shared_toon_srv { };
        ID3D11RasterizerState* rasterizer = nullptr;
        ID3D11DepthStencilState* depth_state = nullptr;
        ID3D11DepthStencilState* depth_state_blend = nullptr;
        ID3D11BlendState* blend_state = nullptr;

        bool pipeline_ready = false;

        static constexpr UINT k_msaa_samples = 8;
        UINT msaa_quality = 0;
    };

} // namespace alice
