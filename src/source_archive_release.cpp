#include "sgn/source_boot_config.hpp"

namespace sgn::retail
{
void release_source_archive_44b1a0(SourceBootFileState &state) noexcept
{
    if (!state.archive)
        return;                               // 44B1A0..44B1A7; all other words retained
    SourceBootRawFree{}(state.archive.get()); // 44B1AA ->462F9E, actual CRT owner
    (void)state.archive.release();            // 44B1B2 pointer-null AFTER free; no second free
    state.archive_name[0] = 0;                // 44B1BC: only first byte, retain remaining31
    state.archive_size = 0;                   // 44B1C3
    state.readable_prefix = 0;                // host validity metadata for the released payload
}
} // namespace sgn::retail
