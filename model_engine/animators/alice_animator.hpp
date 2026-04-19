#pragma once

#include "animator.hpp"
#include "model_engine/physics/spring.hpp"

namespace alice
{
    class c_alice_animator final : public i_animator
    {
    public:
        explicit c_alice_animator( bool tpose_correction = true, bool use_pmx_physics = true );

        void bind( c_skeleton* skel ) override;
        void update( c_skeleton* skel, float delta_time ) override;
        void set_world_rotation( float yaw, float pitch ) override { springs.set_world_rotation( yaw, pitch ); }

        std::unordered_map< std::string, float > const& morph_weights( ) const override { return m_morph_weights; }

    private:
        void apply_base_pose( c_skeleton* skel );
        void apply_idle( c_skeleton* skel );
        void update_morphs( float delta_time );

        bool tpose_correction = true;
        bool use_pmx_physics = true;
        physics::c_spring_system springs;
        std::unordered_map< std::string, float > m_morph_weights;

        int idx_pelvis = -1;
        int idx_spine = -1;
        int idx_spine1 = -1;
        int idx_spine2 = -1;
        int idx_neck = -1;
        int idx_head = -1;
        int idx_l_clavicle = -1;
        int idx_l_upper_arm = -1;
        int idx_l_forearm = -1;
        int idx_l_hand = -1;
        int idx_r_clavicle = -1;
        int idx_r_upper_arm = -1;
        int idx_r_forearm = -1;
        int idx_r_hand = -1;
        int idx_l_thigh = -1;
        int idx_l_calf = -1;
        int idx_l_foot = -1;
        int idx_r_thigh = -1;
        int idx_r_calf = -1;
        int idx_r_foot = -1;

        int idx_eyes = -1;
        int idx_l_eye = -1;
        int idx_r_eye = -1;

        static constexpr int k_ear_chain = 5;
        int idx_l_ear[ k_ear_chain ] = { -1, -1, -1, -1, -1 };
        int idx_r_ear[ k_ear_chain ] = { -1, -1, -1, -1, -1 };
    };
} // namespace alice
