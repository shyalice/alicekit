#include "model_engine.hpp"
#include "skeleton.hpp"

#include <algorithm>
#include <cmath>

using alice::c_model;
using alice::c_model_engine;

void c_model::set_animator( std::unique_ptr< i_animator > animator )
{
    anim = std::move( animator );
    if ( anim && skel )
        anim->bind( skel.get( ) );
}

void c_model::update( float delta_time, bool is_dragging, float drag_dx, float drag_dy )
{
    if ( is_dragging )
    {
        target_yaw += drag_dx * k_drag_sensitivity;
        target_pitch += drag_dy * k_drag_sensitivity;
    }
    else
    {
        target_yaw += idle_yaw_velocity * delta_time;
    }

    target_pitch = std::clamp( target_pitch, limits.min_pitch, limits.max_pitch );

    if ( limits.clamp_yaw )
        target_yaw = std::clamp( target_yaw, limits.min_yaw, limits.max_yaw );

    float const t = 1.f - std::exp( -k_smooth_speed * delta_time );
    pitch = pitch + ( target_pitch - pitch ) * t;
    yaw = yaw + ( target_yaw - yaw ) * t;
    current_zoom = current_zoom + ( target_zoom - current_zoom ) * t;

    if ( anim && skel )
    {
        anim->set_world_rotation( yaw, pitch );
        anim->update( skel.get( ), delta_time );
    }
}

void c_model::reset_rotation( )
{
    pitch = 0.f;
    yaw = 0.15f;
    target_pitch = 0.f;
    target_yaw = 0.15f;
}

c_model* c_model_engine::load( std::string const& name, std::string const& path,
                                std::unique_ptr< i_animator > animator )
{
    auto model = make_model( );
    if ( !model || !model->load( path ) )
        return nullptr;

    if ( animator )
        model->set_animator( std::move( animator ) );

    auto* raw = model.get( );
    models[ name ] = std::move( model );
    return raw;
}

c_model* c_model_engine::get( std::string const& name )
{
    auto it = models.find( name );
    return it == models.end( ) ? nullptr : it->second.get( );
}

void c_model_engine::unload( std::string const& name )
{
    models.erase( name );
}

void c_model_engine::clear( )
{
    models.clear( );
}
