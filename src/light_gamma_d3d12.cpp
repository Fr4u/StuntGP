#include "sgn/light_gamma_d3d12.hpp"
#include <algorithm>
#include <exception>
#include <limits>
#include <utility>

namespace sgn
{
void LightGammaD3D12::check_thread() const
{
    if (std::this_thread::get_id() != owner_)
        throw std::logic_error("gamma adapter used outside owner thread");
}
LightGammaD3D12::~LightGammaD3D12()
{
    if (std::this_thread::get_id() != owner_)
        std::terminate();
}
void LightGammaD3D12::apply(std::span<const std::uint16_t, 768> ramp)
{
    check_thread();
    if (apply_count_ == (std::numeric_limits<std::uint64_t>::max)())
        throw FormatError("gamma apply counter exhausted");
    std::copy(ramp.begin(), ramp.end(), current_.begin());
    ++apply_count_;
}
SdrGammaRamp LightGammaD3D12::snapshot() const
{
    check_thread();
    return current_;
}
std::uint64_t LightGammaD3D12::apply_count() const
{
    check_thread();
    return apply_count_;
}
void LightGammaD3D12::publish(ImmutablePolygonSubmission &displayed, SdrGammaRamp &displayed_ramp,
                              ImmutablePolygonSubmission completed) const
{
    check_thread();
    if (!completed)
        return;
    displayed_ramp = current_;
    displayed = std::move(completed);
}
} // namespace sgn
