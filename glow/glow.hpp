#pragma once

#include "imgui.h"

namespace alice
{
    class c_glow
    {
    public:
        virtual ~c_glow( ) = default;

        virtual void resize( int w, int h ) = 0;

        virtual void apply( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end,
                            float radius, float intensity ) = 0;

        virtual void render_original( ImDrawList* dl, int vtx_start, int vtx_end, int idx_start, int idx_end ) = 0;
    };
} // namespace alice
