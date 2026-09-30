#include "sgn/source_application_palette_win32.hpp"
#include <stdexcept>

namespace sgn::retail
{
static_assert(sizeof(PALETTEENTRY) == 4);
void convert_source_palette_423070(std::span<const std::uint8_t, 1024> input, std::span<PALETTEENTRY, 256> entries)
{
    auto *destination = reinterpret_cast<unsigned char *>(entries.data());
    // Sequential byte reads/stores preserve overlap, unlike an RGB snapshot.
    for (std::size_t offset = 0; offset < 1024; offset += 4)
    {
        destination[offset] = input[offset + 1];
        destination[offset + 1] = input[offset + 2];
        destination[offset + 2] = input[offset + 3];
        destination[offset + 3] = 1;
    }
    destination[3] = 0x40;
    destination[1023] = 0x40;
}
std::uint32_t initialize_source_palette_4230b0(IDirectDraw *const &draw, IDirectDrawSurface4 *const &primary,
                                               IDirectDrawSurface4 *const &back, IDirectDrawPalette *&palette,
                                               std::span<const std::uint8_t, 1024> input,
                                               std::span<PALETTEENTRY, 256> entries)
{
    std::uint32_t result = 0;
    const HDC dc = GetDC(nullptr);
    if ((static_cast<std::uint32_t>(GetDeviceCaps(dc, 0x26)) & 0x100u) != 0)
    {
        (void)GetSystemPaletteEntries(dc, 0, 256, entries.data());
        convert_source_palette_423070(input, entries);
        auto *receiver = draw;
        if (!receiver)
            throw std::logic_error("4230EF reached null DirectDraw");
        const HRESULT created = receiver->CreatePalette(0x0cu, entries.data(), &palette, nullptr);
        // Original turns a nonzero creation HRESULT into0, then skips both
        // surface calls. This unusual result is retained, not repaired.
        if (created == 0)
        {
            auto *first = primary;
            auto *retained_palette = palette; // captured once for BOTH calls
            if (!first)
                throw std::logic_error("42310C reached null primary surface");
            (void)first->SetPalette(retained_palette);
            auto *second = back; // reread after first surface call
            if (!second)
                throw std::logic_error("42311A reached null back surface");
            result = static_cast<std::uint32_t>(second->SetPalette(retained_palette));
        }
    }
    // No exception-finally: this is the reached ordinary source tail only.
    (void)ReleaseDC(nullptr, dc);
    return result;
}
} // namespace sgn::retail
