#pragma once
#include <Windows.h>
#include <ddraw.h>
#include <span>
#include <cstdint>

namespace sgn::retail
{
// Live views of571FD4 input and5723D4 output; no palette copies or ownership.
void convert_source_palette_423070(std::span<const std::uint8_t, 1024>, std::span<PALETTEENTRY, 256>);
std::uint32_t initialize_source_palette_4230b0(IDirectDraw *const &draw_61c378,
                                               IDirectDrawSurface4 *const &primary_61c368,
                                               IDirectDrawSurface4 *const &back_61c3a0,
                                               IDirectDrawPalette *&palette_61c384,
                                               std::span<const std::uint8_t, 1024> input_571fd4,
                                               std::span<PALETTEENTRY, 256> entries_5723d4);
} // namespace sgn::retail
