#include "skeleton.hpp"

#include <cmath>

using alice::c_skeleton;
using alice::matrix4x4_t;

std::array< float, 4 > c_skeleton::quat_multiply( std::array< float, 4 > const& a, std::array< float, 4 > const& b )
{
    return {
        a[ 3 ] * b[ 0 ] + a[ 0 ] * b[ 3 ] + a[ 1 ] * b[ 2 ] - a[ 2 ] * b[ 1 ],
        a[ 3 ] * b[ 1 ] - a[ 0 ] * b[ 2 ] + a[ 1 ] * b[ 3 ] + a[ 2 ] * b[ 0 ],
        a[ 3 ] * b[ 2 ] + a[ 0 ] * b[ 1 ] - a[ 1 ] * b[ 0 ] + a[ 2 ] * b[ 3 ],
        a[ 3 ] * b[ 3 ] - a[ 0 ] * b[ 0 ] - a[ 1 ] * b[ 1 ] - a[ 2 ] * b[ 2 ],
    };
}

std::array< float, 4 > c_skeleton::quat_from_euler( float rx, float ry, float rz )
{
    float const cx = std::cos( rx * 0.5f ), sx = std::sin( rx * 0.5f );
    float const cy = std::cos( ry * 0.5f ), sy = std::sin( ry * 0.5f );
    float const cz = std::cos( rz * 0.5f ), sz = std::sin( rz * 0.5f );

    return {
        sx * cy * cz - cx * sy * sz,
        cx * sy * cz + sx * cy * sz,
        cx * cy * sz - sx * sy * cz,
        cx * cy * cz + sx * sy * sz,
    };
}

matrix4x4_t c_skeleton::quat_to_matrix( float qx, float qy, float qz, float qw )
{
    float const xx = qx * qx, yy = qy * qy, zz = qz * qz;
    float const xy = qx * qy, xz = qx * qz, yz = qy * qz;
    float const wx = qw * qx, wy = qw * qy, wz = qw * qz;

    auto r = matrix4x4_t::identity( );
    r.m[ 0 ][ 0 ] = 1.f - 2.f * ( yy + zz );
    r.m[ 0 ][ 1 ] = 2.f * ( xy + wz );
    r.m[ 0 ][ 2 ] = 2.f * ( xz - wy );
    r.m[ 1 ][ 0 ] = 2.f * ( xy - wz );
    r.m[ 1 ][ 1 ] = 1.f - 2.f * ( xx + zz );
    r.m[ 1 ][ 2 ] = 2.f * ( yz + wx );
    r.m[ 2 ][ 0 ] = 2.f * ( xz + wy );
    r.m[ 2 ][ 1 ] = 2.f * ( yz - wx );
    r.m[ 2 ][ 2 ] = 1.f - 2.f * ( xx + yy );
    return r;
}

matrix4x4_t c_skeleton::compose_trs(
    std::array< float, 3 > const& t,
    std::array< float, 4 > const& r,
    std::array< float, 3 > const& s )
{
    auto const rot = quat_to_matrix( r[ 0 ], r[ 1 ], r[ 2 ], r[ 3 ] );
    auto result = matrix4x4_t::identity( );

    for ( int i = 0; i < 3; ++i )
        for ( int j = 0; j < 3; ++j )
            result.m[ i ][ j ] = rot.m[ i ][ j ] * s[ i ];

    result.m[ 3 ][ 0 ] = t[ 0 ];
    result.m[ 3 ][ 1 ] = t[ 1 ];
    result.m[ 3 ][ 2 ] = t[ 2 ];
    return result;
}

void c_skeleton::build( glb::skin_data_t const& skin )
{
    int const count = static_cast< int >( skin.joints.size( ) );
    joints.resize( count );
    world_transforms.resize( count );
    esp_bones.clear( );

    for ( int i = 0; i < count; ++i )
    {
        auto& j = joints[ i ];
        auto const& src = skin.joints[ i ];

        j.parent = src.parent;
        j.name = src.name;
        j.bind_translation = src.translation;
        j.bind_rotation = src.rotation;
        j.bind_scale = src.scale;
        j.anim_rotation = { 0.f, 0.f, 0.f, 1.f };

        if ( static_cast< std::size_t >( i * 16 + 15 ) < skin.inverse_bind_matrices.size( ) )
        {
            float const* src_m = &skin.inverse_bind_matrices[ static_cast< std::size_t >( i * 16 ) ];
            for ( int row = 0; row < 4; ++row )
                for ( int col = 0; col < 4; ++col )
                    j.inverse_bind.m[ row ][ col ] = src_m[ row * 4 + col ];
        }
        else
        {
            j.inverse_bind = matrix4x4_t::identity( );
        }
    }

    recompute( );
}

void c_skeleton::clear_anim( )
{
    for ( auto& j : joints )
        j.anim_rotation = { 0.f, 0.f, 0.f, 1.f };
}

void c_skeleton::recompute( )
{
    for ( int i = 0; i < static_cast< int >( joints.size( ) ); ++i )
    {
        auto const& j = joints[ i ];
        auto const final_rot = quat_multiply( j.bind_rotation, j.anim_rotation );
        auto const local = compose_trs( j.bind_translation, final_rot, j.bind_scale );

        if ( j.parent >= 0 && j.parent < i )
            world_transforms[ i ] = local * world_transforms[ j.parent ];
        else
            world_transforms[ i ] = local;
    }
}

std::vector< matrix4x4_t > c_skeleton::compute_skinning_matrices( ) const
{
    std::vector< matrix4x4_t > result( joints.size( ) );
    for ( std::size_t i = 0; i < joints.size( ); ++i )
        result[ i ] = joints[ i ].inverse_bind * world_transforms[ i ];
    return result;
}

std::vector< matrix4x4_t > const& c_skeleton::compute_skinning_matrices_cached( )
{
    skinning_cache.resize( joints.size( ) );
    for ( std::size_t i = 0; i < joints.size( ); ++i )
        skinning_cache[ i ] = joints[ i ].inverse_bind * world_transforms[ i ];
    return skinning_cache;
}

std::vector< alice::projected_bone_t > c_skeleton::project(
    matrix4x4_t const& wvp, float viewport_w, float viewport_h ) const
{
    std::vector< projected_bone_t > result( joints.size( ) );

    for ( std::size_t i = 0; i < joints.size( ); ++i )
    {
        float const wx = world_transforms[ i ].m[ 3 ][ 0 ];
        float const wy = world_transforms[ i ].m[ 3 ][ 1 ];
        float const wz = world_transforms[ i ].m[ 3 ][ 2 ];

        float cx = wx * wvp.m[ 0 ][ 0 ] + wy * wvp.m[ 1 ][ 0 ] + wz * wvp.m[ 2 ][ 0 ] + wvp.m[ 3 ][ 0 ];
        float cy = wx * wvp.m[ 0 ][ 1 ] + wy * wvp.m[ 1 ][ 1 ] + wz * wvp.m[ 2 ][ 1 ] + wvp.m[ 3 ][ 1 ];
        float const cw = wx * wvp.m[ 0 ][ 3 ] + wy * wvp.m[ 1 ][ 3 ] + wz * wvp.m[ 2 ][ 3 ] + wvp.m[ 3 ][ 3 ];

        if ( cw <= 0.001f )
        {
            result[ i ] = { 0.f, 0.f, false };
            continue;
        }

        float const inv_w = 1.f / cw;
        float const ndc_x = cx * inv_w;
        float const ndc_y = cy * inv_w;

        result[ i ] = {
            ( ndc_x * 0.5f + 0.5f ) * viewport_w,
            ( 1.f - ( ndc_y * 0.5f + 0.5f ) ) * viewport_h,
            true
        };
    }

    return result;
}

int c_skeleton::find_joint( std::initializer_list< std::string > const& patterns ) const
{
    for ( auto const& pat : patterns )
        for ( int i = 0; i < static_cast< int >( joints.size( ) ); ++i )
            if ( joints[ i ].name == pat )
                return i;

    for ( auto const& pat : patterns )
        for ( int i = 0; i < static_cast< int >( joints.size( ) ); ++i )
            if ( joints[ i ].name.find( pat ) != std::string::npos )
                return i;

    return -1;
}

void c_skeleton::add_esp_link( int from, int to )
{
    if ( from >= 0 && to >= 0 )
        esp_bones.push_back( { from, to } );
}
