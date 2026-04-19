#pragma once

#include "imgui.h"

namespace alice
{
    enum class fx_mode : int
    {
        none = 0,
        colored,
        flat,
        glow_full,
        advanced,
        hologram,
        galaxy,
        metallic,
        pulse,
        rainbow,
        iridescent,
        neon,
        glitch,
        liquid_chrome,
        void_energy,
        electric,
        glass,
        glow_outline,
        count
    };

    inline constexpr char const* k_fx_mode_names[] = {
        "None",
        "Colored",
        "Flat",
        "Glow (Full)",
        "Advanced",
        "Hologram",
        "Galaxy",
        "Metallic",
        "Pulse",
        "Rainbow",
        "Iridescent",
        "Neon",
        "Glitch",
        "Liquid Chrome",
        "Void",
        "Electric",
        "Glass",
        "Glow Outline",
    };

    struct fx_settings_t
    {
        fx_mode mode = fx_mode::none;
        ImVec4 main_color = { 1.f, 0.35f, 0.65f, 1.f };
        ImVec4 main_color_invisible = { 0.8f, 0.2f, 0.5f, 1.f };
        ImVec4 rim_color = { 0.3f, 0.75f, 1.f, 1.f };
        ImVec4 rim_color_invisible = { 0.2f, 0.5f, 0.8f, 1.f };
        float rim_power = 2.5f;
    };
} // namespace alice
