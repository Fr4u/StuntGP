#pragma once
#include "sgn/retail_camera_frame_session.hpp"
#include "sgn/retail_model_draw_queue.hpp"
#include "sgn/live_scene_geometry.hpp"

namespace sgn::retail
{
struct PreparedModelDraw
{
    ModelDrawCommand command;
    unsigned source_lod{};
    CameraModelTransform outer;
    LiveModelViews views;
};
// Shared CPU preparation for the playable scene and headless world probes.
// Owns derived captures of consumed model commands and their matrices. Session
// owns the persistent source queues, model resources and matrix arena.
// This is the model/shadow subset of418180, not full442500: trail producers,
// strip publication, effects and impact rendering remain unconnected.
class PlayableVehicleStages final : public CameraFrameStages
{
    RaceSession &session_;
    ModelDrawQueue regular_, background_;
    std::vector<PreparedModelDraw> regular_views_, background_views_;
    std::array<std::optional<unsigned>, 6> cars_{}, shadows_{};

  public:
    explicit PlayableVehicleStages(RaceSession &);
    void stage_entities_442500(const CameraFrameSnapshot &) override;
    void prepare_world_42e870() override;
    void consume_model_views(const CameraFrameSnapshot &) override;
    // Borrowed outputs remain valid until the next accepted simulation tick.
    // Null means the actual active/visibility branch did not append a model.
    const ModelDrawCommand *car(unsigned slot) const;
    const ModelDrawCommand *shadow(unsigned slot) const;
    const PreparedModelDraw *car_view(unsigned slot) const;
    const PreparedModelDraw *shadow_view(unsigned slot) const;
    const PreparedModelDraw *static_view(std::uint32_t identity) const;
    const ModelDrawQueue &regular_queue() const noexcept
    {
        return regular_;
    }
    const ModelDrawQueue &background_queue() const noexcept
    {
        return background_;
    }
};
} // namespace sgn::retail
