#pragma once
#include "sgn/model_resources.hpp"
#include "sgn/retail_track_part_preparation.hpp"
#include "sgn/track_part_animation.hpp"

namespace sgn::retail
{
// Full426260 order: source scene/model guards, per-part425DF0 when kind1..3,
// then that same part's72-byte state publications. All geometry views borrow
// the actual relocated ModelResource. Qualified by track-initialization-v1.
void initialize_track_part_resources(std::uint32_t scene, ModelResource *track, std::span<const std::uint8_t> pmd,
                                     std::span<TrackPartAnimation> records, TrackPartPreparationScratch &);
} // namespace sgn::retail
