#pragma once
#include "sgn/source_transport_word.hpp"
#include "sgn/vehicle_math.hpp"
#include <bit>
#include <optional>

namespace sgn::retail
{
// Sole 4D5B30 payload, with NEW validity metadata. A legacy whole-matrix borrow
// cannot silently reinterpret an opaque lane. Source leaves explicitly borrow
// only the nine numeric lanes their pinned ASM reads.
// NEW R19 (perf_transport): shipped game (SGN_SOURCE_PROVENANCE=0) = a plain
// 16-float matrix: every stored word is known (unproduced words are the
// documented 0 default), so stores write the float bits and borrows are
// direct. Diagnostic build (=1): a 16-bit opaque lane mask plus the opaque
// transport words; a masked lane always holds a word with an opaque byte.
class SourceTransportMatrix
{
    VehicleMatrix values_{};
#if SGN_SOURCE_PROVENANCE
    std::array<SourceTransportWord, 16> opaque_{};
    std::uint16_t opaque_mask_{};
    bool opaque_lane(unsigned lane) const noexcept
    {
        return ((opaque_mask_ >> lane) & 1u) != 0;
    }
    bool any_opaque() const noexcept
    {
        return opaque_mask_ != 0;
    }
#else
    bool any_opaque() const noexcept
    {
        return false;
    } // not a constant expression: no C4127
#endif
    void check(unsigned lane, std::uint32_t site = 0) const
    {
        if (lane >= 16)
            throw std::out_of_range("source matrix lane");
#if SGN_SOURCE_PROVENANCE
        if (opaque_lane(lane))
            (void)opaque_[lane].numeric(site);
#else
        (void)site;
#endif
    }
    void check_all() const
    {
        if (any_opaque())
            for (unsigned i = 0; i < 16; ++i)
                check(i);
    }

  public:
    void store(unsigned lane, std::uint32_t value)
    {
        if (lane >= 16)
            throw std::out_of_range("source matrix store lane");
        *reinterpret_cast<volatile float *>(&values_[lane]) = std::bit_cast<float>(value);
#if SGN_SOURCE_PROVENANCE
        opaque_mask_ = static_cast<std::uint16_t>(opaque_mask_ & ~(1u << lane));
#endif
    }
    void store(unsigned lane, const SourceTransportWord &value)
    {
#if SGN_SOURCE_PROVENANCE
        if (value.is_known())
        {
            store(lane, value.numeric(0));
            return;
        }
        if (lane >= 16)
            throw std::out_of_range("source matrix transport lane");
        opaque_[lane] = value;
        opaque_mask_ = static_cast<std::uint16_t>(opaque_mask_ | (1u << lane)); // no fabricated physical float
#else
        store(lane, value.numeric(0));
#endif
    }
    SourceTransportWord transport(unsigned lane, std::uint32_t site) const
    {
        if (lane >= 16)
            throw std::out_of_range("source matrix transport read lane");
#if SGN_SOURCE_PROVENANCE
        if (opaque_lane(lane))
            return opaque_[lane];
#endif
        check(lane, site);
        return SourceTransportWord::known(std::bit_cast<std::uint32_t>(values_[lane]));
    }
    const float *xyz_data(std::uint32_t site) const
    {
        if (any_opaque())
            for (unsigned i : {0u, 1u, 2u, 4u, 5u, 6u, 8u, 9u, 10u})
                check(i, site);
        (void)site;
        return values_.data();
    }
    float &operator[](unsigned i)
    {
        check(i);
        return values_[i];
    }
    const float &operator[](unsigned i) const
    {
        check(i);
        return values_[i];
    }
    float *data()
    {
        check_all();
        return values_.data();
    }
    const float *data() const
    {
        check_all();
        return values_.data();
    }
    // NEW R18 (DESIGN A4): whole-matrix borrow for the native 41E510/413B60 and
    // 416420 leaves on the tick path. Every lane except 3 (413B60 local -04, X.W)
    // and 7 (local -14, Y.W) must be known. Lanes 3/7 keep their opaque flags:
    // the only native writers (vehicle_trails.cpp basis(), vehicle_model_basis.cpp)
    // copy destination lanes 3/7 back bit-for-bit (x.w=out[3]; y.w=out[7]) and no
    // native reader consumes them numerically (lanes 0-2,4-6,8-10 only). The
    // reference is call-scoped; never store it.
    VehicleMatrix &borrow_retaining_basis_w(std::uint32_t site)
    {
        if (any_opaque())
            for (unsigned i = 0; i < 16; ++i)
                if (i != 3 && i != 7)
                    check(i, site);
        (void)site;
        return values_;
    }
    operator VehicleMatrix &()
    {
        check_all();
        return values_;
    }
    operator const VehicleMatrix &() const
    {
        check_all();
        return values_;
    }
};
} // namespace sgn::retail
