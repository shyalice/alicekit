#pragma once

#include <cstdint>

namespace alice
{
    struct matrix4x4_t
    {
        float m[ 4 ][ 4 ] { };

        static matrix4x4_t identity( );
        static matrix4x4_t rotation_x( float angle );
        static matrix4x4_t rotation_y( float angle );
        static matrix4x4_t translation( float x, float y, float z );
        static matrix4x4_t scaling( float s );
        static matrix4x4_t perspective( float fov_deg, float aspect, float z_near, float z_far );

        matrix4x4_t operator*( matrix4x4_t const& rhs ) const;
        matrix4x4_t transposed( ) const;
    };

    struct skinned_vertex_t
    {
        float x, y, z;
        float nx, ny, nz;
        float u, v;
        uint16_t joints[ 4 ];
        float weights[ 4 ];
    };

    struct rotation_limits_t
    {
        float min_pitch = -0.6f;
        float max_pitch = 0.4f;
        bool clamp_yaw = false;
        float min_yaw = -3.14159f;
        float max_yaw = 3.14159f;
    };
} // namespace alice
