#include "types.hpp"

#include <cmath>

using alice::matrix4x4_t;

matrix4x4_t matrix4x4_t::identity( )
{
    matrix4x4_t r { };
    r.m[ 0 ][ 0 ] = 1.f;
    r.m[ 1 ][ 1 ] = 1.f;
    r.m[ 2 ][ 2 ] = 1.f;
    r.m[ 3 ][ 3 ] = 1.f;
    return r;
}

matrix4x4_t matrix4x4_t::rotation_x( float a )
{
    auto r = identity( );
    r.m[ 1 ][ 1 ] = std::cos( a );
    r.m[ 1 ][ 2 ] = std::sin( a );
    r.m[ 2 ][ 1 ] = -std::sin( a );
    r.m[ 2 ][ 2 ] = std::cos( a );
    return r;
}

matrix4x4_t matrix4x4_t::rotation_y( float a )
{
    auto r = identity( );
    r.m[ 0 ][ 0 ] = std::cos( a );
    r.m[ 0 ][ 2 ] = -std::sin( a );
    r.m[ 2 ][ 0 ] = std::sin( a );
    r.m[ 2 ][ 2 ] = std::cos( a );
    return r;
}

matrix4x4_t matrix4x4_t::translation( float x, float y, float z )
{
    auto r = identity( );
    r.m[ 3 ][ 0 ] = x;
    r.m[ 3 ][ 1 ] = y;
    r.m[ 3 ][ 2 ] = z;
    return r;
}

matrix4x4_t matrix4x4_t::scaling( float s )
{
    auto r = identity( );
    r.m[ 0 ][ 0 ] = s;
    r.m[ 1 ][ 1 ] = s;
    r.m[ 2 ][ 2 ] = s;
    return r;
}

matrix4x4_t matrix4x4_t::perspective( float fov_deg, float aspect, float z_near, float z_far )
{
    matrix4x4_t r { };
    float const t = 1.f / std::tan( fov_deg * 0.5f * ( 3.14159265f / 180.f ) );
    r.m[ 0 ][ 0 ] = t / aspect;
    r.m[ 1 ][ 1 ] = t;
    r.m[ 2 ][ 2 ] = z_far / ( z_far - z_near );
    r.m[ 2 ][ 3 ] = 1.f;
    r.m[ 3 ][ 2 ] = ( -z_near * z_far ) / ( z_far - z_near );
    return r;
}

matrix4x4_t matrix4x4_t::operator*( matrix4x4_t const& rhs ) const
{
    matrix4x4_t r { };
    for ( int i = 0; i < 4; ++i )
        for ( int j = 0; j < 4; ++j )
            for ( int k = 0; k < 4; ++k )
                r.m[ i ][ j ] += m[ i ][ k ] * rhs.m[ k ][ j ];
    return r;
}

matrix4x4_t matrix4x4_t::transposed( ) const
{
    matrix4x4_t r { };
    for ( int i = 0; i < 4; ++i )
        for ( int j = 0; j < 4; ++j )
            r.m[ i ][ j ] = m[ j ][ i ];
    return r;
}
