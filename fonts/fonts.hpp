//
// Created by rei on 12/2/2025.
//

#ifndef EVERNESS_UI_FONTS_HPP
#define EVERNESS_UI_FONTS_HPP

#include "font_loader.hpp"

#include <unordered_map>
#include <string>
#include <memory>

#include "imgui.h"

namespace alice
{
    struct font_t
    {
        ImFont* ptr = nullptr;
        float size = 0.0f;

        ImVec2 calculate_text_size( std::string const& value, int offset = 0 ) const
        {
            auto result = ptr->CalcTextSizeA( size, FLT_MAX, -1, value.c_str( ) );
            result.y -= offset;
            return result;
        }
    };

    class c_fonts
    {
    public:
        font_t& get( std::string const& key, float scale = 1.5f );
        void initialize( );
        c_font_loader* get_loader( );
        void bind( );

    private:
        std::unordered_map< std::string, font_t > m_list;
        std::unique_ptr< c_font_loader > m_loader;
    };
} // namespace alice

#endif // EVERNESS_UI_FONTS_HPP
