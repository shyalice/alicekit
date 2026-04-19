#pragma once

#include <array>
#include <string_view>
#include <vector>

namespace alice
{
    class c_skeleton;
}

namespace alice::physics
{
    struct spring_bone_t
    {
        int joint = -1;
        std::array< float, 3 > rest_offset { };
        float rest_length = 0.f;
        std::array< float, 3 > curr_tail { };
        std::array< float, 3 > prev_tail { };
        float stiffness = 0.20f;
        float drag = 0.65f;
        float gravity = 0.0010f;
        bool use_world_rotation = false;
        bool initialized = false;
    };

    class c_spring_system
    {
    public:
        void bind( c_skeleton* skel );

        void add_bone( int joint_idx,
                       float stiffness = 0.20f,
                       float drag = 0.65f,
                       float gravity = 0.0010f,
                       bool world_rotation = false );

        void auto_detect( std::initializer_list< std::string_view > name_substrings,
                          float stiffness = 0.20f,
                          float drag = 0.65f,
                          float gravity = 0.0010f,
                          bool world_rotation = false );

        void set_world_rotation( float yaw, float pitch ) { model_yaw = yaw; model_pitch = pitch; }

        void update( );
        void clear( );

        [[nodiscard]] bool empty( ) const { return bones.empty( ); }

    private:
        c_skeleton* skel = nullptr;
        std::vector< spring_bone_t > bones;
        float model_yaw = 0.f;
        float model_pitch = 0.f;
        float prev_model_yaw = 0.f;
        float prev_model_pitch = 0.f;
    };
} // namespace alice::physics
