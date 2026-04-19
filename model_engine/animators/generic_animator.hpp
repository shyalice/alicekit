#pragma once

#include "animator.hpp"

#include <string>

namespace alice
{
    class c_generic_animator final : public i_animator
    {
    public:
        explicit c_generic_animator( bool tpose_correction = true );

        void bind( c_skeleton* skel ) override;
        void update( c_skeleton* skel, float delta_time ) override;

    private:
        void apply_base_pose( c_skeleton* skel );
        void apply_idle( c_skeleton* skel );

        bool tpose_correction = true;

        int idx_pelvis = -1;
        int idx_spine = -1;
        int idx_spine1 = -1;
        int idx_spine2 = -1;
        int idx_neck = -1;
        int idx_head = -1;
        int idx_l_clavicle = -1;
        int idx_l_upper_arm = -1;
        int idx_l_forearm = -1;
        int idx_r_clavicle = -1;
        int idx_r_upper_arm = -1;
        int idx_r_forearm = -1;
        int idx_l_thigh = -1;
        int idx_r_thigh = -1;
    };
} // namespace alice
