#pragma once
#include "sgn/presentation_snapshot.hpp"

namespace sgn
{
struct PresentationDraw
{
    View view;
    std::vector<PresentationObject> objects;
    std::vector<SceneInstance> instances;
    std::vector<LiveInstanceView> live_views;
};
struct PresentationInterpolationStats
{
    std::uint64_t publications{}, samples{}, intermediate_samples{}, history_resets{};
    std::uint64_t camera_cuts{}, object_cuts{}, interpolated_parts{};
};
// A new platform policy: previous completed tick -> current completed tick.
// Publishing prepares allocations/decompositions. Sampling only writes owned
// draw scratch and counters; it cannot access or mutate game/resource owners.
class PresentationInterpolator
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    PresentationInterpolator();
    ~PresentationInterpolator();
    PresentationInterpolator(const PresentationInterpolator &) = delete;
    PresentationInterpolator &operator=(const PresentationInterpolator &) = delete;
    void publish(ImmutablePresentationSnapshot);
    void invalidate_history() noexcept;
    const PresentationDraw &sample(double alpha);
    const PresentationInterpolationStats &stats() const noexcept;
    ImmutablePresentationSnapshot current() const noexcept;
};
} // namespace sgn
