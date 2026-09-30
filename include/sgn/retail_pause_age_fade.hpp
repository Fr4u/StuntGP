#pragma once
#include "sgn/retail_quad_submission.hpp"

namespace sgn::retail
{
struct PauseAgeFadeWorld
{
    std::span<std::uint32_t, 35> descriptor;              // 576C38, shared mutable UI descriptor
    std::span<PolygonSubmissionVertex, 4> vertices;       // actual retained58D818
    std::uint32_t &selected_material;                     // 61C5E4
    std::span<const PolygonSubmissionMaterial> materials; // live6227D4 bank
    const ViewportState &viewport;
    const std::uint32_t &frame_pause; // 61294C; equality to1, not truthiness
    const std::uint32_t &age;         // 612954; unsigned, retained and never reset here
};
// Full442200..4422B7, tail to the existing full432E00. Borrows must remain
// valid/nonoverlapping for the synchronous call. No timer tick or Session owner.
void draw_pause_age_fade_442200(PauseAgeFadeWorld, PolygonSubmissionSink &);
} // namespace sgn::retail
