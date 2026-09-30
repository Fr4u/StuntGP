#include "sgn/source_numeric_win64.hpp"
#include "sgn/source_numeric_control.hpp"
#include <limits>
#include <stdexcept>
#include <xmmintrin.h>

#if !defined(_WIN32) || !defined(_M_X64) || defined(_M_ARM64EC)
#error This source numeric receiver requires native Windows AMD64, not ARM64EC or x86.
#endif
#pragma fenv_access(on)

namespace sgn::retail
{
namespace
{
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559 && std::numeric_limits<float>::digits == 24);
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559 &&
              std::numeric_limits<double>::digits == 53);
static_assert(_MM_MASK_MASK == 0x1f80 && _MM_ROUND_MASK == 0x6000 && _MM_FLUSH_ZERO_MASK == 0x8000);
class NativeControl final : public SourceX87Control
{
  public:
    std::uint16_t fstcw() override
    {
        const auto word = sgn_source_read_x87_control();
        if ((word & 0x3fu) != 0x3fu)
            // R25: clang-format off keeps this message one string literal, as in the base (no literal split).
            // clang-format off
            throw std::runtime_error(
                "Win64 source numeric receiver requires masked x87 exceptions; unmasked trap equivalence is not implemented");
        // clang-format on
        return word;
    }
    void fldcw(std::uint16_t word) override
    {
        sgn_source_write_x87_control(word); // actual original46306E hardware boundary
        // New SSE compatibility policy, not a claimed original SSE operation.
        _mm_setcsr(source_compatible_mxcsr(_mm_getcsr(), word));
    }
};
} // namespace
void prepare_source_numeric_win64_429510()
{
    NativeControl native;
    prepare_source_numeric_429510(native);
}
} // namespace sgn::retail
