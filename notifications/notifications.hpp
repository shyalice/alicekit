//
// Created by rei on 4/13/2026.
//

#pragma once

#include "imgui.h"

#include <cstdint>
#include <string>
#include <vector>

namespace alice
{
    enum class notification_type : uint8_t
    {
        loading,
        success,
        failed,
        tip
    };

    struct notification_t
    {
        uint32_t id = 0;
        notification_type type = notification_type::tip;
        notification_type prev_type = notification_type::tip;
        std::string title;
        std::string description;

        float lifetime = 0.f;
        float max_lifetime = 3.5f;
        bool persistent = false;
        bool dismissed = false;

        float anim = 0.f;
        float type_blend = 1.f;
        float sweep_t = 0.f;
    };

    class c_notifications
    {
        static std::vector< notification_t > entries;
        static uint32_t next_id;

        static constexpr float k_width = 280.f;
        static constexpr float k_pad = 10.f;
        static constexpr float k_icon_area = 26.f;
        static constexpr float k_rounding = 8.f;
        static constexpr float k_gap = 14.f;
        static constexpr float k_margin = 14.f;
        static constexpr float k_text_gap = 8.f;
        static constexpr float k_desc_max = k_width - k_pad * 2.f - k_icon_area - k_text_gap;

        static float smooth( float current, float target, float speed, float dt );
        static ImVec4 color_for_type( notification_type type );
        static char const* icon_for_type( notification_type type );
        static float compute_height( notification_t const& n );

    public:
        static uint32_t push( notification_type type, std::string title, std::string description,
                              float duration = 3.5f );
        static uint32_t push_persistent( notification_type type, std::string title, std::string description );
        static void resolve( uint32_t id, notification_type new_type,
                             std::string new_title = { }, std::string new_description = { },
                             float dismiss_after = 2.5f );
        static void dismiss( uint32_t id );
        static void render( );
        static void clear( );
    };
} // namespace alice
