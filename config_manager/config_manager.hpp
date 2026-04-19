//
// Created by rei on 4/13/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "custom_scrollbar/custom_scrollbar.hpp"
#include "popups/popups.hpp"

#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace alice
{
    struct config_entry_t
    {
        std::string id;
        std::string name;
    };

    struct config_item_anim_t
    {
        ImVec4 background { };
        ImVec4 outline { };
        ImVec4 name_color { };
        ImVec4 dots_icon { };
        ImVec4 save_icon { };
        float rename_t { 0.f };
    };

    class c_config_manager
    {
    public:
        using action_callback_t = std::function< void( std::string const& ) >;
        using rename_callback_t = std::function< void( std::string const&, std::string const& ) >;
        using create_callback_t = std::function< std::string( std::string const& ) >;
        using void_callback_t   = std::function< void( ) >;

        c_config_manager( );

        void set_on_load( action_callback_t cb );
        void set_on_save( action_callback_t cb );
        void set_on_delete( action_callback_t cb );
        void set_on_rename( rename_callback_t cb );
        void set_on_create( create_callback_t cb );
        void set_on_refresh( void_callback_t cb );

        void set_configs( std::vector< config_entry_t > entries );
        std::vector< config_entry_t > const& get_configs( ) const;
        std::string const& selected_id( ) const;

        void open( ImVec2 pos );
        void close( );
        bool is_open( ) const;
        void save_selected( );
        void render( );

    private:
        void render_item( int vi, config_entry_t const& entry, float width );

        std::vector< config_entry_t > configs;
        std::string selected_config_id;

        c_popup popup;
        c_popup ctx_popup;
        c_custom_scrollbar list_sb;
        std::string ctx_id;

        std::unordered_map< std::string, config_item_anim_t > item_anims;

        struct toolbar_btn_anim_t { ImVec4 background { }, icon { }; };
        std::unordered_map< ImGuiID, toolbar_btn_anim_t > tb_anims;

        struct tab_anim_t { ImVec4 bg { }, ic { }, tx { }; float mod { 0.f }; };
        tab_anim_t tab_anim;

        char search_buf[ 128 ] { };
        std::string renaming_id;
        char rename_buf[ 128 ] { };
        bool rename_focus_pending { false };

        int open_frame { -1 };
        ImVec2 open_pos { };
        float sb_reserve_anim { 0.f };

        int deferred { 0 };
        std::string deferred_id;

        action_callback_t on_load, on_save, on_delete;
        rename_callback_t on_rename;
        create_callback_t on_create;
        void_callback_t on_refresh;
    };
} // namespace alice
