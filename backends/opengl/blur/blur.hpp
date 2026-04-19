#pragma once

#include <blur/blur.hpp>

namespace alice
{
    class c_ogl_blur final : public c_blur
    {
    public:
        c_ogl_blur( ) = default;
        ~c_ogl_blur( ) override;

        bool init( int width, int height );
        void destroy( );

        void resize( int w, int h ) override;

        bool init_duplication( ) override { return false; }
        void capture_desktop( ) override { }

        void prepare_frame( float radius = 12.f, int passes = 2 ) override;
        void begin( ImDrawList* dl, float radius = 12.f, int passes = 2 ) override;

        void apply( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                    float radius = 12.f, float rounding = 0.f, int passes = 2, float alpha = 1.f ) override;
        void apply_squircle( ImDrawList* dl, ImVec2 rect_min, ImVec2 rect_max,
                              float radius = 12.f, float rounding = 0.f,
                              float exponent = 5.f, float arm_ratio = 1.f,
                              int passes = 2, float alpha = 1.f ) override;

        [[nodiscard]] ImTextureID get_texture( ) const override;

        void uv_for( ImVec2 p_min, ImVec2 p_max, ImVec2& uv_min, ImVec2& uv_max ) const override;
        void add_image( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, ImU32 col = IM_COL32_WHITE ) const override;
        void add_image_rounded( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                 ImDrawFlags flags = ImDrawFlags_RoundCornersAll, ImU32 col = IM_COL32_WHITE ) const override;
        void add_image_squircle( ImDrawList* dl, ImVec2 p_min, ImVec2 p_max, float rounding,
                                  ImDrawFlags flags = ImDrawFlags_RoundCornersAll,
                                  float exponent = 5.f, float arm_ratio = 1.f,
                                  int segs_per_corner = 0, ImU32 col = IM_COL32_WHITE ) const override;

    private:
        struct blur_cmd_t
        {
            ImVec2 min { };
            ImVec2 max { };
            ImVec2 clip_min { };
            ImVec2 clip_max { };
            float radius = 12.f;
            float rounding = 0.f;
            float alpha = 1.f;
            int passes = 2;
        };

        struct callback_data_t
        {
            c_ogl_blur* renderer = nullptr;
            blur_cmd_t cmd { };
        };

        struct saved_state_t
        {
            int draw_fbo = 0;
            int read_fbo = 0;
            int program = 0;
            int vao = 0;
            int active_texture = 0;
            int texture_2d = 0;
            int viewport[ 4 ] = { };
            int scissor_box[ 4 ] = { };
            unsigned char blend = 0;
            unsigned char depth_test = 0;
            unsigned char scissor_test = 0;
            unsigned char cull_face = 0;
            int blend_src_rgb = 0;
            int blend_dst_rgb = 0;
            int blend_src_alpha = 0;
            int blend_dst_alpha = 0;
            int blend_eq_rgb = 0;
            int blend_eq_alpha = 0;
        };

        int width = 0;
        int height = 0;

        unsigned int prog_blur = 0;
        unsigned int prog_composite = 0;
        unsigned int empty_vao = 0;

        int loc_blur_radius = -1;
        int loc_blur_rt_size = -1;
        int loc_blur_horiz = -1;
        int loc_comp_rect_min = -1;
        int loc_comp_rect_max = -1;
        int loc_comp_rt_size = -1;
        int loc_comp_rounding = -1;
        int loc_comp_alpha = -1;

        unsigned int fbo_capture = 0;
        unsigned int tex_capture = 0;
        unsigned int fbo_ping = 0;
        unsigned int tex_ping = 0;
        unsigned int fbo_pong = 0;
        unsigned int tex_pong = 0;

        saved_state_t saved { };
        bool shaders_ready = false;

        bool create_shaders( );
        bool create_fbo( unsigned int& out_fbo, unsigned int& out_tex, int w, int h );
        void release_fbos( );
        bool ensure_fbos( int w, int h );

        void render_blur( blur_cmd_t const& cmd );
        void save_state( );
        void restore_state( );

        static void blur_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
    };

} // namespace alice
