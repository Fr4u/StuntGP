#include "sgn/source_menu_language_binding.hpp"
#include "sgn/source_frontend_material_binding.hpp"
#include "sgn/retail_frontend_language_selection.hpp"
#include "sgn/retail_font_resources.hpp"
#include "sgn/race_session.hpp"
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
SourceMenuLanguageChildren::SourceMenuLanguageChildren(MenuKeyboardAcquisition &keys,
                                                       Win32MenuDirectInputStartup &input, CampaignScoreMusic &music,
                                                       SessionOuterFrameServices &frames, PolygonSubmissionSink &sink,
                                                       SourceFrontendMaterialBinding &materials,
                                                       std::uint8_t &character)
    : SourceSavePageDrawChildren(keys, input, music, frames, sink, materials.textures(), character),
      materials_(&materials)
{
}

std::int32_t SourceMenuLanguageChildren::select_language_440c40(FrontendState &frontend, std::uint32_t flag)
{
    frontend.shared_platform()->require_owner_thread();
    auto views = frontend.source.language_selection_view();
    // Preserve 440C49..440C92 before resolving input/fonts. These branches
    // cannot fail merely because a resource that the source never reads is absent.
    const auto count = std::bit_cast<std::int32_t>(views.catalog.catalog.back());
    if (count <= 0)
        return -1;
    if (count > 8)
        throw std::invalid_argument("frontend language catalog exceeds eight source records");
    bool matched = false;
    for (std::int32_t column = 0; column < count; ++column)
        if (views.catalog.catalog[9 + 9 * column] == flag)
        {
            matched = true;
            break;
        }
    if (!matched)
        return -1;

    // Current completed resource, not a remembered startup span. The existing
    // core admits complete input only; partial source reads remain unresolved.
    auto &memory = frontend.resource_memory();
    const auto input = memory.complete_input(memory.current_reference(), 0x440cfb);
    auto &fonts = materials_->fonts(); // validates live scene/material/backend generation
    if (&fonts != &frontend.world().ui.fonts)
        throw FormatError("menu language fonts do not belong to the frontend world");
    // All parsing, arena stores, wrap order and final current-language store
    // remain in the existing core. Its font-selection writes reach the live bank.
    return select_frontend_language_440c40(views, input, fonts.metrics(), flag).column;
}
} // namespace sgn::retail
