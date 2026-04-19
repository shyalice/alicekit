#include "spring.hpp"
#include "model_engine/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

using alice::c_skeleton;
using alice::matrix4x4_t;
using alice::physics::c_spring_system;
using alice::physics::spring_bone_t;

namespace
{
    using vec3 = std::array< float, 3 >;
    using quat = std::array< float, 4 >;

    vec3 sub( vec3 const& a, vec3 const& b )
    {
        return { a[ 0 ] - b[ 0 ], a[ 1 ] - b[ 1 ], a[ 2 ] - b[ 2 ] };
    }

    float dot( vec3 const& a, vec3 const& b )
    {
        return a[ 0 ] * b[ 0 ] + a[ 1 ] * b[ 1 ] + a[ 2 ] * b[ 2 ];
    }

    vec3 cross( vec3 const& a, vec3 const& b )
    {
        return {
            a[ 1 ] * b[ 2 ] - a[ 2 ] * b[ 1 ],
            a[ 2 ] * b[ 0 ] - a[ 0 ] * b[ 2 ],
            a[ 0 ] * b[ 1 ] - a[ 1 ] * b[ 0 ]
        };
    }

    float length( vec3 const& v )
    {
        return std::sqrt( dot( v, v ) );
    }

    vec3 normalize_safe( vec3 const& v )
    {
        float const l = length( v );
        if ( l < 1e-6f )
            return { 0.f, 1.f, 0.f };
        return { v[ 0 ] / l, v[ 1 ] / l, v[ 2 ] / l };
    }

    vec3 rotate_by_world_rot( matrix4x4_t const& m, vec3 const& v )
    {
        return {
            v[ 0 ] * m.m[ 0 ][ 0 ] + v[ 1 ] * m.m[ 1 ][ 0 ] + v[ 2 ] * m.m[ 2 ][ 0 ],
            v[ 0 ] * m.m[ 0 ][ 1 ] + v[ 1 ] * m.m[ 1 ][ 1 ] + v[ 2 ] * m.m[ 2 ][ 1 ],
            v[ 0 ] * m.m[ 0 ][ 2 ] + v[ 1 ] * m.m[ 1 ][ 2 ] + v[ 2 ] * m.m[ 2 ][ 2 ]
        };
    }

    vec3 rotate_by_world_rot_inv( matrix4x4_t const& m, vec3 const& v )
    {
        return {
            v[ 0 ] * m.m[ 0 ][ 0 ] + v[ 1 ] * m.m[ 0 ][ 1 ] + v[ 2 ] * m.m[ 0 ][ 2 ],
            v[ 0 ] * m.m[ 1 ][ 0 ] + v[ 1 ] * m.m[ 1 ][ 1 ] + v[ 2 ] * m.m[ 1 ][ 2 ],
            v[ 0 ] * m.m[ 2 ][ 0 ] + v[ 1 ] * m.m[ 2 ][ 1 ] + v[ 2 ] * m.m[ 2 ][ 2 ]
        };
    }

    quat quat_from_to( vec3 const& a, vec3 const& b )
    {
        float const d = dot( a, b );
        if ( d > 0.999999f )
            return { 0.f, 0.f, 0.f, 1.f };

        if ( d < -0.999999f )
        {
            vec3 axis = ( std::fabs( a[ 0 ] ) < 0.9f )
                ? cross( a, { 1.f, 0.f, 0.f } )
                : cross( a, { 0.f, 1.f, 0.f } );
            axis = normalize_safe( axis );
            return { axis[ 0 ], axis[ 1 ], axis[ 2 ], 0.f };
        }

        vec3 const c = cross( a, b );
        float const s = std::sqrt( ( 1.f + d ) * 2.f );
        float const inv_s = 1.f / s;
        return { c[ 0 ] * inv_s, c[ 1 ] * inv_s, c[ 2 ] * inv_s, s * 0.5f };
    }
} // namespace

void c_spring_system::bind( c_skeleton* s )
{
    skel = s;
    clear( );
}

void c_spring_system::clear( )
{
    bones.clear( );
}

void c_spring_system::add_bone( int joint_idx, float stiffness, float drag, float gravity, bool world_rotation )
{
    if ( !skel || joint_idx < 0 || joint_idx >= skel->joint_count( ) )
        return;

    auto const& joints = skel->get_joints( );

    int child = -1;
    for ( int j = 0; j < skel->joint_count( ); ++j )
    {
        if ( joints[ j ].parent == joint_idx )
        {
            child = j;
            break;
        }
    }

    spring_bone_t b { };
    b.joint = joint_idx;
    if ( child >= 0 )
    {
        b.rest_offset = joints[ child ].bind_translation;
    }
    else
    {
        b.rest_offset = joints[ joint_idx ].bind_translation;
    }

    b.rest_length = length( b.rest_offset );
    if ( b.rest_length < 0.01f )
        return;

    b.stiffness = stiffness;
    b.drag = drag;
    b.gravity = gravity;
    b.use_world_rotation = world_rotation;
    bones.push_back( b );
}

void c_spring_system::auto_detect( std::initializer_list< std::string_view > patterns,
                                   float stiffness, float drag, float gravity, bool world_rotation )
{
    if ( !skel )
        return;

    auto const& joints = skel->get_joints( );

    static constexpr std::string_view k_weight_suffix = "\xe9\x8c\x98";

    for ( int i = 0; i < skel->joint_count( ); ++i )
    {
        auto const& name = joints[ i ].name;

        if ( name.size( ) >= k_weight_suffix.size( )
             && name.compare( name.size( ) - k_weight_suffix.size( ), k_weight_suffix.size( ), k_weight_suffix ) == 0 )
            continue;

        for ( auto const& pat : patterns )
        {
            if ( name.find( pat ) != std::string::npos )
            {
                add_bone( i, stiffness, drag, gravity, world_rotation );
                break;
            }
        }
    }
}

void c_spring_system::update( )
{
    if ( !skel || bones.empty( ) )
        return;

    auto& joints = skel->mutable_joints( );
    auto const& world = skel->get_world_transforms( );
    matrix4x4_t const identity = matrix4x4_t::identity( );

    matrix4x4_t const model_rot = matrix4x4_t::rotation_y( model_yaw )
                                  * matrix4x4_t::rotation_x( model_pitch );

    for ( auto& b : bones )
    {
        if ( b.joint < 0 || b.joint >= skel->joint_count( ) )
            continue;

        int const parent = joints[ b.joint ].parent;
        matrix4x4_t const& parent_m = ( parent >= 0 && parent < skel->joint_count( ) )
            ? world[ parent ]
            : identity;

        vec3 const head_skel { world[ b.joint ].m[ 3 ][ 0 ],
                               world[ b.joint ].m[ 3 ][ 1 ],
                               world[ b.joint ].m[ 3 ][ 2 ] };

        vec3 const natural_dir_skel = rotate_by_world_rot( parent_m, b.rest_offset );
        vec3 const natural_tail_skel { head_skel[ 0 ] + natural_dir_skel[ 0 ],
                                       head_skel[ 1 ] + natural_dir_skel[ 1 ],
                                       head_skel[ 2 ] + natural_dir_skel[ 2 ] };

        vec3 const head = b.use_world_rotation
            ? rotate_by_world_rot( model_rot, head_skel )
            : head_skel;
        vec3 const natural_tail = b.use_world_rotation
            ? rotate_by_world_rot( model_rot, natural_tail_skel )
            : natural_tail_skel;

        if ( !b.initialized )
        {
            b.curr_tail = natural_tail;
            b.prev_tail = natural_tail;
            b.initialized = true;
            continue;
        }

        float const vdrag = b.use_world_rotation ? 0.35f : b.drag;
        float const vstiff = b.use_world_rotation ? ( b.stiffness * 1.5f ) : b.stiffness;

        vec3 vel {
            ( b.curr_tail[ 0 ] - b.prev_tail[ 0 ] ) * ( 1.f - b.drag ),
            ( b.curr_tail[ 1 ] - b.prev_tail[ 1 ] ) * ( 1.f - vdrag ),
            ( b.curr_tail[ 2 ] - b.prev_tail[ 2 ] ) * ( 1.f - b.drag )
        };

        vel[ 1 ] -= b.gravity;

        vec3 const spring_force {
            ( natural_tail[ 0 ] - b.curr_tail[ 0 ] ) * b.stiffness,
            ( natural_tail[ 1 ] - b.curr_tail[ 1 ] ) * vstiff,
            ( natural_tail[ 2 ] - b.curr_tail[ 2 ] ) * b.stiffness
        };

        vec3 new_tail {
            b.curr_tail[ 0 ] + vel[ 0 ] + spring_force[ 0 ],
            b.curr_tail[ 1 ] + vel[ 1 ] + spring_force[ 1 ],
            b.curr_tail[ 2 ] + vel[ 2 ] + spring_force[ 2 ]
        };

        if ( !b.use_world_rotation )
        {
            vec3 const dir = normalize_safe( sub( new_tail, head ) );
            new_tail = { head[ 0 ] + dir[ 0 ] * b.rest_length,
                         head[ 1 ] + dir[ 1 ] * b.rest_length,
                         head[ 2 ] + dir[ 2 ] * b.rest_length };
        }

        b.prev_tail = b.curr_tail;
        b.curr_tail = new_tail;

        vec3 target_dir = sub( new_tail, head );
        if ( b.use_world_rotation )
            target_dir = rotate_by_world_rot_inv( model_rot, target_dir );
        vec3 const target_local = rotate_by_world_rot_inv( parent_m, target_dir );
        vec3 const target_local_n = normalize_safe( target_local );
        vec3 const rest_n = normalize_safe( b.rest_offset );

        joints[ b.joint ].anim_rotation = quat_from_to( rest_n, target_local_n );
    }
}
