#include "generic_animator.hpp"
#include "model_engine/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

using alice::c_generic_animator;

c_generic_animator::c_generic_animator( bool tpose )
    : tpose_correction( tpose )
{
}

void c_generic_animator::bind( c_skeleton* skel )
{
    if ( !skel )
        return;

    idx_pelvis = skel->find_joint( { "\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab", "\xe8\x85\xb0", "Pelvis", "Hips", "Hip", "Bone_Pelvis01_M", "Bip001 Pelvis", "Bip002 Pelvis" } );
    idx_spine = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab", "Spine", "Bone_Spine01_M", "Bip001 Spine", "Bip002 Spine" } );
    idx_spine1 = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x31", "Spine1", "Bone_Spine02_M", "Bip001 Spine1", "Bip002 Spine1" } );
    idx_spine2 = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x32", "Spine2", "Chest", "UpperChest", "Bone_Spine03_M", "Bone_Chest01_M", "Bip001 Spine2", "Bip002 Spine2" } );
    idx_neck = skel->find_joint( { "\xe9\xa6\x96", "Neck", "Neck_M", "Bone_Neck01_M", "Bip001 Neck", "Bip002 Neck" } );
    idx_head = skel->find_joint( { "\xe9\xa0\xad", "Head", "Head_M", "Bone_Head01_M", "Bip001 Head", "Bip002 Head" } );
    idx_l_clavicle = skel->find_joint( { "\xe5\xb7\xa6\xe8\x82\xa9", "L Clavicle", "LeftShoulder", "L_Shoulder", "Left shoulder", "Bone_Clavicle01_L", "Bone_Shoulder01_L", "Bip001 L Clavicle", "Bip002 L Clavicle" } );
    idx_l_upper_arm = skel->find_joint( { "\xe5\xb7\xa6\xe8\x85\x95", "L UpperArm", "LeftArm", "L_Arm", "Left arm", "Bone_UpperArm01_L", "Bip001 L UpperArm", "Bip002 L UpperArm" } );
    idx_l_forearm = skel->find_joint( { "\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x98", "L Forearm", "LeftForeArm", "L_ForeArm", "Left elbow", "Bone_Elbow01_L", "Bip001 L Forearm", "Bip002 L Forearm" } );
    idx_r_clavicle = skel->find_joint( { "\xe5\x8f\xb3\xe8\x82\xa9", "R Clavicle", "RightShoulder", "R_Shoulder", "Right shoulder", "Bone_Clavicle01_R", "Bone_Shoulder01_R", "Bip001 R Clavicle", "Bip002 R Clavicle" } );
    idx_r_upper_arm = skel->find_joint( { "\xe5\x8f\xb3\xe8\x85\x95", "R UpperArm", "RightArm", "R_Arm", "Right arm", "Bone_UpperArm01_R", "Bip001 R UpperArm", "Bip002 R UpperArm" } );
    idx_r_forearm = skel->find_joint( { "\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x98", "R Forearm", "RightForeArm", "R_ForeArm", "Right elbow", "Bone_Elbow01_R", "Bip001 R Forearm", "Bip002 R Forearm" } );
    idx_l_thigh = skel->find_joint( { "\xe5\xb7\xa6\xe8\xb6\xb3", "L Thigh", "LeftUpLeg", "L_UpLeg", "Left leg", "Bip001 L Thigh", "Bip002 L Thigh" } );
    idx_r_thigh = skel->find_joint( { "\xe5\x8f\xb3\xe8\xb6\xb3", "R Thigh", "RightUpLeg", "R_UpLeg", "Right leg", "Bip001 R Thigh", "Bip002 R Thigh" } );
}

void c_generic_animator::update( c_skeleton* skel, float delta_time )
{
    if ( !skel )
        return;

    time += delta_time;

    skel->clear_anim( );

    if ( tpose_correction )
        apply_base_pose( skel );

    apply_idle( skel );
    skel->recompute( );
}

void c_generic_animator::apply_base_pose( c_skeleton* skel )
{
    auto& joints = skel->mutable_joints( );
    auto set = [ & ]( int idx, float rx, float ry, float rz ) {
        if ( idx < 0 )
            return;
        joints[ idx ].anim_rotation = c_skeleton::quat_from_euler( rx, ry, rz );
    };

    set( idx_r_upper_arm, 0.f, 0.f, 0.65f );
    set( idx_l_upper_arm, 0.f, 0.f, -0.65f );

    set( idx_r_forearm, 0.f, -0.15f, 0.f );
    set( idx_l_forearm, 0.f, 0.15f, 0.f );
}

void c_generic_animator::apply_idle( c_skeleton* skel )
{
    auto& joints = skel->mutable_joints( );

    auto layer = [ & ]( int idx, float rx, float ry, float rz ) {
        if ( idx < 0 )
            return;
        auto const q = c_skeleton::quat_from_euler( rx, ry, rz );
        joints[ idx ].anim_rotation = c_skeleton::quat_multiply( joints[ idx ].anim_rotation, q );
    };

    float const t = time;

    float const breath = std::sin( t * 2.2f ) * 0.010f;
    float const sway_y = std::sin( t * 0.42f ) * 0.024f + std::sin( t * 0.23f + 0.8f ) * 0.014f;
    float const sway_z = std::sin( t * 0.31f + 0.5f ) * 0.014f;
    float const head_turn = std::sin( t * 0.38f + 1.1f ) * 0.022f + std::sin( t * 0.83f + 0.4f ) * 0.009f;
    float const head_bob = std::sin( t * 0.55f + 0.2f ) * 0.010f;

    layer( idx_pelvis, 0.f, sway_y, 0.f );
    layer( idx_l_thigh, 0.f, -sway_y, 0.f );
    layer( idx_r_thigh, 0.f, -sway_y, 0.f );

    layer( idx_spine, -breath, sway_y * 0.5f, sway_z * 0.6f );
    layer( idx_spine1, -breath * 0.7f, sway_y * 0.3f, sway_z * 0.45f );
    layer( idx_spine2, -breath, sway_y * 0.15f, sway_z * 0.3f );

    layer( idx_neck, head_bob * 0.4f, head_turn * 0.45f, -sway_z * 0.25f );
    layer( idx_head, head_bob, head_turn, -sway_z * 0.15f );
}
