#include "alice_animator.hpp"
#include "model_engine/skeleton.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>

using alice::c_alice_animator;
using alice::c_skeleton;

c_alice_animator::c_alice_animator( bool tpose, bool pmx_physics )
    : tpose_correction( tpose )
    , use_pmx_physics( pmx_physics )
{
}

void c_alice_animator::bind( c_skeleton* skel )
{
    if ( !skel )
        return;

    idx_pelvis = skel->find_joint( { "\xe4\xb8\x8b\xe5\x8d\x8a\xe8\xba\xab", "\xe8\x85\xb0", "Pelvis", "Hips", "Hip", "pelvis", "Bone_Pelvis01_M", "Bip001 Pelvis", "Bip002 Pelvis" } );
    idx_spine = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab", "Spine", "spine", "Bone_Spine01_M", "Bip001 Spine", "Bip002 Spine" } );
    idx_spine1 = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x31", "Spine1", "Spine_1", "Bone_Spine02_M", "Bip001 Spine1", "Bip002 Spine1" } );
    idx_spine2 = skel->find_joint( { "\xe4\xb8\x8a\xe5\x8d\x8a\xe8\xba\xab\x32", "Spine2", "Spine_2", "Chest", "UpperChest", "Chest_M", "Bone_Spine03_M", "Bone_Chest01_M", "Bip001 Spine2", "Bip002 Spine2" } );
    idx_neck = skel->find_joint( { "\xe9\xa6\x96", "Neck", "neck", "Neck_M", "Bone_Neck01_M", "Bip001 Neck", "Bip002 Neck" } );
    idx_head = skel->find_joint( { "\xe9\xa0\xad", "Head", "head", "Head_M", "Head1_M", "Bone_Head01_M", "Bip001 Head", "Bip002 Head" } );
    idx_l_clavicle = skel->find_joint( { "\xe5\xb7\xa6\xe8\x82\xa9", "L Clavicle", "LeftShoulder", "L_Shoulder", "Left shoulder", "Clavicle_L", "Bone_Clavicle01_L", "Bone_Shoulder01_L", "Bip001 L Clavicle", "Bip002 L Clavicle" } );
    idx_l_upper_arm = skel->find_joint( { "\xe5\xb7\xa6\xe8\x85\x95", "L UpperArm", "LeftArm", "L_Arm", "Left arm", "UpperArm_L", "Arm_L", "Bone_UpperArm01_L", "Bip001 L UpperArm", "Bip002 L UpperArm" } );
    idx_l_forearm = skel->find_joint( { "\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x98", "L Forearm", "LeftForeArm", "L_ForeArm", "Left elbow", "Elbow_L", "Forearm_L", "Bone_Elbow01_L", "Bip001 L Forearm", "Bip002 L Forearm" } );
    idx_l_hand = skel->find_joint( { "\xe5\xb7\xa6\xe6\x89\x8b\xe9\xa6\x96", "L Hand", "LeftHand", "L_Hand", "Left wrist", "Wrist_L", "Hand_L", "Bip001 L Hand", "Bip002 L Hand" } );
    idx_r_clavicle = skel->find_joint( { "\xe5\x8f\xb3\xe8\x82\xa9", "R Clavicle", "RightShoulder", "R_Shoulder", "Right shoulder", "Clavicle_R", "Bone_Clavicle01_R", "Bone_Shoulder01_R", "Bip001 R Clavicle", "Bip002 R Clavicle" } );
    idx_r_upper_arm = skel->find_joint( { "\xe5\x8f\xb3\xe8\x85\x95", "R UpperArm", "RightArm", "R_Arm", "Right arm", "UpperArm_R", "Arm_R", "Bone_UpperArm01_R", "Bip001 R UpperArm", "Bip002 R UpperArm" } );
    idx_r_forearm = skel->find_joint( { "\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x98", "R Forearm", "RightForeArm", "R_ForeArm", "Right elbow", "Elbow_R", "Forearm_R", "Bone_Elbow01_R", "Bip001 R Forearm", "Bip002 R Forearm" } );
    idx_r_hand = skel->find_joint( { "\xe5\x8f\xb3\xe6\x89\x8b\xe9\xa6\x96", "R Hand", "RightHand", "R_Hand", "Right wrist", "Wrist_R", "Hand_R", "Bip001 R Hand", "Bip002 R Hand" } );
    idx_l_thigh = skel->find_joint( { "\xe5\xb7\xa6\xe8\xb6\xb3", "L Thigh", "LeftUpLeg", "L_UpLeg", "Left leg", "Thigh_L", "Bip001 L Thigh", "Bip002 L Thigh" } );
    idx_l_calf = skel->find_joint( { "\xe5\xb7\xa6\xe3\x81\xb2\xe3\x81\x96", "L Calf", "LeftLeg", "L_Leg", "Left knee", "Knee_L", "Bone_Knee01_L", "Bip001 L Calf", "Bip002 L Calf" } );
    idx_l_foot = skel->find_joint( { "\xe5\xb7\xa6\xe8\xb6\xb3\xe9\xa6\x96", "L Foot", "LeftFoot", "L_Foot", "Left ankle", "Ankle_L", "Bone_Foot01_L", "Bone_Ankle01_L", "Bip001 L Foot", "Bip002 L Foot" } );
    idx_r_thigh = skel->find_joint( { "\xe5\x8f\xb3\xe8\xb6\xb3", "R Thigh", "RightUpLeg", "R_UpLeg", "Right leg", "Thigh_R", "Bip001 R Thigh", "Bip002 R Thigh" } );
    idx_r_calf = skel->find_joint( { "\xe5\x8f\xb3\xe3\x81\xb2\xe3\x81\x96", "R Calf", "RightLeg", "R_Leg", "Right knee", "Knee_R", "Bone_Knee01_R", "Bip001 R Calf", "Bip002 R Calf" } );
    idx_r_foot = skel->find_joint( { "\xe5\x8f\xb3\xe8\xb6\xb3\xe9\xa6\x96", "R Foot", "RightFoot", "R_Foot", "Right ankle", "Ankle_R", "Bone_Foot01_R", "Bone_Ankle01_R", "Bip001 R Foot", "Bip002 R Foot" } );

    idx_eyes = skel->find_joint( { "\xe4\xb8\xa1\xe7\x9b\xae", "Eyes", "Bone_EyesA01_M" } );
    idx_l_eye = skel->find_joint( { "\xe5\xb7\xa6\xe7\x9b\xae", "Eye_L", "EYE_L", "Left Eye", "Bone_Eye01_L", "EyeBone L A01", "Ext_Eye_L" } );
    idx_r_eye = skel->find_joint( { "\xe5\x8f\xb3\xe7\x9b\xae", "Eye_R", "EYE_R", "Right Eye", "Bone_Eye01_R", "EyeBone R A01", "Ext_Eye_R" } );

    for ( int i = 0; i < k_ear_chain; ++i )
    {
        auto const n = std::to_string( i + 1 );
        auto const n2 = std::string( i < 9 ? "0" : "" ) + n;
        idx_r_ear[ i ] = skel->find_joint( { "\xe5\x8f\xb3\xe8\x80\xb3_" + n, "Bdy_R_Ear_" + n2, "Bone_Ear0" + n + "_R" } );
        idx_l_ear[ i ] = skel->find_joint( { "\xe5\xb7\xa6\xe8\x80\xb3_" + n, "Bdy_L_Ear_" + n2, "Bone_Ear0" + n + "_L" } );
    }

    auto add_chain = [ & ]( std::initializer_list< int > chain ) {
        int prev = -1;
        for ( int b : chain )
        {
            if ( b < 0 )
                continue;
            if ( prev >= 0 )
                skel->add_esp_link( prev, b );
            prev = b;
        }
    };

    add_chain( { idx_pelvis, idx_spine, idx_spine1, idx_spine2, idx_neck, idx_head } );
    add_chain( { idx_spine2, idx_l_clavicle, idx_l_upper_arm, idx_l_forearm, idx_l_hand } );
    add_chain( { idx_spine2, idx_r_clavicle, idx_r_upper_arm, idx_r_forearm, idx_r_hand } );
    add_chain( { idx_pelvis, idx_l_thigh, idx_l_calf, idx_l_foot } );
    add_chain( { idx_pelvis, idx_r_thigh, idx_r_calf, idx_r_foot } );

    springs.bind( skel );

    auto is_animated_bone = [ this ]( int idx ) {
        if ( idx < 0 )
            return false;
        for ( int i = 0; i < k_ear_chain; ++i )
            if ( idx == idx_l_ear[ i ] || idx == idx_r_ear[ i ] )
                return true;
        return idx == idx_eyes || idx == idx_l_eye || idx == idx_r_eye;
    };

    auto const& joints = skel->get_joints( );
    auto uses_world_rot = [ &joints ]( int idx ) {
        auto const& name = joints[ idx ].name;
        static constexpr std::string_view k_breast = "\xe8\x83\xb8";
        static constexpr std::string_view k_butt = "\xe5\xb0\xbb";
        return name.find( k_breast ) != std::string::npos
            || name.find( k_butt ) != std::string::npos
            || name.find( "breast" ) != std::string::npos
            || name.find( "Breast" ) != std::string::npos
            || name.find( "butt" ) != std::string::npos
            || name.find( "Butt" ) != std::string::npos;
    };

    auto is_skirt_bone = [ &joints ]( int idx ) {
        auto const& name = joints[ idx ].name;
        static constexpr std::array< std::string_view, 11 > patterns {
            "\xe3\x82\xb9\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x88",
            "\xe8\xa3\xbe",
            "\xe8\xa3\x99",
            "\xe5\xb7\xa6\x46\x5f",
            "\xe5\x8f\xb3\x46\x5f",
            "skirt",
            "Skirt",
            "SKIRT",
            "Skn_Skirt",
            "dress",
            "Dress"
        };
        for ( auto const& p : patterns )
            if ( name.find( p ) != std::string::npos )
                return true;
        return false;
    };

    if ( use_pmx_physics )
    {
        static constexpr std::string_view k_weight_suffix = "\xe9\x8c\x98";

        for ( auto const& pb : skel->get_physics_bones( ) )
        {
            if ( pb.bone_index < 0 || pb.bone_index >= skel->joint_count( ) )
                continue;

            if ( is_animated_bone( pb.bone_index ) )
                continue;

            if ( uses_world_rot( pb.bone_index ) )
                continue;

            if ( is_skirt_bone( pb.bone_index ) )
                continue;

            auto const& name = joints[ pb.bone_index ].name;
            if ( name.size( ) >= k_weight_suffix.size( )
                 && name.compare( name.size( ) - k_weight_suffix.size( ), k_weight_suffix.size( ), k_weight_suffix ) == 0 )
                continue;

            springs.add_bone( pb.bone_index, 0.035f, 0.20f, 0.0030f, false );
        }
    }

    springs.auto_detect( {
        "\xe9\xab\xaa",
        "\xe9\xab\xae",
        "hair",
        "Hair",
        "\xe5\xb0\xbb\xe5\xb0\xbe",
        "tail",
        "Tail",
        "\xe6\x8c\xaf\xe8\xa2\x96",
        "\xe9\xa0\x98\xe5\xb8\xa6",
        "\xe6\xb5\x81\xe8\x98\x87",
        "ribbon",
        "Ribbon",
        "cape",
        "Cape"
    }, 0.035f, 0.20f, 0.0030f, false );


    springs.auto_detect( {
        "\xe8\x83\xb8",
        "breast",
        "Breast",
        "bust",
        "Bust"
    }, 0.04f, 0.30f, 0.f, true );

    springs.auto_detect( {
        "\xe5\xb0\xbb",
        "butt",
        "Butt",
        "ass",
        "Ass"
    }, 0.02f, 0.10f, 0.0008f, false );
}

void c_alice_animator::update( c_skeleton* skel, float delta_time )
{
    if ( !skel )
        return;

    time += delta_time;

    skel->clear_anim( );

    if ( tpose_correction )
        apply_base_pose( skel );

    apply_idle( skel );
    skel->recompute( );

    if ( !springs.empty( ) )
    {
        springs.update( delta_time );
        skel->recompute( );
    }

    update_morphs( delta_time );
}

void c_alice_animator::update_morphs( float )
{
    auto hash01 = []( int n ) {
        auto h = static_cast< std::uint32_t >( n ) * 2654435761u;
        h ^= h >> 16;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        return static_cast< float >( h & 0xFFFFFFu ) / static_cast< float >( 0xFFFFFFu );
    };

    float const period = 3.2f;
    int const slot = static_cast< int >( time / period );
    float const local_t = std::fmod( time, period );

    float const jitter = hash01( slot * 31 + 7 ) * 1.5f;
    float const start = jitter;
    float const close_t = 0.08f;
    float const hold_t = 0.03f;
    float const open_t = 0.10f;
    float const end = start + close_t + hold_t + open_t;

    float blink = 0.f;
    if ( local_t >= start && local_t < end )
    {
        float const dt = local_t - start;
        if ( dt < close_t )
            blink = dt / close_t;
        else if ( dt < close_t + hold_t )
            blink = 1.f;
        else
            blink = 1.f - ( dt - close_t - hold_t ) / open_t;
    }

    if ( ( slot % 7 ) == 3 && local_t >= start + 0.5f && local_t < start + 0.6f )
    {
        float const dt2 = local_t - start - 0.5f;
        float const b2 = ( dt2 < 0.04f ) ? ( dt2 / 0.04f ) : ( 1.f - ( dt2 - 0.04f ) / 0.06f );
        blink = std::max( blink, std::clamp( b2, 0.f, 1.f ) );
    }

    blink = std::clamp( blink, 0.f, 1.f );
    m_morph_weights[ "\xe3\x81\xbe\xe3\x81\xb0\xe3\x81\x9f\xe3\x81\x8d" ] = blink;
}

void c_alice_animator::apply_base_pose( c_skeleton* skel )
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

void c_alice_animator::apply_idle( c_skeleton* skel )
{
    auto& joints = skel->mutable_joints( );

    auto layer = [ & ]( int idx, float rx, float ry, float rz ) {
        if ( idx < 0 )
            return;
        auto const q = c_skeleton::quat_from_euler( rx, ry, rz );
        joints[ idx ].anim_rotation = c_skeleton::quat_multiply( joints[ idx ].anim_rotation, q );
    };

    auto smoothstep = []( float x ) {
        x = std::clamp( x, 0.f, 1.f );
        return x * x * ( 3.f - 2.f * x );
    };

    auto envelope = [ & ]( float t0, float attack, float hold, float release, float t ) {
        float const end = t0 + attack + hold + release;
        if ( t < t0 || t > end )
            return 0.f;
        if ( t < t0 + attack )
            return smoothstep( ( t - t0 ) / attack );
        if ( t < t0 + attack + hold )
            return 1.f;
        return smoothstep( ( end - t ) / release );
    };

    auto hash01 = []( int n ) {
        auto h = static_cast< std::uint32_t >( n ) * 2654435761u;
        h ^= h >> 16;
        h *= 0x85ebca6bu;
        h ^= h >> 13;
        return static_cast< float >( h & 0xFFFFFFu ) / static_cast< float >( 0xFFFFFFu );
    };

    float const t = time;
    float const pi = std::numbers::pi_v< float >;

    float const breath_main = std::sin( t * 2.2f ) * 0.010f;
    float const sigh_cycle = std::fmod( t + 4.f, 19.f );
    float const sigh_env = ( sigh_cycle < 3.4f )
        ? smoothstep( sigh_cycle / 0.7f ) * smoothstep( ( 3.4f - sigh_cycle ) / 0.8f )
        : 0.f;
    float const sigh = std::sin( sigh_cycle * 1.05f ) * 0.022f * sigh_env;
    float const breath = breath_main + sigh;
    float const breath_slow = std::sin( t * 2.2f - 0.4f ) * 0.007f + sigh * 0.6f;

    float const ws_a = std::sin( t * 0.37f );
    float const ws_b = std::sin( t * 0.19f + 1.3f );
    float const weight_norm = std::clamp( ws_a * 0.7f + ws_b * 0.3f, -1.f, 1.f );
    float const weight = weight_norm * 0.016f;
    float const body_sway_y = std::sin( t * 0.42f ) * 0.024f + std::sin( t * 0.23f + 0.8f ) * 0.014f;
    float const body_sway_z = std::sin( t * 0.31f + 0.5f ) * 0.014f;
    float const torso_pitch = std::sin( t * 0.28f + 0.3f ) * 0.006f;
    float const head_bob = std::sin( t * 0.55f + 0.2f ) * 0.010f + std::sin( t * 1.3f + 0.6f ) * 0.004f;
    float const head_turn = std::sin( t * 0.38f + 1.1f ) * 0.022f + std::sin( t * 0.83f + 0.4f ) * 0.009f + std::sin( t * 1.9f + 0.2f ) * 0.004f;
    float const head_wobble = std::sin( t * 2.1f + 0.3f ) * 0.003f;

    float const fidget_spine = std::sin( t * 1.1f ) * 0.007f + std::sin( t * 0.6f + 0.7f ) * 0.004f;

    float const wave_cycle = std::fmod( t + 2.f, 33.f );
    float const wb = envelope( 0.4f, 1.2f, 2.6f, 1.6f, wave_cycle );
    float const wave_flutter = wb * std::sin( ( wave_cycle - 1.2f ) * 6.5f ) * 0.18f;

    float const peek_cycle = std::fmod( t + 11.f, 34.f );
    float const peek = envelope( 0.3f, 0.9f, 3.0f, 0.9f, peek_cycle );
    float const peek_late = envelope( 0.9f, 0.7f, 2.4f, 1.1f, peek_cycle );

    int const tilt_slot = static_cast< int >( ( t + 5.f ) / 22.f );
    float const tilt_cycle = std::fmod( t + 5.f, 22.f );
    float const tilt_dir = ( hash01( tilt_slot * 17 + 3 ) < 0.5f ) ? 1.f : -1.f;
    float const tilt_env = envelope( 0.8f, 1.8f, 2.2f, 2.0f, tilt_cycle );
    float const head_tilt = tilt_env * tilt_dir * 0.08f;

    float const shrug_cycle = std::fmod( t + 7.f, 23.f );
    float const shrug = envelope( 1.0f, 0.35f, 0.55f, 0.7f, shrug_cycle );

    float const stretch_cycle = std::fmod( t + 17.f, 41.f );
    float const stretch = envelope( 1.2f, 0.9f, 1.4f, 1.6f, stretch_cycle );

    layer( idx_pelvis, 0.f, body_sway_y, 0.f );
    layer( idx_l_thigh, 0.f, -body_sway_y, 0.f );
    layer( idx_r_thigh, 0.f, -body_sway_y, 0.f );

    layer( idx_spine, -breath - peek * 0.1f + stretch * 0.08f + stretch * -0.04f + torso_pitch, body_sway_y * 0.5f, peek * 0.04f + fidget_spine * 0.35f + weight * 1.2f + body_sway_z * 0.6f );
    layer( idx_spine1, -breath_slow - peek * 0.1f + stretch * 0.06f + torso_pitch * 0.5f, body_sway_y * 0.3f, peek * 0.03f + weight * 0.7f + body_sway_z * 0.45f );
    layer( idx_spine2, -breath - peek * 0.08f + stretch * 0.05f + torso_pitch * 0.2f, body_sway_y * 0.15f, peek * 0.02f + weight * 0.4f + body_sway_z * 0.3f );

    constexpr float look_period = 4.8f;
    int const look_slot = static_cast< int >( t / look_period );
    float const slot_phase = std::fmod( t, look_period ) / look_period;
    auto look_target = [ & ]( int s ) {
        return std::pair{ ( hash01( s * 3 + 1 ) - 0.5f ) * 0.18f,
                          ( hash01( s * 3 + 7 ) - 0.5f ) * 0.28f };
    };
    auto const [ prev_x, prev_z ] = look_target( look_slot );
    auto const [ next_x, next_z ] = look_target( look_slot + 1 );
    float const saccade = smoothstep( slot_phase * 1.4f );
    float const look_x = prev_x + ( next_x - prev_x ) * saccade;
    float const look_z = prev_z + ( next_z - prev_z ) * saccade;

    float const head_micro_x = std::sin( t * 0.85f ) * 0.006f + std::sin( t * 0.41f + 0.4f ) * 0.007f;
    float const head_micro_z = std::sin( t * 0.92f + 0.7f ) * 0.005f + std::sin( t * 0.47f ) * 0.006f;

    float const peek_nod = peek_late * std::sin( t * 1.5f ) * 0.03f;
    float const peek_tilt = peek_late * 0.12f + peek_late * std::sin( t * 0.8f ) * 0.03f;
    float const peek_micro_y = peek_late * std::sin( t * 0.9f ) * 0.02f;

    float const neck_x = look_x * 0.35f + head_micro_x - peek_late * 0.05f + peek_nod + stretch * 0.10f + head_bob * 0.4f;
    float const head_x = look_x * 0.55f + head_micro_x * 0.6f - peek_late * 0.07f - peek_nod * 0.3f + stretch * 0.05f + head_bob + head_wobble;
    float const neck_y = look_z * 0.25f + peek_micro_y * 0.5f + head_turn * 0.45f;
    float const head_y = look_z * 0.40f - peek_micro_y * 0.5f + head_turn + head_wobble * 0.7f;
    float const neck_z = head_tilt * 0.45f + head_micro_z + peek_tilt * 0.45f - weight * 0.35f - body_sway_z * 0.25f;
    float const head_z = head_tilt * 0.6f + head_micro_z * 0.8f + peek_tilt * 0.55f - weight * 0.15f - body_sway_z * 0.15f + head_wobble * 0.5f;

    layer( idx_neck, neck_x, neck_y, neck_z );
    layer( idx_head, head_x, head_y, head_z );

    float const clav_breath = breath * 0.5f;
    float const l_shrug_amt = shrug * 0.22f;
    float const r_shrug_amt = shrug * 0.22f;
    layer( idx_l_clavicle, clav_breath - l_shrug_amt + peek * 0.02f + stretch * 0.18f, 0.f, clav_breath + l_shrug_amt * 0.25f );
    layer( idx_r_clavicle, clav_breath - r_shrug_amt - wb * 0.10f + peek * 0.02f + stretch * 0.18f, wb * 0.04f, -clav_breath - wb * 0.18f - r_shrug_amt * 0.25f );

    float const arm_l_idle = std::sin( t * 0.38f ) * 0.014f + std::sin( t * 0.17f + 1.2f ) * 0.010f;
    float const arm_r_idle = std::sin( t * 0.35f + 1.f ) * 0.014f + std::sin( t * 0.15f + 0.3f ) * 0.010f;
    layer( idx_l_upper_arm, shrug * 0.10f + stretch * 0.35f, 0.f, arm_l_idle + shrug * 0.14f + stretch * 0.25f );
    layer( idx_r_upper_arm, wb * 0.14f + shrug * 0.10f + stretch * 0.35f, 0.f, -wb * 0.85f + arm_r_idle * ( 1.f - wb ) - shrug * 0.14f - stretch * 0.25f );

    float const wave_local = std::clamp( wave_cycle - 0.4f, 0.f, 4.f );
    float const wave_osc_base = std::sin( wave_local * 2.f * pi / 2.0f );
    float const wave_osc = wave_osc_base * 0.26f * wb + wave_flutter * 0.2f;
    float const forearm_swing = wave_osc_base * 0.14f * wb;
    float const fg_r_idle = std::sin( t * 0.45f + 0.3f ) * 0.010f + std::sin( t * 0.22f ) * 0.006f;
    layer( idx_r_forearm, 0.f, 0.f, -wb * 0.75f + forearm_swing + fg_r_idle * ( 1.f - wb ) + stretch * 0.18f );

    layer( idx_r_hand, wb * 1.30f, 0.f, wave_osc );

    float const fg_l = std::sin( t * 0.42f ) * 0.014f + std::sin( t * 0.19f + 1.2f ) * 0.008f;
    layer( idx_l_forearm, 0.f, 0.f, fg_l + stretch * 0.18f );

    float const l_hand_fx = std::sin( t * 0.55f ) * 0.020f + std::sin( t * 0.27f + 0.6f ) * 0.012f;
    float const l_hand_fz = std::sin( t * 0.31f + 0.8f ) * 0.014f;
    layer( idx_l_hand, l_hand_fx, 0.f, l_hand_fz );

    float const r_hand_fidget = ( 1.f - wb ) * ( std::sin( t * 0.48f + 0.4f ) * 0.014f + std::sin( t * 0.23f ) * 0.008f );
    layer( idx_r_hand, r_hand_fidget, 0.f, 0.f );

    float const w_pos = std::max( 0.f,  weight_norm );
    float const w_neg = std::max( 0.f, -weight_norm );

    layer( idx_l_thigh,  weight_norm * 0.025f, 0.f, -weight_norm * 0.015f );
    layer( idx_r_thigh, -weight_norm * 0.025f, 0.f, -weight_norm * 0.015f );

    float const knee_micro_l = std::sin( t * 0.42f ) * 0.006f + std::sin( t * 0.21f ) * 0.003f;
    float const knee_micro_r = std::sin( t * 0.38f + 0.7f ) * 0.006f + std::sin( t * 0.19f + 0.4f ) * 0.003f;
    layer( idx_l_calf, w_neg * 0.085f + knee_micro_l, 0.f, 0.f );
    layer( idx_r_calf, w_pos * 0.085f + knee_micro_r, 0.f, 0.f );

    layer( idx_l_foot, -w_neg * 0.045f, 0.f, 0.f );
    layer( idx_r_foot, -w_pos * 0.045f, 0.f, 0.f );

    constexpr float gaze_period = 2.2f;
    int const gaze_slot = static_cast< int >( t / gaze_period );
    float const gaze_phase = std::fmod( t, gaze_period ) / gaze_period;
    auto gaze_target = [ & ]( int s ) {
        return std::pair{ ( hash01( s * 7 + 11 ) - 0.5f ) * 0.30f,
                          ( hash01( s * 7 + 31 ) - 0.5f ) * 0.40f };
    };
    auto const [ prev_gx, prev_gy ] = gaze_target( gaze_slot );
    auto const [ next_gx, next_gy ] = gaze_target( gaze_slot + 1 );
    float const saccade_ease = smoothstep( gaze_phase * 4.0f );
    float const gaze_y = prev_gy + ( next_gy - prev_gy ) * saccade_ease;
    float const gaze_x = prev_gx + ( next_gx - prev_gx ) * saccade_ease;

    float const micro_eye_x = std::sin( t * 1.3f ) * 0.008f + std::sin( t * 2.7f + 0.4f ) * 0.004f;
    float const micro_eye_y = std::sin( t * 1.1f + 0.8f ) * 0.008f + std::sin( t * 2.3f ) * 0.004f;

    layer( idx_eyes, gaze_x + micro_eye_x, gaze_y + micro_eye_y, 0.f );

    auto ear_twitch = [ & ]( float period, float offset ) {
        float const phase = std::fmod( t / period + offset, 1.f );
        if ( phase > 0.022f )
            return 0.f;
        return std::sin( phase / 0.022f * pi ) * 0.15f;
    };

    float const tw_r = ear_twitch( 17.f, 0.10f ) + ear_twitch( 9.3f, 0.40f ) * 0.55f;
    float const tw_l = ear_twitch( 23.f, 0.70f ) + ear_twitch( 11.7f, 0.20f ) * 0.55f;

    for ( int i = 0; i < k_ear_chain; ++i )
    {
        float const w = 1.f - static_cast< float >( i ) / static_cast< float >( k_ear_chain - 1 ) * 0.5f;
        float const fold = peek * -0.22f + stretch * -0.08f;
        float const wig_r = std::sin( t * 1.8f + i * 0.35f ) * 0.010f;
        float const wig_l = std::sin( t * 1.7f - i * 0.35f + 1.f ) * 0.010f;
        layer( idx_r_ear[ i ], ( fold + tw_r + wig_r ) * w,  wig_r * 0.30f,  wig_r * 0.20f );
        layer( idx_l_ear[ i ], ( fold + tw_l + wig_l ) * w, -wig_l * 0.30f, -wig_l * 0.20f );
    }
}
