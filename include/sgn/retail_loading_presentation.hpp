#pragma once
#include "sgn/retail_loading_sequence.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <optional>

namespace sgn::retail
{
class SessionOuterFrameServices;
struct LoadingPresentationWorld
{
    LoadingSequenceWorld sequence;
    std::uint32_t &tile_material_base; // same61B628 borrowed by sequence.frame.quads
};
// 445A40 consumer for the real42C510 loader. All references must outlive this
// object. Material spans are borrowed anew after each real reset/prelude load;
// no stale vector span is dereferenced after the loader changes its owner.
class RetailLoadingPresentation final : public WorldMaterialLoadingPresentation
{
  public:
    RetailLoadingPresentation(LoadingPresentationWorld, FrontendPerformanceCounter &, PolygonSubmissionSink &,
                              LoadingFrameDevice &);
    // Inert cold composition; borrows the real world/backend only when reached.
    RetailLoadingPresentation(SessionOuterFrameServices &, FrontendPerformanceCounter &, PolygonSubmissionSink &,
                              LoadingFrameDevice &);
    void run(WorldMaterialLoadingState &, WorldMaterialLoadingContext &, MaterialFileSource &,
             MaterialTextureBackend &) override;
    void finish_4459e0(WorldMaterialLoadingState &, WorldMaterialLoadingContext &, MaterialFileSource &,
                       MaterialTextureBackend &);

  private:
    void validate() const;
    void play(WorldMaterialLoadingState &, MaterialTextureBackend &, std::uint32_t frames, std::uint32_t hold,
              std::uint32_t fade_out);
    std::optional<LoadingPresentationWorld> world_;
    SessionOuterFrameServices *frames_{};
    std::uint32_t &tile_base(WorldMaterialLoadingState &, MaterialTextureBackend &);
    FrontendPerformanceCounter &counter_;
    PolygonSubmissionSink &sink_;
    LoadingFrameDevice &device_;
};
// One implementation; the name identifies the source entry, not a second renderer.
using WorldMaterialLoadingPresentation445A40 = RetailLoadingPresentation;
} // namespace sgn::retail
