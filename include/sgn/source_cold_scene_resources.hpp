#pragma once
#include "sgn/assets.hpp"
#include <cstdint>
#include <memory>

namespace sgn::retail
{
class RaceSessionPlatform;
class SessionWorldMaterials;
struct SourceBootFilePlatform;

// NEW diagnostic boundary, not a source EAX, readiness flag or resume token.
// The real synchronous caller must propagate it; returning success would skip
// the mandatory child. Earlier source stores and allocations remain reached.
struct SourceColdSceneResourceBoundary final : FormatError
{
    const std::uint32_t source_site;
    const std::uint32_t required_child;
    SourceColdSceneResourceBoundary(std::uint32_t site, std::uint32_t child, const char *message)
        : FormatError(message), source_site(site), required_child(child)
    {
    }
};

// Invocation-only borrows. Construction does no source work and owns no bank,
// world, material table, resource registry or replacement menu.
struct SourceColdSceneResourceContext
{
    const std::shared_ptr<RaceSessionPlatform> &platform;
    SessionWorldMaterials &materials;
    SourceBootFilePlatform &files;
};

// Full existing42D070 on actual cold scene2/target2, including all parsers,
// real file calls and script allocation/release. Its4453A0 child executes the
// source target!=1 branch. A reached target1 is rejected, never treated as an
// empty race override. No scene/target substitution or new request bank.
void load_source_cold_level_script_42d070(RaceSessionPlatform &, SourceBootFilePlatform &);

// Actual42E6E1 ->42E510 through42EB00,42D070,44FD60,full source4342C0,
// then42F090 on the same registry/bank/selection/cars. The returned boundary
// is BEFORE the renderer6244F8 predicate; not full42E6E0 or material readiness.
// Entry is one-shot via the SAME SessionWorldMaterials cold reservation.
struct SourceColdSceneResourcePrefixResult
{
    std::uint32_t next_instruction{0x42e6fa};
};
[[nodiscard]] SourceColdSceneResourcePrefixResult initialize_source_cold_scene_prefix_42e6e0(
    SourceColdSceneResourceContext &);
} // namespace sgn::retail
