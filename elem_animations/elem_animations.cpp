//
// Created by rei on 4/9/2026.
//

#include "elem_animations.hpp"

using alice::c_elem_animations;

c_elem_animations& c_elem_animations::instance( )
{
    static c_elem_animations inst;
    return inst;
}

void c_elem_animations::clear_window( std::string const& window )
{
    windows.erase( window );
}

void c_elem_animations::tick( )
{
    for ( auto it = windows.begin( ); it != windows.end( ); )
    {
        auto* win = ImGui::FindWindowByName( it->first.c_str( ) );
        if ( !win || !win->WasActive )
            it = windows.erase( it );
        else
            ++it;
    }
}
