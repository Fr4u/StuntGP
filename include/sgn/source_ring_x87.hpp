#pragma once
#include <array>
#include <cstdint>
#include <intrin.h>
namespace sgn::retail
{
// NEW host x87 transport. Individually ordered source ops.
//
// R19 (perf_entities): the ops execute directly on the host x87 unit. The
// earlier representation seeded a private FNSAVE image from the host at
// construction, applied every op to that image (FNSAVE host, FRSTOR image, op,
// FNSAVE image, FRSTOR host) and published it with FRSTOR at publish() and at
// destruction; recapture() re-seeded it from the host. Between construction and
// destruction the only code that runs besides these ops is x64 C++ (MSVC x64
// never emits x87) and, bracketed by publish()..recapture(), other RingX87
// users. Hence the private image equals the host x87 state before and after
// every op, including exception unwinding (the destructor published the image,
// which is the state the host now already holds). Registers (80-bit), TOP,
// tags, CW and SW evolve identically; FIP/FDP were host addresses before and
// remain so. Consequently publish()/recapture() and construction/destruction
// have no work left. Rule unchanged for callers: never call an x87 MASM helper
// between ops of a live RingX87 outside a publish()..recapture() window.
enum class RingX87Op : std::uint32_t
{
    fld32,
    fst32,
    fstp32,
    fadd32,
    fsub32,
    fmul32,
    fdivr32,
    fcom32,
    fcomp32,
    fld0,
    fld2,
    fld3,
    mul1,
    mul3,
    mul4,
    addp,
    pop3,
    pop0,
    sqrt,
    chs,
    abs,
    sub1,
    div1,
    exchange,
    sw,
    cw,
    load_cw,
    fistp64
};
// Former emulation entry points, kept exported and unchanged.
extern "C" void sgn_ring_x87_capture(void *) noexcept;
extern "C" void sgn_ring_x87_restore(const void *) noexcept;
extern "C" std::uint64_t sgn_ring_x87_step(void *, std::uint32_t, std::uint64_t) noexcept;
// One native instruction each. The QWORD operand slot is seeded with the
// argument, the instruction uses its low DWORD/WORD (or all eight bytes for
// FISTP m64), and the slot's final QWORD is returned: the same value the former
// sgn_ring_x87_step returned for the same op and argument.
extern "C"
{
    std::uint64_t sgn_ring_x87n_fld32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fst32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fstp32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fadd32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fsub32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fmul32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fdivr32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fcom32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fcomp32(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fld0(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fld2(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fld3(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_mul1(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_mul3(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_mul4(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_addp(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_pop3(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_pop0(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_sqrt(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_chs(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_abs(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_sub1(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_div1(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_exchange(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_sw(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_cw(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_load_cw(std::uint64_t) noexcept;
    std::uint64_t sgn_ring_x87n_fistp64(std::uint64_t) noexcept;
}
class RingX87
{
  public:
    RingX87() = default;
    ~RingX87() = default;
    RingX87(const RingX87 &) = delete;
    RingX87 &operator=(const RingX87 &) = delete;
    std::uint32_t op(RingX87Op code, std::uint32_t value = 0)
    {
        const std::uint64_t v = value;
        std::uint64_t r;
        switch (code)
        {
        case RingX87Op::fld32:
            r = sgn_ring_x87n_fld32(v);
            break;
        case RingX87Op::fst32:
            r = sgn_ring_x87n_fst32(v);
            break;
        case RingX87Op::fstp32:
            r = sgn_ring_x87n_fstp32(v);
            break;
        case RingX87Op::fadd32:
            r = sgn_ring_x87n_fadd32(v);
            break;
        case RingX87Op::fsub32:
            r = sgn_ring_x87n_fsub32(v);
            break;
        case RingX87Op::fmul32:
            r = sgn_ring_x87n_fmul32(v);
            break;
        case RingX87Op::fdivr32:
            r = sgn_ring_x87n_fdivr32(v);
            break;
        case RingX87Op::fcom32:
            r = sgn_ring_x87n_fcom32(v);
            break;
        case RingX87Op::fcomp32:
            r = sgn_ring_x87n_fcomp32(v);
            break;
        case RingX87Op::fld0:
            r = sgn_ring_x87n_fld0(v);
            break;
        case RingX87Op::fld2:
            r = sgn_ring_x87n_fld2(v);
            break;
        case RingX87Op::fld3:
            r = sgn_ring_x87n_fld3(v);
            break;
        case RingX87Op::mul1:
            r = sgn_ring_x87n_mul1(v);
            break;
        case RingX87Op::mul3:
            r = sgn_ring_x87n_mul3(v);
            break;
        case RingX87Op::mul4:
            r = sgn_ring_x87n_mul4(v);
            break;
        case RingX87Op::addp:
            r = sgn_ring_x87n_addp(v);
            break;
        case RingX87Op::pop3:
            r = sgn_ring_x87n_pop3(v);
            break;
        case RingX87Op::pop0:
            r = sgn_ring_x87n_pop0(v);
            break;
        case RingX87Op::sqrt:
            r = sgn_ring_x87n_sqrt(v);
            break;
        case RingX87Op::chs:
            r = sgn_ring_x87n_chs(v);
            break;
        case RingX87Op::abs:
            r = sgn_ring_x87n_abs(v);
            break;
        case RingX87Op::sub1:
            r = sgn_ring_x87n_sub1(v);
            break;
        case RingX87Op::div1:
            r = sgn_ring_x87n_div1(v);
            break;
        case RingX87Op::exchange:
            r = sgn_ring_x87n_exchange(v);
            break;
        case RingX87Op::sw:
            r = sgn_ring_x87n_sw(v);
            break;
        case RingX87Op::cw:
            r = sgn_ring_x87n_cw(v);
            break;
        case RingX87Op::load_cw:
            r = sgn_ring_x87n_load_cw(v);
            break;
        case RingX87Op::fistp64:
            r = sgn_ring_x87n_fistp64(v);
            break;
        default:
            __debugbreak();
            r = v;
            break; // former closed-enum INT3
        }
        return static_cast<std::uint32_t>(r);
    }
    void publish() {} // host already holds the state (see above)
    void recapture() {}
    std::uint32_t ftol()
    {
        const auto cw = op(RingX87Op::cw);
        op(RingX87Op::load_cw, cw | 0xc00u);
        const auto result = op(RingX87Op::fistp64);
        op(RingX87Op::load_cw, cw);
        return result;
    }
};
} // namespace sgn::retail
