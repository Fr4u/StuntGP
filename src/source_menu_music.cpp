#include "sgn/source_menu_music.hpp"
#include "sgn/source_music_calls.hpp"
#include "sgn/race_session_platform.hpp"
#include <stdexcept>

namespace sgn::retail
{
SourceMenuMusic::SourceMenuMusic(const std::shared_ptr<RaceSessionPlatform> &p)
    : platform_(p), thread_(std::this_thread::get_id())
{
    if (!p)
        throw std::invalid_argument("menu music requires the application platform");
    p->require_owner_thread();
    if (!p->source_construction_only_entry())
        throw std::invalid_argument("source menu music requires a source platform");
}
void SourceMenuMusic::start(std::uint32_t requested, std::uint32_t loop)
{
    if (std::this_thread::get_id() != thread_)
        throw std::logic_error("menu music called from a foreign thread");
    const auto p = platform_.lock();
    if (!p)
        throw std::logic_error("menu music application platform expired");
    // 45F8F0 disabled gates precede lookup of a runtime. The enabled branch
    // requires the existing installed binding and never creates a replacement.
    start_source_music_45f8f0(*p, requested, loop);
}
} // namespace sgn::retail
