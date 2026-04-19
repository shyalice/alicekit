#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <vector>
#include <functional>
#include <string>

namespace alice
{
    enum class esp_side : int
    {
        top = 0,
        bottom = 1,
        left = 2,
        right = 3
    };
    enum class esp_group
    {
        bar,
        text,
        box
    };
    enum class esp_align
    {
        start,
        center,
        end
    };

    struct esp_slot;

    using esp_measure_fn = std::function< ImVec2( ImRect const&, esp_side, float, void* ) >;
    // render args: (draw_list, bounds, side, cursor, alpha, vis_alpha, user)
    //   alpha     — caller-supplied opacity (e.g. distance fade)
    //   vis_alpha — slot's own enable/disable fade [0..1]; renderers may use it
    //               for both opacity AND geometric reveal (e.g. bar grow-from-bottom)
    using esp_render_fn = std::function< void( ImDrawList*, ImRect const&, esp_side, ImVec2, float, float, void* ) >;
    using esp_settings_fn = std::function< void( esp_slot& ) >;

    struct esp_slot
    {
        int id = 0;
        std::string name;
        esp_side side = esp_side::top;
        esp_group group = esp_group::bar;
        int order = 0;
        esp_align align = esp_align::center;

        esp_measure_fn measure;
        esp_render_fn render;
        esp_settings_fn settings;

        bool enabled = true;
        bool draggable = true;

        ImVec2 size = { 0.f, 0.f };
        ImVec2 flow_size = { 0.f, 0.f };
        ImVec2 target_pos = { 0.f, 0.f };

        bool anim_initialized = false;
        ImVec2 anim_pos = { 0.f, 0.f };
        float vis_alpha = 1.f;
        float hover_alpha = 0.f;
    };

    struct esp_side_padding
    {
        float box_to_bar = 2.f;
        float between_bars = 1.f;
        float box_to_text = 4.f;
        float bar_to_text = 1.f;
        float between_texts = 1.f;
    };

    class c_esp_layout
    {
    public:
        c_esp_layout( );

        void add_bar( int id, std::string name, esp_side side, int order,
                      esp_measure_fn measure, esp_render_fn render,
                      esp_align align = esp_align::center );
        void add_text( int id, std::string name, esp_side side, int order,
                       esp_measure_fn measure, esp_render_fn render,
                       esp_align align = esp_align::center );
        // Adds a non-draggable, layout-free slot whose render fn draws over the
        // whole bounds rect (e.g. a player box outline). It still appears in
        // `slots()` so it can be toggled like any other slot.
        void add_box( int id, std::string name, esp_render_fn render );

        void set_slot_settings( int slot_id, esp_settings_fn fn );

        void render( ImDrawList* dl, ImRect const& bounds, float alpha, void* user = nullptr, bool interactive = true );

        void render_static( ImDrawList* dl, ImRect const& bounds, float alpha, void* user = nullptr ) const;

        esp_side_padding& padding( esp_side side ) { return m_padding[ ( int ) side ]; }
        [[nodiscard]] const esp_side_padding& padding( esp_side side ) const { return m_padding[ ( int ) side ]; }

        esp_slot* find_slot( int id );
        [[nodiscard]] std::vector< esp_slot > const& slots( ) const { return m_slots; }
        std::vector< esp_slot >& slots( ) { return m_slots; }

        bool move_slot( int slot_id, esp_side target_side, int target_index_in_group );

        void set_enabled( int slot_id, bool enabled );
        [[nodiscard]] bool is_enabled( int slot_id ) const;

        [[nodiscard]] bool is_dragging( ) const { return m_dragging_id >= 0; }
        [[nodiscard]] int dragging_id( ) const { return m_dragging_id; }
        [[nodiscard]] int hovered_id( ) const { return m_hovered_id; }
        [[nodiscard]] bool dragging_over_delete( ) const { return m_dragging_over_delete; }

        void set_delete_zone( ImRect const& r )
        {
            m_delete_zone = r;
            m_has_delete_zone = true;
        }
        void clear_delete_zone( ) { m_has_delete_zone = false; }

    private:
        std::vector< esp_slot > m_slots;
        esp_side_padding m_padding[ 4 ];

        int m_dragging_id = -1;
        ImVec2 m_drag_grab_offset = { 0.f, 0.f };
        ImVec2 m_drag_prev_size = { 0.f, 0.f };
        bool m_dragging_over_delete = false;

        int m_hovered_id = -1;

        ImRect m_delete_zone = { };
        bool m_has_delete_zone = false;

        ImVec2 m_last_bounds_origin = { 0.f, 0.f };
        bool m_has_last_bounds = false;

        [[nodiscard]] ImVec2 compute_cursor( esp_side side, ImRect const& bounds,
                                             float accumulated, ImVec2 slot_size, esp_align align ) const;

        void hit_test_drop( ImRect const& bounds, ImVec2 mouse, esp_group drag_group,
                            esp_side& out_side, int& out_index );

        void compute_layout( ImRect const& bounds, float alpha, void* user,
                             std::vector< ImVec2 >& out_sizes,
                             std::vector< ImVec2 >& out_positions ) const;
    };

} // namespace alice
