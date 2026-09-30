#pragma once
#include "sgn/model_resources.hpp"

namespace sgn::retail::fixture {
// Test-only initial PE data for existing cold-template oracle corpora.
// Runtime callers must borrow WorldModelDrawState::template_pose instead.
inline constexpr ModelTransform cold_world_template{{0,0,0},0,{1,1,1},0,
    {0,0,-1},0,{0,1,0},0,{0,0,0,2}};
}
