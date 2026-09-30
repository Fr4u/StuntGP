#pragma once

namespace sgn::retail
{
class FrontendState;
class SourceMenuSampleNative;
// Full local401770/4011F0/401030, with the actual same-platform source461150
// receiver. Its spatial461030/460E70 boundary is still a reached exception,
// never a successful no-op. One invocation is one source update, not a frame.
void advance_source_score_popups_401770(FrontendState &, SourceMenuSampleNative &);
} // namespace sgn::retail
