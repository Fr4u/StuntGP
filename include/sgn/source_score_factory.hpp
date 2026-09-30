#pragma once
#include <cstdint>
namespace sgn::retail
{
class FrontendState;
class SourceMenuSampleNative;
void create_source_score_popup_401220(FrontendState &, SourceMenuSampleNative &, std::uint32_t category,
                                      std::uint32_t variant, std::uint32_t points);
} // namespace sgn::retail
