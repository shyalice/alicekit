#pragma once

#include <string>
#include <vector>
#include <array>
#include <cstdint>

namespace alice::glb
{
    struct mesh_data_t
    {
        std::vector< float > positions;
        std::vector< float > normals;
        std::vector< float > texcoords;
        std::vector< uint16_t > joints;
        std::vector< float > weights;
        std::vector< uint32_t > indices;
        std::vector< uint32_t > src_vertex_indices;
        int material_index = -1;
    };

    struct morph_vertex_offset_t
    {
        uint32_t vertex_index;
        float dx, dy, dz;
    };

    struct morph_data_t
    {
        std::string name;
        std::vector< morph_vertex_offset_t > vertex_offsets;
    };

    struct joint_info_t
    {
        int parent = -1;
        int node_index = -1;
        std::string name;
        std::array< float, 3 > translation { };
        std::array< float, 4 > rotation { 0.f, 0.f, 0.f, 1.f };
        std::array< float, 3 > scale { 1.f, 1.f, 1.f };
    };

    struct skin_data_t
    {
        std::vector< joint_info_t > joints;
        std::vector< float > inverse_bind_matrices;
    };

    struct image_data_t
    {
        std::vector< uint8_t > pixels;
        std::string mime;
    };

    enum class alpha_mode_e
    {
        opaque,
        mask,
        blend,
    };

    struct material_data_t
    {
        int texture_index = -1;
        int sphere_texture_index = -1;
        int sphere_mode = 0;
        int toon_texture_index = -1;
        bool shared_toon = false;
        std::array< float, 4 > diffuse_rgba { 1.f, 1.f, 1.f, 1.f };
        bool double_sided = false;
        alpha_mode_e alpha_mode = alpha_mode_e::mask;
        float alpha_cutoff = 0.1f;
    };

    struct scene_data_t
    {
        std::vector< mesh_data_t > meshes;
        skin_data_t skin;
        std::vector< image_data_t > images;
        std::vector< material_data_t > materials;
        std::vector< int > texture_to_image;
        std::vector< morph_data_t > morphs;
    };

    bool parse( std::string const& path, scene_data_t& out );

} // namespace alice::glb
