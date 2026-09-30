#include "source_music_period_owner.hpp"
#include "sgn/audio_timer_capabilities.hpp"

#if !defined(_WIN32)
#error Installed music period calls require Windows WinMM.
#endif

namespace sgn::retail
{
namespace
{
class InstalledMusicPeriodCalls final : public SourceMusicPeriodCalls
{
  public:
    std::uint32_t begin(std::uint32_t period) noexcept override
    {
        return ::timeBeginPeriod(period);
    }
    std::uint32_t end(std::uint32_t period) noexcept override
    {
        return ::timeEndPeriod(period);
    }
};
} // namespace
std::shared_ptr<SourceMusicPeriodCalls> make_installed_music_period_calls()
{
    // Allocates an adapter only. It creates no timer request or native window.
    return std::make_shared<InstalledMusicPeriodCalls>();
}
} // namespace sgn::retail
