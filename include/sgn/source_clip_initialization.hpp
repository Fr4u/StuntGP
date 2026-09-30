#pragma once
#include <memory>
namespace sgn::retail
{
class RaceSessionPlatform;
// Full427270: checked alias publication, actual427240 pointer copies,16WORDs.
void initialize_source_clip_storage_427270(const std::weak_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
