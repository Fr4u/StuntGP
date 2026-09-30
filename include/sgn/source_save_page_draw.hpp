#pragma once
#include "sgn/source_selection_page_draw.hpp"
#include <span>
namespace sgn::retail
{
// 45811B consumes the identity captured at458115. The two proven static replay
// buffers belong to this FrontendState's existing cold_suffix. No allocation or
// publication is performed; other identities remain an explicit owner gap.
std::uint32_t read_source_replay_time_45811b(FrontendState &, std::uint32_t captured_identity);
// Shared43D550 body. Writes the same frontend5FF3A8 scratch, returns its
// written NUL-inclusive prefix. Save row and background do not own copies.
std::span<const std::uint8_t> format_source_time_43d550(FrontendState &, std::uint32_t ticks);
// Render-only extension. SourceSaveNativePersistence keeps its sole IO interface.
class SourceSavePageDrawChildren : public SourceSelectionPageDrawChildren
{
  protected:
    // The retained RequiredServices adapter may forward the captured identity
    // to read_source_replay_time_45811b; no copied replay or substitute time.
    virtual std::uint32_t replay_time_word_45811b(FrontendState &, std::uint32_t identity) = 0;

  public:
    using SourceSelectionPageDrawChildren::SourceSelectionPageDrawChildren;
    void draw_cards_4581b0(FrontendState &, std::uint32_t page);
    void draw_progress_458730(FrontendState &, std::uint32_t page);
    void draw_confirmation_459070(FrontendState &, std::uint32_t page, std::uint32_t text);
    void replay_background_4580fc(FrontendState &, std::uint32_t mode);
};
} // namespace sgn::retail
