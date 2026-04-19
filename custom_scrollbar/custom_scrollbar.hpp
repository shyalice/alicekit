//
// Created by rei on 4/11/2026.
//

#pragma once

#include "imgui.h"
#include "imgui_internal.h"

namespace alice
{
    class c_custom_scrollbar
    {
        float fade { 0.f };
        float hover { 0.f };
        float active { 0.f };
        bool dragging { false };
        float drag_grab { 0.f };

    public:
        void render( ImGuiWindow* window, ImVec2 const& padding, bool inputs_allowed = true, float width = 6.f );
        float get_fade( ) const { return fade; }
    };
} // namespace alice
