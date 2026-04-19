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

    for ( auto const& existing : bones )
        if ( existing.joint == joint_idx )
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
    if ( b.rest_length <= 0.f )
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

void c_spring_system::update( float delta_time )
{
    if ( !skel || bones.empty( ) )
        return;

    static constexpr float k_ref_fps = 60.f;
    static constexpr float k_ref_fps_sq = k_ref_fps * k_ref_fps;
    static constexpr float k_max_dt = 1.f / 20.f;

    float const dt = std::clamp( delta_time, 0.f, k_max_dt );
    if ( dt <= 0.f )
        return;

    auto& joints = skel->mutable_joints( );
    matrix4x4_t const identity = matrix4x4_t::identity( );

    matrix4x4_t const model_rot = matrix4x4_t::rotation_y( model_yaw )
                                  * matrix4x4_t::rotation_x( model_pitch );

    struct cached_collider_t
    {
        vec3 center;
        vec3 axis_half;
        float radius;
        uint8_t shape;
        int bone_index;
    };

    auto const& colliders_src = skel->get_colliders( );
    auto const& world_init = skel->get_world_transforms( );
    std::vector< cached_collider_t > cached_colliders;
    cached_colliders.reserve( colliders_src.size( ) );
    for ( auto const& c : colliders_src )
    {
        if ( c.bone_index < 0 || c.bone_index >= skel->joint_count( ) )
            continue;
        if ( c.shape != 0 && c.shape != 2 )
            continue;

        auto const& bm = world_init[ c.bone_index ];
        cached_collider_t cc;
        cc.bone_index = c.bone_index;
        cc.shape = c.shape;
        cc.radius = c.size[ 0 ];
        cc.center = {
            bm.m[ 0 ][ 0 ] * c.local_pos[ 0 ] + bm.m[ 1 ][ 0 ] * c.local_pos[ 1 ] + bm.m[ 2 ][ 0 ] * c.local_pos[ 2 ] + bm.m[ 3 ][ 0 ],
            bm.m[ 0 ][ 1 ] * c.local_pos[ 0 ] + bm.m[ 1 ][ 1 ] * c.local_pos[ 1 ] + bm.m[ 2 ][ 1 ] * c.local_pos[ 2 ] + bm.m[ 3 ][ 1 ],
            bm.m[ 0 ][ 2 ] * c.local_pos[ 0 ] + bm.m[ 1 ][ 2 ] * c.local_pos[ 1 ] + bm.m[ 2 ][ 2 ] * c.local_pos[ 2 ] + bm.m[ 3 ][ 2 ]
        };
        if ( c.shape == 2 )
        {
            float const half_h = c.size[ 1 ] * 0.5f;
            cc.axis_half = {
                bm.m[ 1 ][ 0 ] * half_h,
                bm.m[ 1 ][ 1 ] * half_h,
                bm.m[ 1 ][ 2 ] * half_h
            };
        }
        cached_colliders.push_back( cc );
    }

    for ( auto& b : bones )
    {
        if ( b.joint < 0 || b.joint >= skel->joint_count( ) )
            continue;

        auto const& world = skel->get_world_transforms( );

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
            b.velocity = { 0.f, 0.f, 0.f };
            b.initialized = true;
            continue;
        }

        float const drag_xz = b.drag;
        float const drag_y = b.use_world_rotation ? 0.35f : b.drag;
        float const stiff_xz = b.stiffness;
        float const stiff_y = b.use_world_rotation ? ( b.stiffness * 1.5f ) : b.stiffness;

        auto const damp_rate = []( float drag_ref ) {
            return -std::log( std::max( 1.f - drag_ref, 1e-4f ) ) * k_ref_fps;
        };

        float const damp_xz = damp_rate( drag_xz );
        float const damp_y = damp_rate( drag_y );
        float const omega2_xz = stiff_xz * k_ref_fps_sq;
        float const omega2_y = stiff_y * k_ref_fps_sq;
        float const grav_accel = b.gravity * k_ref_fps_sq;

        vec3 const accel {
            ( natural_tail[ 0 ] - b.curr_tail[ 0 ] ) * omega2_xz,
            ( natural_tail[ 1 ] - b.curr_tail[ 1 ] ) * omega2_y - grav_accel,
            ( natural_tail[ 2 ] - b.curr_tail[ 2 ] ) * omega2_xz
        };

        float const damp_xz_factor = std::exp( -damp_xz * dt );
        float const damp_y_factor = std::exp( -damp_y * dt );

        b.velocity[ 0 ] = b.velocity[ 0 ] * damp_xz_factor + accel[ 0 ] * dt;
        b.velocity[ 1 ] = b.velocity[ 1 ] * damp_y_factor + accel[ 1 ] * dt;
        b.velocity[ 2 ] = b.velocity[ 2 ] * damp_xz_factor + accel[ 2 ] * dt;

        vec3 new_tail {
            b.curr_tail[ 0 ] + b.velocity[ 0 ] * dt,
            b.curr_tail[ 1 ] + b.velocity[ 1 ] * dt,
            b.curr_tail[ 2 ] + b.velocity[ 2 ] * dt
        };

        if ( !b.use_world_rotation && !cached_colliders.empty( ) )
        {
            std::array< int, 8 > ancestors { b.joint, -1, -1, -1, -1, -1, -1, -1 };
            int anc = joints[ b.joint ].parent;
            for ( int k = 1; k < 8 && anc >= 0; ++k )
            {
                ancestors[ k ] = anc;
                anc = joints[ anc ].parent;
            }

            auto is_self_chain = [ & ]( int bi ) {
                for ( int a : ancestors )
                    if ( a == bi )
                        return true;
                return false;
            };

            float const broad_reach = b.rest_length + 0.5f;

            for ( int pass = 0; pass < 2; ++pass )
            {
                for ( auto const& cc : cached_colliders )
                {
                    if ( is_self_chain( cc.bone_index ) )
                        continue;

                    vec3 const to_center = sub( cc.center, head );
                    float const broad_d2 = dot( to_center, to_center );
                    float const reach = broad_reach + cc.radius;
                    if ( broad_d2 > reach * reach )
                        continue;

                    vec3 closest = cc.center;
                    if ( cc.shape == 2 )
                    {
                        vec3 const cap_a { cc.center[ 0 ] - cc.axis_half[ 0 ], cc.center[ 1 ] - cc.axis_half[ 1 ], cc.center[ 2 ] - cc.axis_half[ 2 ] };
                        vec3 const ab { cc.axis_half[ 0 ] * 2.f, cc.axis_half[ 1 ] * 2.f, cc.axis_half[ 2 ] * 2.f };
                        float const ab_len2 = dot( ab, ab );
                        float const t = ( ab_len2 > 1e-6f )
                            ? std::clamp( dot( sub( new_tail, cap_a ), ab ) / ab_len2, 0.f, 1.f )
                            : 0.f;
                        closest = { cap_a[ 0 ] + ab[ 0 ] * t,
                                    cap_a[ 1 ] + ab[ 1 ] * t,
                                    cap_a[ 2 ] + ab[ 2 ] * t };
                    }

                    vec3 const delta = sub( new_tail, closest );
                    float const d2 = dot( delta, delta );
                    if ( d2 < cc.radius * cc.radius && d2 > 1e-4f )
                    {
                        float const d = std::sqrt( d2 );
                        float const inv_d = 1.f / d;
                        new_tail = { closest[ 0 ] + delta[ 0 ] * inv_d * cc.radius,
                                     closest[ 1 ] + delta[ 1 ] * inv_d * cc.radius,
                                     closest[ 2 ] + delta[ 2 ] * inv_d * cc.radius };
                    }
                }

                vec3 const dir = normalize_safe( sub( new_tail, head ) );
                new_tail = { head[ 0 ] + dir[ 0 ] * b.rest_length,
                             head[ 1 ] + dir[ 1 ] * b.rest_length,
                             head[ 2 ] + dir[ 2 ] * b.rest_length };
            }
        }

        float const inv_dt = 1.f / dt;
        b.velocity = {
            ( new_tail[ 0 ] - b.curr_tail[ 0 ] ) * inv_dt,
            ( new_tail[ 1 ] - b.curr_tail[ 1 ] ) * inv_dt,
            ( new_tail[ 2 ] - b.curr_tail[ 2 ] ) * inv_dt
        };
        b.curr_tail = new_tail;

        vec3 target_dir = sub( new_tail, head );
        if ( b.use_world_rotation )
            target_dir = rotate_by_world_rot_inv( model_rot, target_dir );
        vec3 const target_local = rotate_by_world_rot_inv( parent_m, target_dir );
        vec3 const target_local_n = normalize_safe( target_local );
        vec3 const rest_n = normalize_safe( b.rest_offset );

        joints[ b.joint ].anim_rotation = quat_from_to( rest_n, target_local_n );
        skel->recompute_single( b.joint );
    }
}

