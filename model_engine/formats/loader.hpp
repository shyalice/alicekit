#pragma once

#include "model_engine/formats/glb/glb.hpp"

#include <string>

namespace alice::formats
{
    bool parse( std::string const& path, glb::scene_data_t& out );
    bool is_tpose_format( std::string const& path );
} // namespace alice::formats
