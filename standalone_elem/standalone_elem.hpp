//
// Created by rei on 4/9/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "popups/popups.hpp"

#include <optional>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

namespace alice
{
    class c_window_animator;

    struct toggle_info_t
    {
        ImVec4 pill;
        ImVec4 background;
        ImVec4 outline;

        float pos;
    };

    struct slider_info_t
    {
        ImVec4 track;
        ImVec4 fill;
        ImVec4 grab;
        ImVec4 text;

        ImVec4 input_bg;
        ImVec4 input_outline;

        float mod;
        float pos;
        float input_w { };

        char input_buf[ 64 ] { };
        bool input_was_active { };
    };

    struct dropdown_info_t
    {
        ImVec4 background;
        ImVec4 outline;
        ImVec4 text;

        c_popup popup;
        std::string random_title;
    };

    struct palette_entry_info_t
    {
        ImVec4 background { 0, 0, 0, 0 };
        ImVec4 select { 0, 0, 0, 0 };
        float alpha { 0.f };
    };

    struct color_info_t
    {
        ImVec4 hover;

        c_popup popup;

        ImVec4* first = nullptr;
        std::optional< ImVec4* > second;

        int selected_page_idx = 0;
        int display_page_idx = 0;
        float switch_t = 1.f;

        std::unordered_map< ImGuiID, palette_entry_info_t > palette_infos;
    };

    struct osx_tabs_info_t
    {
        struct entry_info_t
        {
            ImVec4 text;
            ImVec4 background;
        };

        std::unordered_map< ImGuiID, entry_info_t > entries_info;
    };

    struct simple_tabs_info_t
    {
        struct entry_info_t
        {
            ImVec4 background { 0, 0, 0, 0 };
            ImVec4 icon;
            ImVec4 text { 1, 1, 1, 0 };

            float mod = 0.f;
        };

        std::unordered_map< ImGuiID, entry_info_t > entries_info;
        float smooth_width = 0.f;
    };

    namespace std_elem
    {
        extern bool toggle( ImGuiID const& id, bool* v, toggle_info_t* info, ImRect const& rect, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );

        template < typename T >
        extern bool slider( ImGuiID const& id, slider_info_t* info, ImRect const& rect, T* v, T v_min, T v_max, char const* format = nullptr, bool has_input = true, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );

        extern bool begin_dropdown( ImGuiID const& id, ImRect const& rect, dropdown_info_t* info, std::string const& preview_value, int entries_amount, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );

        extern bool dropdown( ImGuiID const& id, dropdown_info_t* info, ImRect const& rect, int* v, std::vector< std::string > const& entries, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );

        using tab_badge_t = std::variant< std::string, ImVec4 >;

        extern void simple_tabs( ImGuiID const& id, simple_tabs_info_t* info, int* v, ImVec2 const& center, std::vector< std::pair< tab_badge_t, std::string > > const& entries, c_window_animator* animator = nullptr );

        extern void osx_tabs( ImGuiID const& id, osx_tabs_info_t* info, int* v, ImRect const& rect, std::vector< std::string > const& entries );

        extern void color_button( ImGuiID const& id, color_info_t* info, ImRect const& rect, std::pair< ImVec4*, std::optional< ImVec4* > > const& colors, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );
        extern void color_button( ImGuiID const& id, color_info_t* info, ImRect const& rect, ImVec4* color, const std::optional< std::pair< bool, float > >& disable_mod = std::nullopt );

        static void render_color_picker( ImGuiID const& id, color_info_t* info, ImVec4* color, std::optional< ImVec4* > secondary = std::nullopt );

    } // namespace std_elem
} // namespace alice