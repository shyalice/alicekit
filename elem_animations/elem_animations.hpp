//
// Created by rei on 4/9/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

#include <any>
#include <string>
#include <unordered_map>

namespace alice
{
    class c_elem_animations
    {
        struct window_entry_t
        {
            std::unordered_map< ImGuiID, std::any > elements;
        };

        std::unordered_map< std::string, window_entry_t > windows;

    public:
        template < typename T >
        T* get( std::string const& window, ImGuiID id )
        {
            auto& entry = windows[ window ];

            auto it = entry.elements.find( id );
            if ( it == entry.elements.end( ) )
                it = entry.elements.emplace( id, T { } ).first;

            return std::any_cast< T >( &it->second );
        }

        template < typename T >
        void insert_if_empty( std::string const& window, ImGuiID id, const T& value )
        {
            auto& entry = windows[ window ];

            if ( !entry.elements.contains( id ) )
                entry.elements.emplace( id, value );
        }

        void clear_window( std::string const& window );

        void tick( );

        static c_elem_animations& instance( );
    };
} // namespace alice
