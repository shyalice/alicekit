#include "pmx.hpp"

#include <algorithm>
#include <fstream>
#include <cstring>
#include <filesystem>
#include <cstdint>

struct pmx_reader_t
{
    std::vector< uint8_t > data;
    std::size_t pos = 0;

    uint8_t encoding = 0;
    uint8_t additional_uv = 0;
    uint8_t vert_idx_size = 0;
    uint8_t tex_idx_size = 0;
    uint8_t mat_idx_size = 0;
    uint8_t bone_idx_size = 0;
    uint8_t morph_idx_size = 0;
    uint8_t rigid_idx_size = 0;

    bool load( std::string const& path )
    {
        std::ifstream file( path, std::ios::binary | std::ios::ate );
        if ( !file )
            return false;
        auto const sz = file.tellg( );
        file.seekg( 0 );
        data.resize( static_cast< std::size_t >( sz ) );
        file.read( reinterpret_cast< char* >( data.data( ) ), sz );
        pos = 0;
        return true;
    }

    template< typename T >
    T read( )
    {
        T val;
        std::memcpy( &val, data.data( ) + pos, sizeof( T ) );
        pos += sizeof( T );
        return val;
    }

    void skip( std::size_t bytes ) { pos += bytes; }

    std::string read_text( )
    {
        uint32_t const len = read< uint32_t >( );
        if ( len == 0 )
            return "";

        if ( encoding == 0 )
        {
            std::string result;
            result.reserve( len );
            for ( uint32_t i = 0; i + 1 < len; i += 2 )
            {
                uint16_t cp = 0;
                std::memcpy( &cp, data.data( ) + pos + i, 2 );

                if ( cp < 0x80 )
                    result += static_cast< char >( cp );
                else if ( cp < 0x800 )
                {
                    result += static_cast< char >( 0xC0 | ( cp >> 6 ) );
                    result += static_cast< char >( 0x80 | ( cp & 0x3F ) );
                }
                else
                {
                    result += static_cast< char >( 0xE0 | ( cp >> 12 ) );
                    result += static_cast< char >( 0x80 | ( ( cp >> 6 ) & 0x3F ) );
                    result += static_cast< char >( 0x80 | ( cp & 0x3F ) );
                }
            }
            pos += len;
            return result;
        }

        std::string result( reinterpret_cast< char const* >( data.data( ) + pos ), len );
        pos += len;
        return result;
    }

    int read_index( uint8_t size )
    {
        if ( size == 1 ) return static_cast< int >( read< int8_t >( ) );
        if ( size == 2 ) return static_cast< int >( read< int16_t >( ) );
        return read< int32_t >( );
    }

    int read_bone_index( ) { return read_index( bone_idx_size ); }
    int read_tex_index( ) { return read_index( tex_idx_size ); }

    int read_vert_index( )
    {
        if ( vert_idx_size == 1 ) return static_cast< int >( read< uint8_t >( ) );
        if ( vert_idx_size == 2 ) return static_cast< int >( read< uint16_t >( ) );
        return read< int32_t >( );
    }
};

bool alice::pmx::parse( std::string const& path, glb::scene_data_t& out )
{
    pmx_reader_t r;
    if ( !r.load( path ) )
        return false;

    auto const base_dir = std::filesystem::path( path ).parent_path( );

    if ( std::memcmp( r.data.data( ), "PMX ", 4 ) != 0 )
        return false;
    r.skip( 4 );

    float const version = r.read< float >( );
    if ( version < 2.f )
        return false;

    uint8_t const glob_count = r.read< uint8_t >( );
    if ( glob_count < 8 )
        return false;

    r.encoding = r.read< uint8_t >( );
    r.additional_uv = r.read< uint8_t >( );
    r.vert_idx_size = r.read< uint8_t >( );
    r.tex_idx_size = r.read< uint8_t >( );
    r.mat_idx_size = r.read< uint8_t >( );
    r.bone_idx_size = r.read< uint8_t >( );
    r.morph_idx_size = r.read< uint8_t >( );
    r.rigid_idx_size = r.read< uint8_t >( );
    if ( glob_count > 8 )
        r.skip( glob_count - 8 );

    for ( int i = 0; i < 4; ++i )
        r.read_text( );

    int const vertex_count = r.read< int32_t >( );

    std::vector< float > positions( vertex_count * 3 );
    std::vector< float > normals( vertex_count * 3 );
    std::vector< float > texcoords( vertex_count * 2 );
    std::vector< uint16_t > joints( vertex_count * 4, 0 );
    std::vector< float > weights( vertex_count * 4, 0.f );

    for ( int vi = 0; vi < vertex_count; ++vi )
    {
        positions[ vi * 3 ] = r.read< float >( );
        positions[ vi * 3 + 1 ] = r.read< float >( );
        positions[ vi * 3 + 2 ] = r.read< float >( );
        normals[ vi * 3 ] = r.read< float >( );
        normals[ vi * 3 + 1 ] = r.read< float >( );
        normals[ vi * 3 + 2 ] = r.read< float >( );
        texcoords[ vi * 2 ] = r.read< float >( );
        texcoords[ vi * 2 + 1 ] = r.read< float >( );
        r.skip( r.additional_uv * 16 );

        uint8_t const wt = r.read< uint8_t >( );
        auto const bj = static_cast< std::size_t >( vi * 4 );
        auto const bw = static_cast< std::size_t >( vi * 4 );

        if ( wt == 0 )
        {
            joints[ bj ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            weights[ bw ] = 1.f;
        }
        else if ( wt == 1 )
        {
            joints[ bj ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            joints[ bj + 1 ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            float const w = r.read< float >( );
            weights[ bw ] = w;
            weights[ bw + 1 ] = 1.f - w;
        }
        else if ( wt == 2 )
        {
            for ( int k = 0; k < 4; ++k )
                joints[ bj + k ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            for ( int k = 0; k < 4; ++k )
                weights[ bw + k ] = r.read< float >( );
        }
        else if ( wt == 3 )
        {
            joints[ bj ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            joints[ bj + 1 ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            float const w = r.read< float >( );
            weights[ bw ] = w;
            weights[ bw + 1 ] = 1.f - w;
            r.skip( 36 );
        }
        else if ( wt == 4 )
        {
            for ( int k = 0; k < 4; ++k )
                joints[ bj + k ] = static_cast< uint16_t >( std::max( 0, r.read_bone_index( ) ) );
            for ( int k = 0; k < 4; ++k )
                weights[ bw + k ] = r.read< float >( );
            r.skip( 36 );
        }

        r.skip( 4 );
    }

    int const face_index_count = r.read< int32_t >( );
    std::vector< uint32_t > all_indices( face_index_count );
    for ( int i = 0; i < face_index_count; ++i )
        all_indices[ i ] = static_cast< uint32_t >( r.read_vert_index( ) );

    int const tex_count = r.read< int32_t >( );
    std::vector< std::string > tex_paths( tex_count );
    for ( int i = 0; i < tex_count; ++i )
        tex_paths[ i ] = r.read_text( );

    int const mat_count = r.read< int32_t >( );
    out.materials.resize( mat_count );

    int face_offset = 0;
    for ( int mi = 0; mi < mat_count; ++mi )
    {
        r.read_text( );
        r.read_text( );

        float const diffuse_r = r.read< float >( );
        float const diffuse_g = r.read< float >( );
        float const diffuse_b = r.read< float >( );
        float const diffuse_a = r.read< float >( );
        r.skip( 12 + 4 + 12 + 1 + 16 + 4 );

        int const tex_i = r.read_tex_index( );
        int const sphere_i = r.read_tex_index( );
        uint8_t const sphere_mode = r.read< uint8_t >( );

        uint8_t const toon_flag = r.read< uint8_t >( );
        int toon_i = -1;
        bool shared_toon = false;
        if ( toon_flag == 0 )
        {
            toon_i = r.read_tex_index( );
        }
        else
        {
            toon_i = static_cast< int >( r.read< uint8_t >( ) );
            shared_toon = true;
        }

        r.read_text( );

        int const mat_face_count = r.read< int32_t >( );

        auto& mat = out.materials[ mi ];
        mat.texture_index = ( tex_i >= 0 ) ? tex_i : -1;
        mat.sphere_texture_index = ( sphere_i >= 0 ) ? sphere_i : -1;
        mat.sphere_mode = static_cast< int >( sphere_mode );
        mat.toon_texture_index = toon_i;
        mat.shared_toon = shared_toon;
        mat.diffuse_rgba = { diffuse_r, diffuse_g, diffuse_b, diffuse_a };

        glb::mesh_data_t md;
        md.positions.resize( mat_face_count * 3 );
        md.normals.resize( mat_face_count * 3 );
        md.texcoords.resize( mat_face_count * 2 );
        md.joints.resize( mat_face_count * 4 );
        md.weights.resize( mat_face_count * 4 );
        md.indices.resize( mat_face_count );
        md.src_vertex_indices.resize( mat_face_count );

        for ( int fi = 0; fi < mat_face_count; ++fi )
        {
            uint32_t const src_vi = all_indices[ face_offset + fi ];

            md.positions[ fi * 3 ] = positions[ src_vi * 3 ];
            md.positions[ fi * 3 + 1 ] = positions[ src_vi * 3 + 1 ];
            md.positions[ fi * 3 + 2 ] = positions[ src_vi * 3 + 2 ];
            md.normals[ fi * 3 ] = normals[ src_vi * 3 ];
            md.normals[ fi * 3 + 1 ] = normals[ src_vi * 3 + 1 ];
            md.normals[ fi * 3 + 2 ] = normals[ src_vi * 3 + 2 ];
            md.texcoords[ fi * 2 ] = texcoords[ src_vi * 2 ];
            md.texcoords[ fi * 2 + 1 ] = texcoords[ src_vi * 2 + 1 ];

            for ( int k = 0; k < 4; ++k )
            {
                md.joints[ fi * 4 + k ] = joints[ src_vi * 4 + k ];
                md.weights[ fi * 4 + k ] = weights[ src_vi * 4 + k ];
            }

            md.indices[ fi ] = static_cast< uint32_t >( fi );
            md.src_vertex_indices[ fi ] = src_vi;
        }

        md.material_index = mi;
        out.meshes.push_back( std::move( md ) );
        face_offset += mat_face_count;
    }

    out.images.resize( tex_count );
    for ( int i = 0; i < tex_count; ++i )
    {
        auto normalized = tex_paths[ i ];
        std::ranges::replace( normalized, '\\', '/' );

        auto const full_path = base_dir / std::filesystem::path( reinterpret_cast< char8_t const* >( normalized.c_str( ) ) );
        std::ifstream tf( full_path, std::ios::binary | std::ios::ate );
        if ( !tf )
            continue;

        auto const sz = tf.tellg( );
        tf.seekg( 0 );
        out.images[ i ].pixels.resize( static_cast< std::size_t >( sz ) );
        tf.read( reinterpret_cast< char* >( out.images[ i ].pixels.data( ) ), sz );

        auto const ext_str = full_path.extension( ).string( );
        out.images[ i ].mime = ( ext_str == ".png" ) ? "image/png" : "image/bmp";
    }

    out.texture_to_image.resize( tex_count );
    for ( int i = 0; i < tex_count; ++i )
        out.texture_to_image[ i ] = i;

    int const bone_count = r.read< int32_t >( );
    out.skin.joints.resize( bone_count );
    out.skin.inverse_bind_matrices.resize( bone_count * 16, 0.f );

    struct bone_pos_t { float x, y, z; };
    std::vector< bone_pos_t > bone_positions( bone_count );

    for ( int bi = 0; bi < bone_count; ++bi )
    {
        auto& ji = out.skin.joints[ bi ];
        ji.name = r.read_text( );
        r.read_text( );

        float const bx = r.read< float >( );
        float const by = r.read< float >( );
        float const bz = r.read< float >( );

        int const parent = r.read_bone_index( );
        r.skip( 4 );
        uint16_t const flags = r.read< uint16_t >( );

        bone_positions[ bi ] = { bx, by, bz };
        ji.parent = parent;
        ji.node_index = bi;
        ji.rotation = { 0.f, 0.f, 0.f, 1.f };
        ji.scale = { 1.f, 1.f, 1.f };

        if ( parent >= 0 && parent < bone_count )
        {
            ji.translation = {
                bx - bone_positions[ parent ].x,
                by - bone_positions[ parent ].y,
                bz - bone_positions[ parent ].z
            };
        }
        else
        {
            ji.translation = { bx, by, bz };
        }

        auto const ib = static_cast< std::size_t >( bi * 16 );
        out.skin.inverse_bind_matrices[ ib + 0 ] = 1.f;
        out.skin.inverse_bind_matrices[ ib + 5 ] = 1.f;
        out.skin.inverse_bind_matrices[ ib + 10 ] = 1.f;
        out.skin.inverse_bind_matrices[ ib + 12 ] = -bx;
        out.skin.inverse_bind_matrices[ ib + 13 ] = -by;
        out.skin.inverse_bind_matrices[ ib + 14 ] = -bz;
        out.skin.inverse_bind_matrices[ ib + 15 ] = 1.f;

        if ( flags & 0x0001 ) r.read_bone_index( );
        else r.skip( 12 );
        if ( flags & 0x0100 || flags & 0x0200 ) { r.read_bone_index( ); r.skip( 4 ); }
        if ( flags & 0x0400 ) r.skip( 12 );
        if ( flags & 0x0800 ) r.skip( 24 );
        if ( flags & 0x1000 ) r.skip( 4 );
        if ( flags & 0x0020 )
        {
            r.read_bone_index( );
            r.skip( 4 + 4 );
            int const ik_links = r.read< int32_t >( );
            for ( int li = 0; li < ik_links; ++li )
            {
                r.read_bone_index( );
                if ( r.read< uint8_t >( ) )
                    r.skip( 24 );
            }
        }
    }

    int const morph_count = r.read< int32_t >( );
    for ( int mi_i = 0; mi_i < morph_count; ++mi_i )
    {
        auto morph_name = r.read_text( );
        r.read_text( );
        r.skip( 1 );
        uint8_t const morph_type = r.read< uint8_t >( );
        int const offset_count = r.read< int32_t >( );

        if ( morph_type == 1 )
        {
            glb::morph_data_t md;
            md.name = morph_name;
            md.vertex_offsets.reserve( offset_count );
            for ( int oi = 0; oi < offset_count; ++oi )
            {
                glb::morph_vertex_offset_t off;
                off.vertex_index = static_cast< uint32_t >( r.read_vert_index( ) );
                off.dx = r.read< float >( );
                off.dy = r.read< float >( );
                off.dz = r.read< float >( );
                md.vertex_offsets.push_back( off );
            }
            out.morphs.push_back( std::move( md ) );
        }
        else
        {
            int skip_per = 0;
            if ( morph_type == 0 ) skip_per = r.mat_idx_size + 4;
            else if ( morph_type == 2 ) skip_per = r.bone_idx_size + 12 + 16;
            else if ( morph_type >= 3 && morph_type <= 7 ) skip_per = r.vert_idx_size + 16;
            else if ( morph_type == 8 ) skip_per = r.mat_idx_size + 1 + 113;
            else if ( morph_type == 9 ) skip_per = 4 + 1 + 4;
            else if ( morph_type == 10 ) skip_per = 4 + 24;
            r.skip( offset_count * skip_per );
        }
    }

    int const frame_count = r.read< int32_t >( );
    for ( int fi = 0; fi < frame_count; ++fi )
    {
        r.read_text( );
        r.read_text( );
        r.skip( 1 );
        int const elem_count = r.read< int32_t >( );
        for ( int ei = 0; ei < elem_count; ++ei )
        {
            uint8_t const target = r.read< uint8_t >( );
            r.skip( target == 0 ? r.bone_idx_size : r.morph_idx_size );
        }
    }

    int const rb_count = r.read< int32_t >( );
    for ( int ri = 0; ri < rb_count; ++ri )
    {
        r.read_text( );
        r.read_text( );
        int const bone_index = r.read_bone_index( );
        r.skip( 1 );
        r.skip( 2 );
        uint8_t const shape = r.read< uint8_t >( );
        float const size_x = r.read< float >( );
        float const size_y = r.read< float >( );
        float const size_z = r.read< float >( );
        float const pos_x = r.read< float >( );
        float const pos_y = r.read< float >( );
        float const pos_z = r.read< float >( );
        float const rot_x = r.read< float >( );
        float const rot_y = r.read< float >( );
        float const rot_z = r.read< float >( );
        float const mass = r.read< float >( );
        float const lin_damp = r.read< float >( );
        float const ang_damp = r.read< float >( );
        float const restitution = r.read< float >( );
        float const friction = r.read< float >( );
        uint8_t const mode = r.read< uint8_t >( );

        if ( bone_index >= 0 )
        {
            glb::collider_t c;
            c.bone_index = bone_index;
            c.shape = shape;
            c.size = { size_x, size_y, size_z };
            c.local_pos = { pos_x, pos_y, pos_z };
            c.local_rot = { rot_x, rot_y, rot_z };
            out.colliders.push_back( c );
        }

        if ( ( mode == 1 || mode == 2 ) && bone_index >= 0 )
        {
            glb::physics_bone_t pb;
            pb.bone_index = bone_index;
            pb.mass = mass;
            pb.linear_damping = lin_damp;
            pb.angular_damping = ang_damp;
            pb.restitution = restitution;
            pb.friction = friction;
            pb.mode = mode;
            out.physics_bones.push_back( pb );
        }
    }

    return !out.meshes.empty( );
}
