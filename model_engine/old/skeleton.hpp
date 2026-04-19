#pragma once

#include "types.hpp"
#include "formats/glb/glb.hpp"

#include <array>
#include <initializer_list>
#include <string>
#include <vector>
#include <cstdint>

namespace alice
{
    struct bone_connection_t
    {
        int from;
        int to;
    };

    struct projected_bone_t
    {
        float x, y;
        bool visible;
    };

    class c_skeleton
    {
    public:
        struct joint_t
        {
            int parent = -1;
            std::string name;
            std::array< float, 3 > bind_translation { };
            std::array< float, 4 > bind_rotation { 0.f, 0.f, 0.f, 1.f };
            std::array< float, 3 > bind_scale { 1.f, 1.f, 1.f };
            std::array< float, 4 > anim_rotation { 0.f, 0.f, 0.f, 1.f };
            matrix4x4_t inverse_bind;
        };

        void build( glb::skin_data_t const& skin );

        void clear_anim( );
        void recompute( );

        [[nodiscard]] std::vector< matrix4x4_t > compute_skinning_matrices( ) const;
        std::vector< matrix4x4_t > const& compute_skinning_matrices_cached( );
        [[nodiscard]] std::vector< projected_bone_t > project(
            matrix4x4_t const& wvp, float viewport_w, float viewport_h ) const;

        [[nodiscard]] int find_joint( std::initializer_list< std::string > const& patterns ) const;
        void add_esp_link( int from, int to );

        [[nodiscard]] std::vector< joint_t >& mutable_joints( ) { return joints; }
        [[nodiscard]] std::vector< joint_t > const& get_joints( ) const { return joints; }
        [[nodiscard]] std::vector< matrix4x4_t > const& get_world_transforms( ) const { return world_transforms; }
        [[nodiscard]] std::vector< bone_connection_t > const& esp_connections( ) const { return esp_bones; }
        [[nodiscard]] int joint_count( ) const { return static_cast< int >( joints.size( ) ); }
        [[nodiscard]] bool ready( ) const { return !joints.empty( ); }

        static std::array< float, 4 > quat_from_euler( float rx, float ry, float rz );
        static std::array< float, 4 > quat_multiply( std::array< float, 4 > const& a, std::array< float, 4 > const& b );

    private:
        static matrix4x4_t quat_to_matrix( float qx, float qy, float qz, float qw );
        static matrix4x4_t compose_trs( std::array< float, 3 > const& t, std::array< float, 4 > const& r, std::array< float, 3 > const& s );

        std::vector< joint_t > joints;
        std::vector< matrix4x4_t > world_transforms;
        std::vector< matrix4x4_t > skinning_cache;
        std::vector< bone_connection_t > esp_bones;
    };

} // namespace alice
