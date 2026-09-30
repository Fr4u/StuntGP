#pragma once
#include "sgn/world_model_draw.hpp"

namespace sgn::retail
{
// Field-specific source identities in descriptor+0C. Borrow only the supplied
// live draw owner; no pointer registry or retained pose copy is introduced.
inline std::uint32_t source_draw_pose_reference(const WorldModelDrawState &draws, const ModelTransform &pose)
{
    if (&pose == &draws.template_pose)
        return 0x4864e8;
    for (std::uint32_t i = 0; i < draws.background.records.size(); ++i)
    {
        if (&pose == &draws.background.records[i].pose)
            return 0x632cfc + i * 84;
        if (&pose == &draws.regular.records[i].pose)
            return 0x62f5dc + i * 84;
    }
    throw FormatError("source outer pose does not belong to this draw owner");
}
inline const ModelTransform &resolve_source_draw_pose(const WorldModelDrawState &draws, std::uint32_t reference)
{
    if (reference == 0x4864e8)
        return draws.template_pose;
    const auto resolve = [&](const ModelDrawQueue &queue, std::uint32_t base) -> const ModelTransform * {
        if (reference < base)
            return nullptr;
        const auto offset = reference - base;
        if (offset % 84 || offset / 84 >= queue.records.size())
            return nullptr;
        return &queue.records[offset / 84].pose;
    };
    if (const auto *pose = resolve(draws.background, 0x632cfc))
        return *pose;
    if (const auto *pose = resolve(draws.regular, 0x62f5dc))
        return *pose;
    throw FormatError("invalid source draw-pose reference");
}
} // namespace sgn::retail
