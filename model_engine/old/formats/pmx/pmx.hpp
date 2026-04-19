#pragma once

#include "model_engine/formats/glb/glb.hpp"

#include <string>

namespace alice::pmx
{
    bool parse( std::string const& path, glb::scene_data_t& out );

} // namespace alice::pmx
