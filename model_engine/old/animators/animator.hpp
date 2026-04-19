#pragma once

#include <string>
#include <unordered_map>

namespace alice
{
    class c_skeleton;

    class i_animator
    {
    public:
        virtual ~i_animator( ) = default;

        virtual void bind( c_skeleton* skel ) = 0;
        virtual void update( c_skeleton* skel, float delta_time ) = 0;
        virtual void reset( ) { time = 0.f; }

        virtual void set_world_rotation( float yaw, float pitch ) { }

        virtual std::unordered_map< std::string, float > const& morph_weights( ) const
        {
            static std::unordered_map< std::string, float > const empty;
            return empty;
        }

    protected:
        float time = 0.f;
    };

} // namespace alice
