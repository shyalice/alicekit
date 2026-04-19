//
// Created by rei on 4/8/2026.
//

#include "util.hpp"

#include <string>

std::string alice::truncate_text( ImFont* font, float font_size, std::string const& text, float max_width )
{
    auto const full = font->CalcTextSizeA( font_size, FLT_MAX, 0.f, text.c_str( ) );
    if ( full.x <= max_width )
        return text;

    auto const ellipsis = font->CalcTextSizeA( font_size, FLT_MAX, 0.f, "..." );
    float const target = max_width - ellipsis.x;
    if ( target <= 0.f )
        return "...";

    std::string result;
    float w = 0.f;
    for ( auto const ch : text )
    {
        auto const glyph = font->FindGlyph( ch );
        if ( !glyph )
            continue;
        if ( w + glyph->AdvanceX > target )
            break;
        w += glyph->AdvanceX;
        result += ch;
    }
    result += "...";
    return result;
}
