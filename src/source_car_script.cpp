#include "sgn/source_car_script.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_script_number.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include <optional>
#include <string_view>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
class Script final
{
    RaceSessionPlatform &platform_;
    SourceBootSearchBoundary start_;

  public:
    Script(RaceSessionPlatform &platform, SourceBootSearchBoundary start) : platform_(platform), start_(start) {}
    bool key(std::string_view name, std::optional<Word> &cursor) const
    {
        SourceBootSearchBoundary local;
        if (cursor)
            local = {start_.allocation, *cursor, false};
        // The verified formatted_key view ends at an actual written NUL.
        // Borrow includes that byte; no copied needle or script payload.
        const SourceScriptNeedle needle{{name.data(), name.size() + 1}, 0, false};
        if (find_source_script_key_42c580(platform_, start_, needle, &local) != 1)
            return false;
        cursor = local.offset;
        return true;
    }
    void number(Word at, Word &actual_destination) const
    {
        parse_source_number_42c6a0(platform_, {start_.allocation, at, start_.is_null}, actual_destination, nullptr);
    }
};
std::string_view formatted_key(const SourceCarKeyCall &call, Word site)
{
    if (call.initialized_prefix > call.bytes.size())
        throw SourceBootConfigError(site, "Formatter published an impossible local prefix");
    for (std::size_t n = 0; n < call.initialized_prefix; ++n)
        if (call.bytes[n] == 0)
            return {call.bytes.data(), n};
    throw SourceBootConfigError(site, "Formatted car key lacks written NUL");
}
void parse_field(RaceSessionPlatform &platform, SourceBootSearchBoundary input, SourceCarKeyFormatter &formatter,
                 Word LevelCarSettings::*field, Word initial, Word format_address, Word format_site, Word key_site)
{
    platform.require_owner_thread();
    auto &cars = platform.state().cars;
    // All six defaults precede the first formatter/search/read. Other words,
    // records and names remain in the same actual Session-owned array.
    for (auto &car : cars)
        car.settings.*field = initial;
    Script script(platform, input);
    std::optional<Word> local_cursor; // same [EBP-4] retained across iterations.
    SourceCarKeyCall key;             // same64-byte caller local, not a persistent key owner.
    for (Word index = 0; index < 6; ++index)
    {
        key.initialized_prefix = 0; // NEW written-byte observation only.
        formatter.format_car_key_46280c(platform, format_address, index + 1, key);
        const auto name = formatted_key(key, format_site);
        if (script.key(name, local_cursor))
        {
            if (!local_cursor)
                throw SourceBootConfigError(key_site, "Unwritten parser value pointer");
            script.number(*local_cursor, cars[index].settings.*field); // 42C6C8.
        }
    }
}
} // namespace
void configure_source_cars_42c9b0(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                  SourceCarKeyFormatter &formatter)
{
    // 42C9C0 defaults;42C9E2 formatter;42C9F3 key;42CA03 number;42CA19 RET.
    parse_field(platform, input, formatter, &LevelCarSettings::config, 1, 0x47782c, 0x42c9e2, 0x42ca03);
}
void configure_source_replays_42cb00(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                     SourceCarKeyFormatter &formatter)
{
    // 42CB10 defaults;42CB32 formatter;42CB43 key;42CB53 number;42CB69 RET.
    parse_field(platform, input, formatter, &LevelCarSettings::replay, 0, 0x47785c, 0x42cb32, 0x42cb53);
}
void configure_source_tyres_42ca20(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                   SourceCarKeyFormatter &formatter)
{
    // 42CA30 defaults;42CA52 formatter;42CA63 key;42CA73 number;42CA89 RET.
    parse_field(platform, input, formatter, &LevelCarSettings::tyre, 0, 0x47783c, 0x42ca52, 0x42ca73);
}
void configure_source_liveries_42ca90(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                      SourceCarKeyFormatter &formatter)
{
    // 42CAA0 defaults;42CAC2 formatter;42CAD3 key;42CAE3 number;42CAF9 RET.
    parse_field(platform, input, formatter, &LevelCarSettings::livery, 0, 0x47784c, 0x42cac2, 0x42cae3);
}
void configure_source_ai_42cb70(RaceSessionPlatform &platform, SourceBootSearchBoundary input,
                                SourceCarKeyFormatter &formatter)
{
    platform.require_owner_thread();
    auto &cars = platform.state().cars;
    // 42CB80/87: skill then mode per record, for all six records before search.
    //  Two independent parse_field calls would change this source ordering.
    for (auto &car : cars)
    {
        car.settings.ai_skill = 100;
        car.settings.ai_mode = 100;
    }
    Script script(platform, input);
    std::optional<Word> local_cursor; // one [EBP-4] for both keys and all cars.
    SourceCarKeyCall key;             // one local64, overwritten by each formatter call.
    for (Word index = 0; index < 6; ++index)
    {
        key.initialized_prefix = 0;
        formatter.format_car_key_46280c(platform, 0x47786c, index + 1, key); // 42CBAB
        if (script.key(formatted_key(key, 0x42cbab), local_cursor))
        { // 42CBBC
            if (!local_cursor)
                throw SourceBootConfigError(0x42cbcf, "Unwritten AI skill cursor");
            script.number(*local_cursor, cars[index].settings.ai_skill); // 42CBCF
        }
        key.initialized_prefix = 0;
        formatter.format_car_key_46280c(platform, 0x47787c, index + 1, key); // 42CBDE
        if (script.key(formatted_key(key, 0x42cbde), local_cursor))
        { // 42CBEF
            if (!local_cursor)
                throw SourceBootConfigError(0x42cbff, "Unwritten AI mode cursor");
            script.number(*local_cursor, cars[index].settings.ai_mode); // 42CBFF
        }
    } // 42CC04..0E: next record only after both children;42CC16 RET.
}
} // namespace sgn::retail
