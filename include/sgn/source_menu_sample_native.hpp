#pragma once
#include "sgn/source_menu_feedback.hpp"
#include <memory>
#include <thread>

namespace sgn::retail
{
class RaceSessionPlatform;
// Source461150, including its461030/460E70 spatial branch, on the SAME
// retained DirectSound/sample owners used by startup. No preview audio path.
class SourceMenuSampleNative final : public SourceMenuSampleReceiving
{
    std::weak_ptr<RaceSessionPlatform> platform_;
    const std::thread::id thread_;

  public:
    explicit SourceMenuSampleNative(const std::shared_ptr<RaceSessionPlatform> &);
    void start_461150(FrontendState &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t, std::int32_t,
                      std::uint32_t, std::uint32_t *) override;
};
} // namespace sgn::retail
