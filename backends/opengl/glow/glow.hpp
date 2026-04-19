#pragma once

#include <glow/glow.hpp>

namespace alice
{
    class c_opengl_glow final : public c_glow
    {
    public:
        c_opengl_glow( ) = default;
        ~c_opengl_glow( ) override;

        bool init( int width, int height );
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

        struct callback_data_t
        {
            c_opengl_glow* renderer = nullptr;
            ImDrawList* draw_list = nullptr;
            glow_cmd_t cmd { };
        };

        struct saved_state_t
        {
            int draw_fbo = 0;
            int read_fbo = 0;
            int program = 0;
            int vao = 0;
            int array_buffer = 0;
            int element_buffer = 0;
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

        unsigned int prog_imgui = 0;
        unsigned int prog_blur = 0;
        unsigned int prog_composite = 0;
        unsigned int empty_vao = 0;
        unsigned int vao = 0;
        unsigned int vbo = 0;
        unsigned int ebo = 0;
        int vbo_capacity = 0;
        int ebo_capacity = 0;

        int loc_imgui_proj = -1;
        int loc_blur_radius = -1;
        int loc_blur_rt_size = -1;
        int loc_blur_horiz = -1;
        int loc_comp_intensity = -1;

        unsigned int fbo_src = 0;
        unsigned int tex_src = 0;
        unsigned int fbo_ping = 0;
        unsigned int tex_ping = 0;
        unsigned int fbo_pong = 0;
        unsigned int tex_pong = 0;
        unsigned int fbo_ping2 = 0;
        unsigned int tex_ping2 = 0;
        unsigned int fbo_pong2 = 0;
        unsigned int tex_pong2 = 0;
        int fbo_w = 0;
        int fbo_h = 0;
        int small_w = 0;
        int small_h = 0;

        saved_state_t saved { };
        bool shaders_ready = false;

        static ImVec4 intersect_clips( ImVec4 const& a, ImVec4 const& b );

        bool create_shaders( );
        bool create_fbo( unsigned int& out_fbo, unsigned int& out_tex, int w, int h );
        void release_fbos( );
        bool ensure_fbos( int w, int h );
        void ensure_buffers( int vtx_count, int idx_count );

        void blit_to_fbo( glow_cmd_t const& cmd, ImDrawList* dl, unsigned int dst_fbo, int w, int h, bool clear );
        void blur_pass( unsigned int src_tex, unsigned int fbo_h, unsigned int tex_h,
                        unsigned int fbo_v, unsigned int tex_v, int w, int h, float radius );
        void composite_pass( unsigned int src_tex, glow_cmd_t const& cmd, int w, int h, float intensity );

        void render_glow( glow_cmd_t const& cmd, ImDrawList* dl );

        void save_state( );
        void restore_state( );

        static void original_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
        static void glow_callback( ImDrawList const*, ImDrawCmd const* draw_cmd );
    };
} // namespace alice
