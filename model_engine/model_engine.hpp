#pragma once

#include "types.hpp"
#include "skeleton.hpp"
#include "animators/animator.hpp"
#include "renderer_fx/renderer_fx.hpp"

#include "imgui.h"

#include <algorithm>
#include <memory>
#include <string>
#include <unordered_map>

namespace alice
{

    class c_model
    {
    public:
        virtual ~c_model( ) = default;

        virtual bool load( std::string const& path ) = 0;
        virtual void unload( ) = 0;

        void set_animator( std::unique_ptr< i_animator > animator );
        void set_rotation_limits( rotation_limits_t const& lim ) { limits = lim; }
        void set_idle_speed( float speed ) { idle_yaw_velocity = speed; }
        void set_zoom( float z ) { target_zoom = std::clamp( z, 0.2f, 4.f ); }
        [[nodiscard]] float zoom( ) const { return current_zoom; }

        void update( float delta_time, bool is_dragging, float drag_dx, float drag_dy );
        void reset_rotation( );

        virtual void render( int width, int height ) = 0;

        [[nodiscard]] virtual ImTextureID srv( ) const = 0;
        [[nodiscard]] virtual bool loaded( ) const = 0;

        [[nodiscard]] c_skeleton* skeleton( ) const { return skel.get( ); }
        [[nodiscard]] matrix4x4_t current_wvp( ) const { return cached_wvp; }

    protected:
        std::unique_ptr< c_skeleton > skel;
        std::unique_ptr< i_animator > anim;

        float center_y = 0.f;
        float model_height = 1.f;

        float pitch = 0.f;
        float yaw = 0.15f;
        float target_pitch = 0.f;
        float target_yaw = 0.15f;
        float idle_yaw_velocity = 0.f;
        float current_zoom = 1.f;
        float target_zoom = 1.f;
        rotation_limits_t limits;

        matrix4x4_t cached_wvp { };

        static constexpr float k_smooth_speed = 8.f;
        static constexpr float k_drag_sensitivity = 0.005f;
        static constexpr float k_view_distance = 4.f;
    };

    class c_model_engine
    {
    public:
        virtual ~c_model_engine( ) = default;

        c_model* load( std::string const& name, std::string const& path,
                       std::unique_ptr< i_animator > animator = nullptr );

        c_model* get( std::string const& name );
        void unload( std::string const& name );
        void clear( );

        virtual void set_fx( fx_settings_t const* ) { }

    protected:
        virtual std::unique_ptr< c_model > make_model( ) = 0;

        std::unordered_map< std::string, std::unique_ptr< c_model > > models;
    };
} // namespace alice
