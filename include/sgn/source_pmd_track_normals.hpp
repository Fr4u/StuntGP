#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/retail_track_normal_allocation.hpp"

namespace sgn::retail
{
// A checked normal-table projection plus the existing scalar cursor owner.
// No vector table, raw pointer or span is retained outside the canonical bank.
class SourceTrackNormalView
{
    SourcePmdTrackView track_;
    TrackNormalAllocation *allocation_;

  public:
    SourceTrackNormalView(SourcePmdTrackView track, TrackNormalAllocation &allocation)
        : track_(std::move(track)), allocation_(&allocation)
    {
        validate();
    }
    void validate() const
    {
        const auto model = track_.model();
        const auto all = model.normals();
        const auto generated = track_.generated_normals();
        const auto a = all.location(), b = generated.location();
        if (a.allocation != b.allocation || b.offset < a.offset || (b.offset - a.offset) % 16 != 0 ||
            (b.offset - a.offset) / 16 != allocation_->first_generated ||
            generated.size() != all.size() - allocation_->first_generated)
            throw FormatError("canonical track normal scalar binding no longer names its owner");
    }
    SourcePmdArrayView<VehicleVector> normals() const
    {
        validate();
        return track_.model().normals();
    }
    TrackNormalAllocation &allocation() const
    {
        validate();
        return *allocation_;
    }
};
// 42EF80's624490 offset is headerE4*16 from62448C.576CC8 is retained.
inline SourceTrackNormalView bind_source_track_normal_allocation(SourcePmdTrackView track,
                                                                 TrackNormalAllocation &allocation)
{
    const auto all = track.model().normals();
    const auto generated = track.generated_normals();
    const auto a = all.location(), b = generated.location();
    if (a.allocation != b.allocation || b.offset < a.offset || (b.offset - a.offset) % 16 != 0 ||
        (b.offset - a.offset) / 16 > all.size())
        throw FormatError("canonical generated normal range leaves its table");
    allocation.first_generated = (b.offset - a.offset) / 16;
    return {track, allocation};
}
} // namespace sgn::retail
