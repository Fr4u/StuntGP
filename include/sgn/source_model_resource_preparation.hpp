#pragma once
#include <cstdint>
#include <memory>

namespace sgn::retail
{
class RaceSessionPlatform;
// Full 42E240 color pass for the actual registry row. Source PMD and generated
// source sprite allocations are supported; native ModelResource/strip mirrors
// are an explicit boundary. No descriptor/header/corner payload is copied.
void prepare_source_model_colors_42e240(RaceSessionPlatform &, std::uint32_t registry_slot);

// 42E380 registry traversal followed by the actual 62447C/62449C light loop.
// Captures exact initialized==1 choice of first slot2, otherwise slot0.
// Same platform owns scene, bank and static light; the borrow must remain live
// and stable during the call. Reached writes survive a later host boundary.
// Does not perform/activate cold-tail or warm scene lifecycle operations.
void prepare_source_model_resources_42e380(const std::shared_ptr<RaceSessionPlatform> &);
} // namespace sgn::retail
