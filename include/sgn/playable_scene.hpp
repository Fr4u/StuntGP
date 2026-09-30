#pragma once
#include "sgn/presentation_snapshot.hpp"

namespace sgn::retail
{
class RaceSession;
// Owns render snapshots and their backing storage. Update once after an
// accepted simulation tick; view/instances/live_views are read-only at Present.
class PlayableScene
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    explicit PlayableScene(RaceSession &);
    PlayableScene(RaceSession &, sgn::Renderer &);
    ~PlayableScene();
    PlayableScene(const PlayableScene &) = delete;
    PlayableScene &operator=(const PlayableScene &) = delete;
    PlayableScene(PlayableScene &&) = delete;
    PlayableScene &operator=(PlayableScene &&) = delete;
    void update_after_tick(RaceSession &);
    void update_after_tick(RaceSession &, sgn::Renderer &);
    sgn::ImmutablePresentationSnapshot snapshot() const noexcept;
    // Restore GPU resources after device loss from retained presentation data;
    // never run camera/world preparation or sample mutable Session owners.
    void rebind_renderer(sgn::Renderer &) const;
    void resize(RaceSession &, unsigned width, unsigned height);
    const sgn::View &view() const noexcept;
    std::span<const sgn::SceneInstance> instances() const noexcept;
    std::span<const sgn::LiveInstanceView> live_views() const noexcept;
};
} // namespace sgn::retail
