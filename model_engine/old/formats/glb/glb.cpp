#include "glb.hpp"
#include "json.hpp"

#include <fstream>
#include <cstring>
#include <unordered_map>

using alice::json_t;
using alice::glb::scene_data_t;
using alice::glb::mesh_data_t;
using alice::glb::joint_info_t;
using alice::glb::image_data_t;
using alice::glb::material_data_t;
using alice::glb::alpha_mode_e;

struct buffer_context_t
{
    json_t const* root = nullptr;
    uint8_t const* bin = nullptr;
    std::size_t bin_size = 0;
};

static void read_accessor_floats( buffer_context_t const& ctx, int accessor_idx, std::vector< float >& out, int expected_components )
{
    auto const& acc = ( *ctx.root )[ "accessors" ][ static_cast< std::size_t >( accessor_idx ) ];
    auto const& bv = ( *ctx.root )[ "bufferViews" ][ static_cast< std::size_t >( acc[ "bufferView" ].as_int( ) ) ];

    int const count = acc[ "count" ].as_int( );
    int const bv_offset = bv.has( "byteOffset" ) ? bv[ "byteOffset" ].as_int( ) : 0;
    int const acc_offset = acc.has( "byteOffset" ) ? acc[ "byteOffset" ].as_int( ) : 0;
    int const stride = bv.has( "byteStride" ) ? bv[ "byteStride" ].as_int( ) : ( expected_components * 4 );

    uint8_t const* base = ctx.bin + bv_offset + acc_offset;

    out.resize( static_cast< std::size_t >( count * expected_components ) );
    for ( int i = 0; i < count; ++i )
    {
        auto const* src = reinterpret_cast< float const* >( base + static_cast< std::ptrdiff_t >( i ) * stride );
        for ( int c = 0; c < expected_components; ++c )
            out[ static_cast< std::size_t >( i * expected_components + c ) ] = src[ c ];
    }
}

static void read_accessor_u16( buffer_context_t const& ctx, int accessor_idx, std::vector< uint16_t >& out, int expected_components )
{
    auto const& acc = ( *ctx.root )[ "accessors" ][ static_cast< std::size_t >( accessor_idx ) ];
    auto const& bv = ( *ctx.root )[ "bufferViews" ][ static_cast< std::size_t >( acc[ "bufferView" ].as_int( ) ) ];

    int const count = acc[ "count" ].as_int( );
    int const bv_offset = bv.has( "byteOffset" ) ? bv[ "byteOffset" ].as_int( ) : 0;
    int const acc_offset = acc.has( "byteOffset" ) ? acc[ "byteOffset" ].as_int( ) : 0;
    int const stride = bv.has( "byteStride" ) ? bv[ "byteStride" ].as_int( ) : ( expected_components * 2 );

    uint8_t const* base = ctx.bin + bv_offset + acc_offset;

    out.resize( static_cast< std::size_t >( count * expected_components ) );
    for ( int i = 0; i < count; ++i )
    {
        auto const* src = reinterpret_cast< uint16_t const* >( base + static_cast< std::ptrdiff_t >( i ) * stride );
        for ( int c = 0; c < expected_components; ++c )
            out[ static_cast< std::size_t >( i * expected_components + c ) ] = src[ c ];
    }
}

static void read_accessor_u32( buffer_context_t const& ctx, int accessor_idx, std::vector< uint32_t >& out )
{
    auto const& acc = ( *ctx.root )[ "accessors" ][ static_cast< std::size_t >( accessor_idx ) ];
    auto const& bv = ( *ctx.root )[ "bufferViews" ][ static_cast< std::size_t >( acc[ "bufferView" ].as_int( ) ) ];

    int const count = acc[ "count" ].as_int( );
    int const comp_type = acc[ "componentType" ].as_int( );
    int const bv_offset = bv.has( "byteOffset" ) ? bv[ "byteOffset" ].as_int( ) : 0;
    int const acc_offset = acc.has( "byteOffset" ) ? acc[ "byteOffset" ].as_int( ) : 0;

    uint8_t const* base = ctx.bin + bv_offset + acc_offset;
    out.resize( static_cast< std::size_t >( count ) );

    if ( comp_type == 5125 )
    {
        std::memcpy( out.data( ), base, count * sizeof( uint32_t ) );
    }
    else if ( comp_type == 5123 )
    {
        auto const* src = reinterpret_cast< uint16_t const* >( base );
        for ( int i = 0; i < count; ++i )
            out[ i ] = src[ i ];
    }
    else if ( comp_type == 5121 )
    {
        for ( int i = 0; i < count; ++i )
            out[ i ] = base[ i ];
    }
}

static void read_accessor_mat4( buffer_context_t const& ctx, int accessor_idx, std::vector< float >& out )
{
    auto const& acc = ( *ctx.root )[ "accessors" ][ static_cast< std::size_t >( accessor_idx ) ];
    auto const& bv = ( *ctx.root )[ "bufferViews" ][ static_cast< std::size_t >( acc[ "bufferView" ].as_int( ) ) ];

    int const count = acc[ "count" ].as_int( );
    int const bv_offset = bv.has( "byteOffset" ) ? bv[ "byteOffset" ].as_int( ) : 0;
    int const acc_offset = acc.has( "byteOffset" ) ? acc[ "byteOffset" ].as_int( ) : 0;

    uint8_t const* base = ctx.bin + bv_offset + acc_offset;
    out.resize( static_cast< std::size_t >( count * 16 ) );
    std::memcpy( out.data( ), base, count * 16 * sizeof( float ) );
}

static void parse_meshes( buffer_context_t const& ctx, json_t const& root, scene_data_t& out )
{
    if ( !root.has( "meshes" ) || !root.has( "nodes" ) )
        return;

    auto const& nodes = root[ "nodes" ];
    for ( std::size_t ni = 0; ni < nodes.size( ); ++ni )
    {
        auto const& node = nodes[ ni ];
        if ( !node.has( "mesh" ) )
            continue;

        int const mesh_idx = node[ "mesh" ].as_int( );
        auto const& mesh = root[ "meshes" ][ static_cast< std::size_t >( mesh_idx ) ];
        auto const& prims = mesh[ "primitives" ];

        for ( std::size_t pi = 0; pi < prims.size( ); ++pi )
        {
            auto const& prim = prims[ pi ];
            auto const& attrs = prim[ "attributes" ];

            mesh_data_t md;

            if ( attrs.has( "POSITION" ) )
                read_accessor_floats( ctx, attrs[ "POSITION" ].as_int( ), md.positions, 3 );
            if ( attrs.has( "NORMAL" ) )
                read_accessor_floats( ctx, attrs[ "NORMAL" ].as_int( ), md.normals, 3 );
            if ( attrs.has( "TEXCOORD_0" ) )
                read_accessor_floats( ctx, attrs[ "TEXCOORD_0" ].as_int( ), md.texcoords, 2 );
            if ( attrs.has( "JOINTS_0" ) )
                read_accessor_u16( ctx, attrs[ "JOINTS_0" ].as_int( ), md.joints, 4 );
            if ( attrs.has( "WEIGHTS_0" ) )
                read_accessor_floats( ctx, attrs[ "WEIGHTS_0" ].as_int( ), md.weights, 4 );
            if ( prim.has( "indices" ) )
                read_accessor_u32( ctx, prim[ "indices" ].as_int( ), md.indices );

            md.material_index = prim.has( "material" ) ? prim[ "material" ].as_int( ) : -1;
            out.meshes.push_back( std::move( md ) );
        }
    }
}

static void parse_skin( buffer_context_t const& ctx, json_t const& root, scene_data_t& out )
{
    if ( !root.has( "skins" ) || root[ "skins" ].size( ) == 0 )
        return;

    auto const& skin = root[ "skins" ][ 0 ];
    auto const& joint_nodes = skin[ "joints" ];
    auto const& nodes = root[ "nodes" ];

    int const joint_count = static_cast< int >( joint_nodes.size( ) );

    std::unordered_map< int, int > node_to_joint;
    for ( int ji = 0; ji < joint_count; ++ji )
        node_to_joint[ joint_nodes[ ji ].as_int( ) ] = ji;

    std::unordered_map< int, int > node_parent;
    for ( std::size_t ni = 0; ni < nodes.size( ); ++ni )
    {
        if ( !nodes[ ni ].has( "children" ) )
            continue;
        auto const& children = nodes[ ni ][ "children" ];
        for ( std::size_t ci = 0; ci < children.size( ); ++ci )
            node_parent[ children[ ci ].as_int( ) ] = static_cast< int >( ni );
    }

    out.skin.joints.resize( static_cast< std::size_t >( joint_count ) );
    for ( int ji = 0; ji < joint_count; ++ji )
    {
        int const ni = joint_nodes[ ji ].as_int( );
        auto const& node = nodes[ static_cast< std::size_t >( ni ) ];

        auto& info = out.skin.joints[ ji ];
        info.node_index = ni;
        info.name = node.has( "name" ) ? node[ "name" ].str : "";

        if ( node.has( "translation" ) )
        {
            auto const& t = node[ "translation" ];
            info.translation = { t[ 0 ].as_float( ), t[ 1 ].as_float( ), t[ 2 ].as_float( ) };
        }
        if ( node.has( "rotation" ) )
        {
            auto const& r = node[ "rotation" ];
            info.rotation = { r[ 0 ].as_float( ), r[ 1 ].as_float( ), r[ 2 ].as_float( ), r[ 3 ].as_float( ) };
        }
        if ( node.has( "scale" ) )
        {
            auto const& sc = node[ "scale" ];
            info.scale = { sc[ 0 ].as_float( ), sc[ 1 ].as_float( ), sc[ 2 ].as_float( ) };
        }

        info.parent = -1;
        if ( auto it = node_parent.find( ni ); it != node_parent.end( ) )
        {
            if ( auto jt = node_to_joint.find( it->second ); jt != node_to_joint.end( ) )
                info.parent = jt->second;
        }
    }

    if ( skin.has( "inverseBindMatrices" ) )
        read_accessor_mat4( ctx, skin[ "inverseBindMatrices" ].as_int( ), out.skin.inverse_bind_matrices );
}

static void parse_materials( json_t const& root, scene_data_t& out )
{
    if ( root.has( "textures" ) )
    {
        auto const& textures = root[ "textures" ];
        out.texture_to_image.resize( textures.size( ) );
        for ( std::size_t i = 0; i < textures.size( ); ++i )
            out.texture_to_image[ i ] = textures[ i ].has( "source" ) ? textures[ i ][ "source" ].as_int( ) : -1;
    }

    if ( !root.has( "materials" ) )
        return;

    auto const& mats = root[ "materials" ];
    out.materials.resize( mats.size( ) );

    for ( std::size_t i = 0; i < mats.size( ); ++i )
    {
        auto const& mat = mats[ i ];
        auto& md = out.materials[ i ];

        md.double_sided = mat.has( "doubleSided" ) && mat[ "doubleSided" ].boolean;

        if ( mat.has( "alphaMode" ) )
        {
            auto const& mode = mat[ "alphaMode" ].str;
            if ( mode == "OPAQUE" )
                md.alpha_mode = alpha_mode_e::opaque;
            else if ( mode == "MASK" )
                md.alpha_mode = alpha_mode_e::mask;
            else if ( mode == "BLEND" )
                md.alpha_mode = alpha_mode_e::blend;
        }
        else
        {
            md.alpha_mode = alpha_mode_e::opaque;
        }

        if ( mat.has( "alphaCutoff" ) )
            md.alpha_cutoff = mat[ "alphaCutoff" ].as_float( );
        else if ( md.alpha_mode == alpha_mode_e::mask )
            md.alpha_cutoff = 0.5f;

        if ( mat.has( "pbrMetallicRoughness" ) )
        {
            auto const& pbr = mat[ "pbrMetallicRoughness" ];
            if ( pbr.has( "baseColorTexture" ) )
                md.texture_index = pbr[ "baseColorTexture" ][ "index" ].as_int( );
        }

        if ( md.texture_index < 0 && mat.has( "extensions" ) )
        {
            auto const& ext = mat[ "extensions" ];
            if ( ext.has( "KHR_materials_pbrSpecularGlossiness" ) )
            {
                auto const& sg = ext[ "KHR_materials_pbrSpecularGlossiness" ];
                if ( sg.has( "diffuseTexture" ) )
                    md.texture_index = sg[ "diffuseTexture" ][ "index" ].as_int( );
            }
        }
    }
}

static void parse_images( buffer_context_t const& ctx, json_t const& root, scene_data_t& out )
{
    if ( !root.has( "images" ) )
        return;

    auto const& images = root[ "images" ];
    out.images.resize( images.size( ) );

    for ( std::size_t i = 0; i < images.size( ); ++i )
    {
        auto const& img = images[ i ];
        if ( !img.has( "bufferView" ) )
            continue;

        auto const& bv = ( *ctx.root )[ "bufferViews" ][ static_cast< std::size_t >( img[ "bufferView" ].as_int( ) ) ];
        int const offset = bv.has( "byteOffset" ) ? bv[ "byteOffset" ].as_int( ) : 0;
        int const length = bv[ "byteLength" ].as_int( );

        auto& id = out.images[ i ];
        id.mime = img.has( "mimeType" ) ? img[ "mimeType" ].str : "image/png";
        id.pixels.assign( ctx.bin + offset, ctx.bin + offset + length );
    }
}

bool alice::glb::parse( std::string const& path, scene_data_t& out )
{
    std::ifstream file( path, std::ios::binary | std::ios::ate );
    if ( !file )
        return false;

    auto const file_size = file.tellg( );
    file.seekg( 0 );

    std::vector< uint8_t > data( static_cast< std::size_t >( file_size ) );
    file.read( reinterpret_cast< char* >( data.data( ) ), file_size );

    if ( data.size( ) < 12 )
        return false;

    uint32_t magic = 0, version = 0, length = 0;
    std::memcpy( &magic, data.data( ), 4 );
    std::memcpy( &version, data.data( ) + 4, 4 );
    std::memcpy( &length, data.data( ) + 8, 4 );

    if ( magic != 0x46546C67 || version != 2 )
        return false;

    uint32_t json_len = 0, json_type = 0;
    std::memcpy( &json_len, data.data( ) + 12, 4 );
    std::memcpy( &json_type, data.data( ) + 16, 4 );
    if ( json_type != 0x4E4F534A )
        return false;

    std::string_view json_str( reinterpret_cast< char const* >( data.data( ) + 20 ), json_len );

    uint8_t const* bin_ptr = nullptr;
    std::size_t bin_size = 0;

    std::size_t const bin_chunk_offset = 20 + json_len;
    if ( bin_chunk_offset + 8 <= data.size( ) )
    {
        uint32_t bin_len = 0, bin_type = 0;
        std::memcpy( &bin_len, data.data( ) + bin_chunk_offset, 4 );
        std::memcpy( &bin_type, data.data( ) + bin_chunk_offset + 4, 4 );
        if ( bin_type == 0x004E4942 )
        {
            bin_ptr = data.data( ) + bin_chunk_offset + 8;
            bin_size = bin_len;
        }
    }

    auto root = json_t::parse( json_str );

    buffer_context_t ctx;
    ctx.root = &root;
    ctx.bin = bin_ptr;
    ctx.bin_size = bin_size;

    parse_meshes( ctx, root, out );
    parse_skin( ctx, root, out );
    parse_materials( root, out );
    parse_images( ctx, root, out );

    return !out.meshes.empty( );
}
