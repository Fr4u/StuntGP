#include "sgn/retail_model_draw_queue.hpp"
#include <limits>
#include <cstring>

namespace sgn::retail
{
void reset_model_draw_queues(ModelDrawQueue &regular, ModelDrawQueue &background) noexcept
{
    background.cursor = 0;
    regular.cursor = 0;
    background.records[0].model_identity = 0;
    regular.records[0].model_identity = 0;
}
void force_model_draw_flags(std::uint16_t &model_flags, std::span<const ModelDrawPartFlags> parts, std::uint32_t flags)
{
    model_flags |= 8;
    const auto mask = static_cast<std::uint16_t>((flags << 8) | 0x80u);
    for (const auto &part : parts)
        if (part.visibility == 1)
            part.flags |= mask;
}
void clear_model_draw_flags(std::uint16_t &model_flags) noexcept
{
    model_flags &= 0xfff7;
}
void append_model_draw(ModelDrawQueue &queue, SourcePmdModelView model, unsigned source_lod, std::uint32_t identity,
                       ModelTransform &pose, std::uint32_t flags, ViewMatrixArena &matrices)
{
    if (!identity || !queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("model draw queue lacks bounded command and terminator");
    const auto lod = model.source_lod_index(source_lod);
    allocate_outer_model_matrices(pose, matrices);
    allocate_model_part_matrices(model, source_lod, matrices);
    pose.tail[0] = 0;
    queue.records[*queue.cursor] = {identity, pose};
    const auto model_flags = model.descriptor().field<std::uint16_t>(2);
    if (flags)
    {
        model_flags.set(static_cast<std::uint16_t>(model_flags.value() | 8u));
        const auto mask = static_cast<std::uint16_t>((flags << 8) | 0x80u);
        for (unsigned i = 0; i < model.part_count(); ++i)
        {
            const auto part = model.part(lod, i);
            if (part.flags().value() == 1)
            {
                const auto field = part.flags_02();
                field.set(static_cast<std::uint16_t>(field.value() | mask));
            }
        }
    }
    else
        model_flags.set(static_cast<std::uint16_t>(model_flags.value() & 0xfff7u));
    ++*queue.cursor;
    queue.records[*queue.cursor].model_identity = 0;
}
void append_model_draw(ModelDrawQueue &queue, ModelResources &models, std::size_t handle, unsigned source_lod,
                       ModelTransform &pose, std::uint32_t flags, ViewMatrixArena &matrices)
{
    if (models.source_backed(handle))
        append_model_draw(queue, models.source_at(handle), source_lod, models.identity(handle), pose, flags, matrices);
    else
        append_model_draw(queue, models.at(handle), source_lod, models.identity(handle), pose, flags, matrices);
}
void append_model_draw(ModelDrawQueue &queue, ModelResource &model, unsigned source_lod, std::uint32_t identity,
                       ModelTransform &pose, std::uint32_t flags, ViewMatrixArena &matrices)
{
    if (!identity || !queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("model draw queue lacks bounded command and terminator");
    const auto resident = model.source_lod_index(source_lod);
    allocate_outer_model_matrices(pose, matrices);
    allocate_model_part_matrices(model, source_lod, matrices);
    pose.tail[0] = 0;
    queue.records[*queue.cursor] = {identity, pose};
    if (flags)
    {
        std::vector<ModelDrawPartFlags> parts;
        parts.reserve(model.part_count(resident));
        for (unsigned i = 0; i < model.part_count(resident); ++i)
            parts.push_back({model.source_part(resident, i).flags, model.source_part_flags_02(resident, i)});
        force_model_draw_flags(model.source_model_flags_02(), parts, flags);
    }
    else
        clear_model_draw_flags(model.source_model_flags_02());
    ++*queue.cursor;
    queue.records[*queue.cursor].model_identity = 0;
}
void append_source_model_draw_42b7f0(ModelDrawQueue &queue, SourcePmdModelView model, std::uint32_t identity,
                                     ModelTransform &pose, std::uint32_t flags, ViewMatrixArena &matrices)
{
    if (identity == 0)
        throw FormatError("source queue requires a nonnull actual model identity");
    allocate_source_outer_model_matrices_44fdb0(pose, matrices); // 42B7FC
    allocate_source_model_part_matrices_450150(model, matrices); // 42B803
    const auto cursor = queue.cursor;                            // 42B808: source queue pointer capture
    pose.tail[0] = 0;                                            // 42B815 precedes the reached queue payload store
    if (!cursor || *cursor >= queue.records.size() - 1)
        throw FormatError("source model queue lacks command and following terminator");
    queue.records[*cursor].model_identity = identity; // 42B81F
    static_assert(sizeof(ModelTransform) == 80);
    std::memcpy(&queue.records[*cursor].pose, &pose, sizeof(pose)); // 42B821: actual80-byte queue copy
    const auto descriptor = model.descriptor();
    if (flags != 0)
    { // 42B7B0, independently recaptures selector/pointer/count
        const auto selector = descriptor.field<std::uint32_t>(0x5c).value();
        const auto model_flags = descriptor.field<std::uint8_t>(2);
        model_flags.set(static_cast<std::uint8_t>(model_flags.value() | 8u));
        if (selector >= 4u)
            throw SourceAllocationError(0x42b7ba, "source flag selector leaves descriptor slots");
        auto current = descriptor.field<std::uint32_t>(0x48u + selector * 4u).value();
        auto count = descriptor.field<std::uint32_t>(0x24).value();
        const auto mask = static_cast<std::uint16_t>((flags << 8) | 0x80u);
        while (count != 0)
        {
            const auto part = model.captured_base_part_42b055(current);
            if (part.flags().value() == 1)
            {
                const auto field = part.flags_02();
                field.set(static_cast<std::uint16_t>(field.value() | mask));
            }
            if (current > std::numeric_limits<std::uint32_t>::max() - 0x40u)
                throw SourceAllocationError(0x42b7d2, "source flag part increment leaves represented offsets");
            current += 0x40u;
            --count;
        }
    }
    else
    {
        const auto field = descriptor.field<std::uint16_t>(2);
        field.set(static_cast<std::uint16_t>(field.value() & 0xfff7u));
    }
    // No callbacks above can alter this cursor. Retain the source final reread.
    if (!queue.cursor || *queue.cursor >= queue.records.size() - 1)
        throw FormatError("source queue advance outside owner");
    queue.cursor = *queue.cursor + 1u;
    queue.records[*queue.cursor].model_identity = 0;
}
} // namespace sgn::retail
