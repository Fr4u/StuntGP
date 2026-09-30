#include "sgn/source_sky_motion_prefix.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <bit>
#include <cstddef>
#include <limits>
#include <type_traits>

// Pinned PE4a1d234ce1f095a427bb7f2f0174294b8c85a8fdbbc29b62a3863156b2f16a1b.
// 42C7CA CALL462890;42C7CF..42C809 exact continuation. The final CALL is NOT run.
namespace sgn::retail
{
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(std::is_standard_layout_v<SkyTextureMotion>);
static_assert(sizeof(SkyTextureMotion) == 20 && offsetof(SkyTextureMotion, step_u) == 12 &&
              offsetof(SkyTextureMotion, step_v) == 16);
SourceSkyMotionBoundary produce_source_requests_through_sky_motion_42c809(RaceSessionPlatform &platform,
                                                                          LevelBlock block,
                                                                          SourceRequestObserver *producer_observer,
                                                                          SourceSearchReadObserver *search_observer,
                                                                          SourceSkyMotionStoreObserver *store_observer)
{
    auto prefix = produce_source_requests_through_sky_call_42c7ca(platform, block, producer_observer);
    if (prefix.boundary.next_site != 0x42c7cau || prefix.boundary.required_callee != 0x462890u ||
        prefix.boundary.reached_return)
        return {prefix.boundary, std::nullopt, std::nullopt};
    if (!prefix.arguments)
        throw SourceRequestError(0x42c7ca, "actual sky call prefix omitted arguments"); // NEW internal invariant
    // Resolve this platform's binding only after the call is actually reached.
    // An unbound/expired literal preserves all completed producer/clear stores.
    const auto eax = find_source_request_462890(prefix.arguments->haystack, platform.source_application_literals(),
                                                static_cast<std::uint32_t>(prefix.arguments->needle), search_observer);
    // 42C7CF removes the two source stack arguments; C++ call lifetime covers
    // that transport.42C7D2/42C7D4 tests only EAX's pointer VALUE, not its bytes.
    if (static_cast<bool>(eax))
    {
        auto &sky = platform.state().texture_animation.sky;
        std::uint32_t ordinal = 0;
        const auto store = [&](float &destination, std::uint32_t site, std::uint32_t target, std::uint32_t bits) {
            destination = std::bit_cast<float>(bits);
            ++ordinal;
            if (store_observer)
                store_observer->after_store({site, target, bits, 4, ordinal}, platform);
        };
        store(sky[0].step_u, 0x42c7d6, 0x57766c, 0x00000000u);
        store(sky[0].step_v, 0x42c7e0, 0x577670, 0x00000000u);
        store(sky[1].step_u, 0x42c7ea, 0x577680, 0x3a378034u);
        store(sky[1].step_v, 0x42c7f4, 0x577684, 0x3a378034u);
    }
    // 42C7FE reloads the existing cursor published by42C638;42C801 pushes
    // L_TRACK's address value.42C806 would pass this SAME local slot to child.
    // The child lacks its script/literal reader binding, so do not call it or
    // export the local slot's host address after this prefix returns.
    auto &cursor_slot = prefix.boundary.script_cursor;
    const SourceTrackSearchValues next{cursor_slot, SourceTrackKeyAddress::l_track};
    prefix.boundary.next_site = 0x42c809u;
    prefix.boundary.required_callee = 0x42c580u;
    return {prefix.boundary, eax, next};
}
} // namespace sgn::retail
