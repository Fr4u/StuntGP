#include "sgn/source_scene_keyboard_prefix.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
SourceSceneKeyboardPrefixResult reset_source_scene_keyboard_42e542(const std::weak_ptr<RaceSessionPlatform> &borrow,
                                                                   SourceSceneKeyboardPrefixObserver *observer)
{
    const auto platform = borrow.lock();
    if (!platform)
        throw FormatError("keyboard scene reset platform expired");
    platform->require_owner_thread();
    auto &startup = platform->state().startup;
    static_assert(sizeof(std::uint32_t) == 4);
    static_assert(std::tuple_size_v<decltype(startup.fields_62dcf0)> == 64);
    static_assert(std::tuple_size_v<decltype(startup.fields_62d480)> == 64);
    for (std::uint32_t i = 0; i < 64; ++i)
    {
        startup.fields_62dcf0[i] = 0;
        if (observer)
            observer->cleared(0x42e542, 0x62dcf0 + 4 * i, startup);
    }
    for (std::uint32_t i = 0; i < 64; ++i)
    {
        startup.fields_62d480[i] = 0;
        if (observer)
            observer->cleared(0x42e54e, 0x62d480 + 4 * i, startup);
    }
    return {};
}
} // namespace sgn::retail
