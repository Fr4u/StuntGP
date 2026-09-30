#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/track_contact.hpp"
#include <optional>

namespace sgn::retail
{
class RaceSessionPlatform;
// NEW host owner (WP-B, DESIGN A2). Not recovered game behavior: the original
// reads collision directly through [624480] (420F80/420F00). The native tick
// consumes contact::TrackGeometry, so exactly one immutable decoded copy per
// level is built from the LIVE track allocation that 42ED80 relocated, keyed by
// that same SourcePmdLocation (allocation identity + generation + offset).
//
// get(): rebuilds when [624480] names another descriptor, drops the copy when
// [624480] is NULL, the scene track is missing or its allocation expired (42EAA0
// releases the bank; the next get() fails on the generation check and drops
// before rethrowing). drop() lets the root release it eagerly at 4420E5.
// The returned reference stays valid until the next get() that rebuilds or
// fails, drop(), or destruction. The copy is never re-synced: the load-time
// invariant below proves that no source writer can change what it decodes.
class SourceTrackCollision final
{
    struct Decoded
    {
        SourcePmdLocation key;
        contact::TrackGeometry geometry;
    };
    std::optional<Decoded> decoded_;

  public:
    SourceTrackCollision() = default;
    SourceTrackCollision(const SourceTrackCollision &) = delete;
    SourceTrackCollision &operator=(const SourceTrackCollision &) = delete;
    const contact::TrackGeometry &get(RaceSessionPlatform &);
    void drop() noexcept
    {
        decoded_.reset();
    }
    bool holds() const noexcept
    {
        return decoded_.has_value();
    }
};
// Load-time invariant (A2, extended to the 425DF0 writer): no vertex referenced
// by a reachable collision polygon lies in any vertex range that 426260/425DF0
// (part 0 when its 624484 kind is 1..3) or 426ED0 (every part p>0 of the
// descriptor+48 list) can write, and no reachable collision normal index is
// >= [0xE4] (the generated normals written by 426370). Otherwise it throws
// FormatError("animated track geometry overlaps collision; decoded collision would diverge").
void require_static_track_collision(const SourcePmdTrackView &, const contact::TrackGeometry &);
} // namespace sgn::retail
