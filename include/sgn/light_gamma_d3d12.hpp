#pragma once
#include "sgn/retail_lighting.hpp"
#include "sgn/polygon_submission.hpp"
#include "sgn/sdr_gamma.hpp"
#include <thread>

namespace sgn
{
// New platform adapter, paired with Renderer::draw_polygon_submission(ramp).
// One per application backend; no GPU/device pointer. It survives device loss.
// Every operation and destruction belongs to its construction thread.
class LightGammaD3D12 final : public retail::LightGammaDevice
{
    const std::thread::id owner_ = std::this_thread::get_id();
    SdrGammaRamp current_ = identity_sdr_gamma();
    std::uint64_t apply_count_{};
    void check_thread() const;

  public:
    LightGammaD3D12() = default;
    ~LightGammaD3D12() override;
    LightGammaD3D12(const LightGammaD3D12 &) = delete;
    LightGammaD3D12 &operator=(const LightGammaD3D12 &) = delete;
    LightGammaD3D12(LightGammaD3D12 &&) = delete;
    LightGammaD3D12 &operator=(LightGammaD3D12 &&) = delete;
    void apply(std::span<const std::uint16_t, 768>) override;
    SdrGammaRamp snapshot() const;
    std::uint64_t apply_count() const;
    // Called only after successful source capture/seal. Null leaves both old
    // display owners unchanged. No allocation, source call or source rollback.
    // Same-thread publication of one logical pair, not concurrent atomic state.
    void publish(ImmutablePolygonSubmission &displayed, SdrGammaRamp &displayed_ramp,
                 ImmutablePolygonSubmission completed) const;
};
} // namespace sgn
