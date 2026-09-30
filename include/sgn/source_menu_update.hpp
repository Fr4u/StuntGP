#pragma once
#include "sgn/retail_frontend_state.hpp"

namespace sgn::retail
{
// Synchronous children whose complete implementations must be supplied by the
// application. Arguments borrow its existing frontend; no private state bank.
struct SourceMenuUpdateChildren
{
    virtual ~SourceMenuUpdateChildren() = default;
    virtual void update_words_436ed0(FrontendState &) = 0;
    virtual void update_navigation_43eff0(FrontendState &) = 0;
    virtual void apply_options_43c000(FrontendState &) = 0;
    virtual std::int32_t select_language_440c40(FrontendState &, std::uint32_t) = 0;
};
struct SourceMenuDrawChildren
{
    virtual ~SourceMenuDrawChildren() = default;
    virtual void draw_pages_43f6b0(FrontendState &) = 0;
    virtual void submit_lists_437ec0(FrontendState &) = 0;
};
// Full 43EFC0, signed truncation after the original wrapping *100.
bool source_menu_transition_ready_43efc0(const FrontendSourceStorage &) noexcept;
std::uint32_t select_source_menu_language_452d70(FrontendState &, std::uint32_t, SourceMenuUpdateChildren &);
// Full parents 43D200 and 43D250; required children are never skipped.
void update_source_menu_43d200(FrontendState &, SourceMenuUpdateChildren &);
void draw_source_menu_43d250(FrontendState &, SourceMenuDrawChildren &);
} // namespace sgn::retail
