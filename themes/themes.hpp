//
// Created by rei on 4/9/2026.
//

#pragma once

#include "imgui.h"

#include <filesystem>
#include <string>
#include <vector>

namespace alice
{
    constexpr ImVec4 rgb( int r, int g, int b, int a = 255 )
    {
        return { r / 255.f, g / 255.f, b / 255.f, a / 255.f };
    }

    constexpr ImVec4 gray( int v, int a = 255 )
    {
        return rgb( v, v, v, a );
    }

    constexpr ImVec4 with_alpha( ImVec4 const& c, float a )
    {
        return { c.x, c.y, c.z, c.w * a };
    }

    struct theme_palette_t
    {
        std::string name;
        bool builtin = false;

        ImVec4 window_bg;
        ImVec4 content_bg;
        ImVec4 title_bg;
        ImVec4 tab_bg;
        ImVec4 header_bg;
        ImVec4 card_bg;
        ImVec4 popup_bg;
        ImVec4 notification_bg;
        ImVec4 notification_border;

        ImVec4 control;
        ImVec4 control_hover;
        ImVec4 control_active;
        ImVec4 control_selected;
        ImVec4 control_subtle;

        ImVec4 border;
        ImVec4 border_subtle;
        ImVec4 border_strong;
        ImVec4 border_hover;
        ImVec4 border_active;
        ImVec4 divider;

        ImVec4 text;
        ImVec4 text_bright;
        ImVec4 text_secondary;
        ImVec4 text_light;
        ImVec4 text_mid;
        ImVec4 text_muted;
        ImVec4 text_subtle;
        ImVec4 text_faint;
        ImVec4 text_hint;
        ImVec4 text_dim;
        ImVec4 text_placeholder;

        ImVec4 pill;
        ImVec4 pill_hover;

        ImVec4 shadow;

        ImVec4 accent;
        ImVec4 accent_bg;
        ImVec4 accent_subtle;
        ImVec4 accent_muted;
        ImVec4 accent_surface;
        ImVec4 accent_border;
        ImVec4 accent_strong;

        void derive_accent_from( ImVec4 const& base )
        {
            accent        = base;
            accent_bg     = { base.x * 0.3f, base.y * 0.3f, base.z * 0.3f, base.w };
            accent_subtle = { base.x * 0.4f, base.y * 0.4f, base.z * 0.4f, base.w };
            accent_muted  = { base.x * 0.6f, base.y * 0.6f, base.z * 0.6f, base.w };
            accent_surface= { base.x * 0.7f, base.y * 0.7f, base.z * 0.7f, base.w };
            accent_border = { base.x * 0.8f, base.y * 0.8f, base.z * 0.8f, base.w };
            accent_strong = { base.x * 0.9f, base.y * 0.9f, base.z * 0.9f, base.w };
        }

        ImVec4 danger;
        ImVec4 danger_subtle;

        ImVec4 overlay_hover;
        ImVec4 overlay_pressed;
        ImVec4 overlay_subtle;
        ImVec4 overlay_faint;
    };

    struct theme_field_t
    {
        char const* label;
        std::size_t offset;
    };

    theme_field_t const* theme_fields( );
    int theme_field_count( );

    class c_themes
    {
        static int active_idx;
        static std::vector< theme_palette_t > presets;
        static std::filesystem::path themes_dir;

    public:
        static void initialize( );
        static theme_palette_t const& palette( );
        static theme_palette_t& palette_mut( );
        static void set( int idx );
        static int count( );
        static std::string const& name( int idx );
        static int index( );

        static void add( theme_palette_t theme );
        static void remove( int idx );
        static bool is_builtin( int idx );
        static theme_palette_t& at( int idx );

        static void save_theme( int idx );
        static void save_all_user( );
        static void load_user_themes( );
        static void delete_theme_file( std::string const& theme_name );
    };
} // namespace alice
