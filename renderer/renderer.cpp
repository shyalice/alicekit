//
// Created by rei on 4/8/2026.
//

#include "renderer.hpp"

using alice::renderer_t;

static inline int squircle_segs( float r )
{
    return ImMax( 8, ( int ) ( r * 0.8f ) );
}

void renderer_t::path_squircle_corner( ImDrawList* dl, ImVec2 center,
                                       float rx, float ry,
                                       float sx, float sy,
                                       float exponent, int segs, bool reverse )
{
    int const start = reverse ? segs : 0;
    int const end = reverse ? 0 : segs;
    int const step = reverse ? -1 : 1;

    for ( int i = start; i != end + step; i += step )
    {
        float theta = ( float ) i / ( float ) segs * ( IM_PI * 0.5f );
        float dx = rx * powf( fabsf( cosf( theta ) ), 2.f / exponent );
        float dy = ry * powf( fabsf( sinf( theta ) ), 2.f / exponent );
        dl->PathLineTo( { center.x + sx * dx, center.y + sy * dy } );
    }
}

void renderer_t::build_squircle_path( ImDrawList* dl,
                                      ImVec2 p_min, ImVec2 p_max,
                                      float rounding, ImDrawFlags flags,
                                      float exponent, float arm_ratio, int segs )
{
    bool const tl = ( flags & ImDrawFlags_RoundCornersTopLeft ) != 0;
    bool const tr = ( flags & ImDrawFlags_RoundCornersTopRight ) != 0;
    bool const br = ( flags & ImDrawFlags_RoundCornersBottomRight ) != 0;
    bool const bl = ( flags & ImDrawFlags_RoundCornersBottomLeft ) != 0;

    // float const w    = ImAbs( p_max.x - p_min.x );
    // float const h    = ImAbs( p_max.y - p_min.y );
    // float const half = ImMin( w, h ) * 0.5f;
    //
    // float const depth = ImMin( rounding, half );
    // float const arm   = ImMin( rounding + ( half - rounding ) * ImSaturate( arm_ratio - 1.f ), half );

    float const half = ImMin( ImAbs( p_max.x - p_min.x ), ImAbs( p_max.y - p_min.y ) ) * 0.5f;
    float const arm = ImMin( rounding * arm_ratio, half );

    if ( tl )
        path_squircle_corner( dl, { p_min.x + arm, p_min.y + arm }, arm, arm, -1.f, -1.f, exponent, segs, false );
    else
        dl->PathLineTo( { p_min.x, p_min.y } );

    if ( tr )
        path_squircle_corner( dl, { p_max.x - arm, p_min.y + arm }, arm, arm, +1.f, -1.f, exponent, segs, true );
    else
        dl->PathLineTo( { p_max.x, p_min.y } );

    if ( br )
        path_squircle_corner( dl, { p_max.x - arm, p_max.y - arm }, arm, arm, +1.f, +1.f, exponent, segs, false );
    else
        dl->PathLineTo( { p_max.x, p_max.y } );

    if ( bl )
        path_squircle_corner( dl, { p_min.x + arm, p_max.y - arm }, arm, arm, -1.f, +1.f, exponent, segs, true );
    else
        dl->PathLineTo( { p_min.x, p_max.y } );
}

void renderer_t::add_rect_filled( ImDrawList* dl,
                                  ImVec2 p_min, ImVec2 p_max,
                                  ImU32 col, float rounding,
                                  ImDrawFlags flags,
                                  float exponent,
                                  float arm_ratio,
                                  int segs_per_corner )
{
    if ( ( col & IM_COL32_A_MASK ) == 0 )
        return;

    if ( rounding <= 0.f || ( flags & ImDrawFlags_RoundCornersAll ) == 0 )
    {
        dl->AddRectFilled( p_min, p_max, col, 0.f, flags );
        return;
    }

    p_min = { p_min.x, p_min.y };
    p_max = { p_max.x, p_max.y };

    if ( segs_per_corner <= 0 )
        segs_per_corner = squircle_segs( rounding );

    build_squircle_path( dl, p_min, p_max, rounding, flags, exponent, arm_ratio, segs_per_corner );
    dl->PathFillConvex( col );
}

void renderer_t::add_rect( ImDrawList* dl,
                           ImVec2 p_min, ImVec2 p_max,
                           ImU32 col, float thickness, float rounding,
                           ImDrawFlags flags,
                           float exponent,
                           float arm_ratio,
                           int segs_per_corner )
{
    if ( ( col & IM_COL32_A_MASK ) == 0 )
        return;

    if ( rounding <= 0.f || ( flags & ImDrawFlags_RoundCornersAll ) == 0 )
    {
        dl->AddRect( p_min, p_max, col, 0.f, flags, thickness );
        return;
    }

    float const inset = thickness * 0.5f;

    p_min = { p_min.x + inset, p_min.y + inset };
    p_max = { p_max.x - inset, p_max.y - inset };
    rounding = rounding - inset;

    if ( segs_per_corner <= 0 )
        segs_per_corner = squircle_segs( rounding );

    build_squircle_path( dl, p_min, p_max, rounding, flags, exponent, arm_ratio, segs_per_corner );
    dl->PathStroke( col, ImDrawFlags_Closed, thickness );
}

void renderer_t::add_shadow( ImDrawList* dl,
                              ImVec2 p_min, ImVec2 p_max,
                              ImU32 col, float rounding,
                              float radius,
                              ImDrawFlags flags,
                              float exponent,
                              float arm_ratio,
                              int steps )
{
    if ( ( col & IM_COL32_A_MASK ) == 0 || radius <= 0.f )
        return;

    float const aa     = dl->_FringeScale;
    float const base_a = ( float ) ( ( col >> IM_COL32_A_SHIFT ) & 0xFF ) / 255.f;
    ImU32 const rgb    = col & ~IM_COL32_A_MASK;

    for ( int i = steps; i >= 1; i-- )
    {
        float t      = ( float ) ( i - 1 ) / ( float ) ( steps - 1 );
        float expand = radius * t;

        float alpha  = base_a * powf( 1.f - t, 2.5f );
        if ( alpha < 0.002f ) continue;

        ImU32 step_col = rgb | ( ( ImU32 ) ( alpha * 255.f ) << IM_COL32_A_SHIFT );

        ImVec2 s_min = { p_min.x - expand + aa, p_min.y - expand + aa };
        ImVec2 s_max = { p_max.x + expand - aa, p_max.y + expand - aa };
        float  s_r   = rounding + expand;

        build_squircle_path( dl, s_min, s_max, s_r, flags, exponent, arm_ratio, squircle_segs( s_r ) );
        dl->PathFillConvex( step_col );
    }
}