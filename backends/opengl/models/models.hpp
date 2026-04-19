#pragma once

#include "model_engine/model_engine.hpp"

#include <string>
#include <unordered_map>
#include <vector>

namespace alice
{
    class c_ogl_model_engine;

    class c_ogl_model final : public c_model
    {
        friend class c_ogl_model_engine;

    public:
        explicit c_ogl_model( c_ogl_model_engine* owner );
        ~c_ogl_model( ) override;

        c_ogl_model( c_ogl_model const& ) = delete;
        c_ogl_model& operator=( c_ogl_model const& ) = delete;

        bool load( std::string const& path ) override;
        void unload( ) override;
        void render( int width, int height ) override;

        [[nodiscard]] ImTextureID srv( ) const override { return static_cast< ImTextureID >( rtt_tex ); }
        [[nodiscard]] bool loaded( ) const override { return !mesh_groups.empty( ); }

    private:
        struct mesh_group_t
        {
            unsigned int vao = 0;
            unsigned int vbo = 0;
            unsigned int ebo = 0;
            unsigned int texture = 0;
            unsigned int sphere_tex = 0;
            unsigned int toon_tex = 0;
            int sphere_mode = 0;
            float diffuse_rgba[ 4 ] = { 1.f, 1.f, 1.f, 1.f };
            int index_count = 0;
        };

        bool create_rtt( int w, int h );

        struct morph_apply_t
        {
            unsigned int mesh_group_idx;
            unsigned int local_vertex_idx;
            float dx, dy, dz;
        };

        struct morph_info_t
        {
            std::string name;
            std::vector< morph_apply_t > applies;
            float applied_weight = 0.f;
        };

        c_ogl_model_engine* engine = nullptr;

        std::vector< mesh_group_t > mesh_groups;
        std::vector< std::vector< skinned_vertex_t > > base_vertices;
        std::vector< std::vector< skinned_vertex_t > > current_vertices;
        std::vector< morph_info_t > morphs;

        unsigned int bone_buffer = 0;
        unsigned int bone_tex = 0;
        unsigned int fbo = 0;
        unsigned int rtt_tex = 0;
        unsigned int depth_rbo = 0;
        int rtt_width = 0;
        int rtt_height = 0;
    };

    class c_ogl_model_engine final : public c_model_engine
    {
        friend class c_ogl_model;

    public:
        c_ogl_model_engine( );
        ~c_ogl_model_engine( ) override;

        c_ogl_model_engine( c_ogl_model_engine const& ) = delete;
        c_ogl_model_engine& operator=( c_ogl_model_engine const& ) = delete;

        bool init_pipeline( );

    protected:
        std::unique_ptr< c_model > make_model( ) override;

    private:
        unsigned int program = 0;
        int loc_wvp = -1;
        int loc_world_rot = -1;
        int loc_light_dir = -1;
        int loc_view_dir = -1;
        int loc_ambient = -1;
        int loc_rim_strength = -1;
        int loc_diffuse_color = -1;
        int loc_sphere_mode = -1;
        int loc_has_toon = -1;
        unsigned int white_tex = 0;
        unsigned int default_toon = 0;
        unsigned int shared_toon[ 10 ] = { };

        bool pipeline_ready = false;
    };

} // namespace alice
