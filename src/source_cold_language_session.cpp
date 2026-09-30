#include "sgn/source_cold_language.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
class PlatformLanguageAssets final : public GameTextAssetSource
{
    RaceSessionPlatform &platform_;

  public:
    explicit PlatformLanguageAssets(RaceSessionPlatform &platform) : platform_(platform) {}
    const AssetStore &assets() const override
    {
        return platform_.assets();
    }
};
} // namespace
void set_session_source_language_445f70(RaceSessionPlatform &platform, std::uint32_t language)
{
    platform.require_owner_thread();
    set_source_language_445f70(platform.state().menu.requested_language, language);
}
void refresh_session_source_language_445f80(RaceSessionPlatform &platform)
{
    platform.require_owner_thread();
    auto &world = platform.state();
    const auto requested = world.menu.requested_language; // 445F80, retained once
    if (requested == world.game_text.language)
        return; // 445F8D ->445F94: no asset borrow
    PlatformLanguageAssets source(platform);
    // 445F8F tail-calls445D70. Its single reset prefix precedes the lazy asset
    // borrow, so even an unprepared host backend preserves reached source writes.
    load_game_text(source, requested, world.game_text);
}
} // namespace sgn::retail
