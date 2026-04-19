//
// Created by rei on 4/8/2026.
//

#pragma once

#include "imgui.h"

#include <vector>

namespace alice
{
    struct vtx_snapshot_t
    {
        ImDrawList* draw_list;
        int start;
    };

    class c_window_animator
    {
    public:
        c_window_animator( float duration = 0.15f, float step = 0.1f );

        void open( );
        void close( );
        void toggle( );
        void update( float dt );
        bool visible( ) const;
        float alpha( ) const;
        float scale( ) const;

        void set_pivot( ImVec2 pivot );

        void begin( );
        void capture( ImDrawList* draw_list );
        void end( ImVec2 window_pos, ImVec2 window_size );

    private:
        static float ease_out_cubic( float t );

        float m_time = 0.0f;
        float m_step = 0.1f;
        float m_duration = 0.15f;
        bool m_open = false;
        ImVec2 m_pivot = { 0.5f, 0.5f };
        std::vector< vtx_snapshot_t > m_snapshots;
    };
} // namespace alice