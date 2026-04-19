//
// font_data_format.hpp
// shared between generator (dx9 tool) and loader (dx11/gl project)
// pure POD, no STL, no windows headers required
//

#pragma once

#include "imgui.h"

#include <cstdint>

namespace alice::font_format
{
    constexpr uint32_t MAGIC_DATA = 0x54414446u;  // 'FDAT'
    constexpr uint32_t MAGIC_ATLAS = 0x544C4146u; // 'FALT'
    constexpr uint32_t VERSION = 1u;

    enum class Rasterizer : uint8_t
    {
        DX9 = 0,
        FreeType = 1,
    };

#pragma pack( push, 1 )

    struct AtlasHeader
    {
        uint32_t magic; // MAGIC_ATLAS
        uint32_t version;
        uint32_t width;
        uint32_t height;
        // followed by width * height * 4 bytes of RGBA8 raw pixels
    };

    struct DataHeader
    {
        uint32_t magic; // MAGIC_DATA
        uint32_t version;
        uint32_t atlas_w;
        uint32_t atlas_h;
        float uv_white_x;
        float uv_white_y;
        uint32_t font_count;
        // followed by font_count FontEntry blocks
    };

    struct FontEntry
    {
        char family[ 64 ];
        int32_t size;
        int32_t weight;
        uint8_t charset;
        uint8_t quality;
        uint8_t rasterizer;
        uint8_t _pad;
        float font_size;
        float ascent;
        float descent;
        float fallback_advance_x;
        uint32_t glyph_count;
        // followed by glyph_count GlyphEntry blocks
    };

    struct GlyphEntry
    {
        uint32_t codepoint;
        float advance_x;
        float x0, y0, x1, y1;
        float u0, v0, u1, v1;
        uint8_t visible;
        uint8_t _pad[ 3 ];
    };

    struct ShadowRectEntry
    {
        uint16_t x;
        uint16_t y;
        uint16_t w;
        uint16_t h;
    };

    struct ShadowData
    {
        uint8_t valid;
        uint8_t pad[3];
        ShadowRectEntry rects[2];
        ImVec4 rect_uvs[10];
    };

#pragma pack( pop )

} // namespace alice::font_format