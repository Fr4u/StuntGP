#include "track_phase_record.hpp"
#include "sgn/model_resources.hpp"
#include <limits>
namespace sgn::retail
{
namespace
{
void observe(TrackPhaseAudit *audit, std::uint32_t at, std::uint32_t index, std::uint32_t value) noexcept
{
    if (audit && audit->after)
        audit->after(audit->context, at, index, value);
}
void advance_records(std::span<TrackPartAnimation> records, std::uint32_t count, TrackPhaseAudit *audit)
{
    for (std::uint32_t i = 1; i < count; ++i)
    {
        if (i >= records.size())
            throw FormatError("track animation current record exceeds owner");
        auto &record = records[i];
        const auto kind = record.kind;
        observe(audit, 0x427168, i, kind);
        detail::advance_track_phase_record(record, kind);
        observe(audit, 0x4271eb, i, record.phase_u);
    }
}
} // namespace
void advance_track_part_animations(std::span<TrackPartAnimation> records, SourcePmdModelView model,
                                   TrackPhaseAudit *audit)
{
    const auto descriptor = model.descriptor(); // capture the source descriptor once
    const auto count = descriptor.field<std::uint32_t>(0x24).value();
    observe(audit, 0x427150, 0, count);
    if (count <= 1)
        return;
    const auto head = descriptor.field<std::uint32_t>(0x48).value();
    observe(audit, 0x42715c, 0, head);
    // The source advances this head and passes it to427050, which never reads EDX.
    // No part payload, transform, LOD/max check or geometry borrow is justified.
    advance_records(records, count, audit);
}
void advance_track_part_animations(std::span<TrackPartAnimation> records, ModelResources &models,
                                   std::optional<std::size_t> handle, TrackPhaseAudit *audit)
{
    if (!handle)
        return;
    if (models.source_backed(*handle))
    {
        advance_track_part_animations(records, models.source_at(*handle), audit);
        return;
    }
    const auto &model = models.at(*handle);
    const auto size = model.part_count(model.source_lod_index(0));
    if (size > std::numeric_limits<std::uint32_t>::max())
        throw FormatError("track part count exceeds source word");
    const auto count = static_cast<std::uint32_t>(size);
    observe(audit, 0x427150, 0, count);
    if (count <= 1)
        return;
    // The decoded legacy owner has no mutable raw-head word. Do not fabricate one
    // for diagnostics; its valid imported count and shared record math are enough.
    advance_records(records, count, audit);
}
} // namespace sgn::retail
