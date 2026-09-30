#include "sgn/source_viewport_win64.hpp"
#include "sgn/retail_viewport.hpp"
#include "sgn/source_numeric_win64.hpp"
#include <cstddef>
#include <limits>
#include <type_traits>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#if !defined(_WIN32) || !defined(_M_X64) || defined(_M_ARM64EC)
#error The424BD0 receiver requires native Windows AMD64.
#endif
namespace
{
// Borrowed stack call record: only the entry CW needed by the SEH finalizer.
struct NativeCall
{
    std::uint16_t expected_cw{}, incoming_cw{};
    std::uint32_t snapshot_valid{};
    sgn::retail::SourceViewportObservation *observation{};
};
static_assert(offsetof(NativeCall, expected_cw) == 0 && offsetof(NativeCall, incoming_cw) == 2);
static_assert(offsetof(NativeCall, snapshot_valid) == 4 && offsetof(NativeCall, observation) == 8 &&
              sizeof(NativeCall) == 16);
// clang-format off: keep the original using-declaration order (SortUsingDeclarations would reorder it)
using sgn::retail::ViewportState;
using sgn::retail::SourceViewportObservation;
// clang-format on
static_assert(std::is_standard_layout_v<ViewportState>);
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
static_assert(offsetof(ViewportState, center_pixel_x) == 8 && offsetof(ViewportState, center_pixel_y) == 12);
static_assert(offsetof(ViewportState, center_x) == 16 && offsetof(ViewportState, center_y) == 20);
static_assert(offsetof(ViewportState, scale_x) == 24 && offsetof(ViewportState, scale_y) == 28);
static_assert(offsetof(ViewportState, origin_x) == 80 && offsetof(ViewportState, origin_y) == 84);
static_assert(std::is_standard_layout_v<SourceViewportObservation>);
static_assert(offsetof(SourceViewportObservation, fistp64) == 0 && offsetof(SourceViewportObservation, stores) == 16);
static_assert(sizeof(sgn::retail::SourceViewportStore) == 8);
static_assert(offsetof(SourceViewportObservation, store_count) == 64);
static_assert(offsetof(SourceViewportObservation, entry_cw) == 68 &&
              offsetof(SourceViewportObservation, exit_cw) == 70);
static_assert(offsetof(SourceViewportObservation, entry_sw) == 72 &&
              offsetof(SourceViewportObservation, exit_sw) == 74);
static_assert(offsetof(SourceViewportObservation, entry_mxcsr) == 76 &&
              offsetof(SourceViewportObservation, exit_mxcsr) == 80);
} // namespace
extern "C" std::uint32_t sgn_source_viewport_424bd0(sgn::retail::ViewportState *, std::uint32_t, std::uint32_t,
                                                    NativeCall *);

namespace sgn::retail
{
SourceViewportAdmission set_viewport_origin_win64_424bd0(ViewportState &state, std::uint32_t x, std::uint32_t y,
                                                         std::uint16_t expected, SourceViewportObservation *observation)
{
    NativeCall call{expected, 0, 0, observation};
    std::uint32_t result{};
    // POD locals only. Real Windows SEH, not an EHsc destructor assumed to run
    // for hardware exceptions. The finally also covers the assembly FRAME.
    __try
    {
        result = sgn_source_viewport_424bd0(&state, x, y, &call);
    }
    __finally
    {
        if (AbnormalTermination() && call.snapshot_valid != 0)
            sgn_source_write_x87_control(call.incoming_cw);
    }
    return static_cast<SourceViewportAdmission>(result);
}
} // namespace sgn::retail
