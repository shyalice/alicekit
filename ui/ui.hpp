//
// Created by rei on 4/8/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "config_manager/config_manager.hpp"
#include "custom_scrollbar/custom_scrollbar.hpp"
#include "window_animator/window_animator.hpp"

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace alice
{
    struct page_t
    {
        ImVec4 background { };
        ImVec4 outline { };
        ImVec4 text { };
        ImVec4 icon { };
    };

    struct action_button_t
    {
        ImVec4 background { };
        ImVec4 outline { };
        ImVec4 icon { };
    };

    struct search_entry_t
    {
        std::string label;
        std::string path;
        int page_idx;
        int subpage_idx;
    };

    struct search_result_anim_t
    {
        ImVec4 background { };
    };

    class c_ui_subpage
    {
    public:
        virtual ~c_ui_subpage( ) = default;
        virtual std::string name( ) const = 0;
        virtual std::string icon( ) const { return { }; }
        virtual void render( ) = 0;

        page_t anim { };
    };

    class c_ui_page
    {
        friend class c_ui;

        std::vector< std::unique_ptr< c_ui_subpage > > subpages;

        int selected_subpage_idx { 0 };
        int display_subpage_idx { 0 };
        float switch_t { 1.f };

        float indicator_x { -1.f };
        float indicator_w { 0.f };

    public:
        virtual ~c_ui_page( ) = default;
        virtual std::string name( ) const = 0;
        virtual std::string icon( ) const = 0;
        virtual void render( ) { }

        template < typename T, typename... Args >
        T* register_subpage( Args&&... args )
        {
            static_assert( std::is_base_of_v< c_ui_subpage, T >, "T must derive from c_ui_subpage" );
            auto ptr = std::make_unique< T >( std::forward< Args >( args )... );
            auto* raw = ptr.get( );
            subpages.push_back( std::move( ptr ) );
            return raw;
        }
    };

    class c_ui
    {
        static std::unordered_map< int, page_t > page_data;

        std::vector< std::unique_ptr< c_ui_page > > pages;

        int selected_page_idx { 0 };
        int display_page_idx { 0 };
        float page_switch_t { 1.f };
        float subtabs_anim { 0.f };

        bool presented = false;
        ImVec2 window_size { 800, 620 };
        c_window_animator window_animator { };
        c_custom_scrollbar page_scrollbar { };
        float scrollbar_inset { 0.f };

        bool render_tab( int idx, std::unique_ptr< c_ui_page >& page, ImVec2 const& size, bool active );
        bool render_subtab( c_ui_subpage* sub, ImVec2 const& size, bool active );
        bool render_action_button( int idx, std::string const& icon, ImVec2 const& size, ImVec4 const& accent );
        bool render_config_button( float height );
        void render_search_overlay( ImRect const& parent_rect, float alpha );
        void update_search_results( );
        void navigate_to( int page_idx, int subpage_idx );

        action_button_t action_buttons[ 3 ] { };

        struct config_button_anim_t
        {
            ImVec4 save_bg { };
            ImVec4 save_icon { };
            ImVec4 select_bg { };
            ImVec4 select_text { };
            ImVec4 select_arrow { };
            float smooth_w { 0.f };
            bool initialized { false };
        } config_button_anim { };

        c_config_manager config_manager;

        bool search_open = false;
        float search_anim = 0.f;
        char search_query[ 256 ] { };
        std::vector< search_entry_t > search_registry;
        std::unordered_set< std::string > search_registered_keys;
        std::vector< int > search_results;
        int search_selected = 0;
        float search_height_anim = 0.f;
        bool search_focus_pending = false;
        std::vector< search_result_anim_t > search_result_anims;

        int search_ctx_page = -1;
        int search_ctx_subpage = -1;
        bool search_index_built = false;
        bool search_indexing = false;

    public:
        c_ui( );
        ~c_ui( );

        template < typename T, typename... Args >
        T* register_page( Args&&... args )
        {
            static_assert( std::is_base_of_v< c_ui_page, T >, "T must derive from c_tab" );
            auto ptr = std::make_unique< T >( std::forward< Args >( args )... );
            auto* raw = ptr.get( );
            pages.push_back( std::move( ptr ) );
            return raw;
        }

        c_window_animator* get_window_animator( );

        void initialize( );
        void render( );

        void set_presence( bool value );
        bool is_presented( );

        void set_search_context( int page_idx, int subpage_idx );
        void try_register_search( std::string const& label );
        void build_search_index( );
        bool is_indexing( ) const;
    };
} // namespace alice
