#include "sgn/source_cold_language.hpp"
#include "sgn/retail_game_text.hpp"

namespace sgn::retail
{
void set_source_language_445f70(std::uint32_t &requested_language, std::uint32_t language) noexcept
{
    requested_language = language; // 445F70
}
void refresh_source_language_445f80(const AssetStore &assets, const std::uint32_t &requested_language,
                                    GameTextState &text)
{
    const auto requested = requested_language;   // 445F80: retained ECX, later445D7B EBX
    if (requested != text.language)              // 445F86/445F8B/445F8D
        load_game_text(assets, requested, text); // 445F8F tail-call445D70
}
} // namespace sgn::retail
