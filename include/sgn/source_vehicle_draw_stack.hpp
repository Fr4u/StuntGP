#pragma once
#include "sgn/source_transport_word.hpp"
#include "sgn/source_vehicle_update_dispatch.hpp"

namespace sgn::retail
{
// The retail instructions read these local DWORDs without producing them in
// the recovered frame. A binding must supply proven caller-stack provenance.
// No default zero, old PMD W, or native preview value is substituted.
struct SourceVehicleDrawStack
{
    virtual ~SourceVehicleDrawStack() = default;
    virtual SourceTransportWord transport(std::uint32_t f, std::uint32_t at, std::int32_t offset)
    {
        return SourceTransportWord::known(word(f, at, offset));
    }
    virtual std::uint32_t word(std::uint32_t function, std::uint32_t instruction, std::int32_t frame_offset) = 0;
};
class SourceVehicleDrawStackBoundary final : public FormatError
{
  public:
    const std::uint32_t function, instruction;
    const std::int32_t frame_offset;
    SourceVehicleDrawStackBoundary(std::uint32_t f, std::uint32_t at, std::int32_t offset)
        : FormatError("source vehicle presentation reaches an unproduced stack DWORD"), function(f), instruction(at),
          frame_offset(offset)
    {
    }
};
struct SourceVehicleDrawStackUnavailable final : SourceVehicleDrawStack
{
    std::uint32_t word(std::uint32_t f, std::uint32_t at, std::int32_t offset) override
    {
        throw SourceVehicleDrawStackBoundary(f, at, offset);
    }
};
// NEW invocation metadata for a proven source stack write. The pointer is only
// an identity between adjacent synchronous calls; it is dereferenced exclusively
// inside the graph's active presentation scope. No guest address is fabricated.
// NEW typed transport admits the three ordered414350 residues, and the separate
// 4155E2 glow residue, only while their exact graph call is active. Their numeric
// values remain unknown; every unmatched call and numeric request still requires
// the original receiver. This is not an extension of the proven near value1.
class SourceVehicleDrawKnownPrefix final : public SourceVehicleDrawStack
{
    SourceVehicleDrawStack &remaining_;
    RaceSessionPlatform *platform_{};
    unsigned slot_{};
    bool pending_{}, near_{}, active_{}, consumed_{}, glow_{};
    unsigned transport_reads_{};
    friend class SourceVehicleDrawGraph;
    void before_detail();
    void after_detail(RaceSessionPlatform &, bool completed_near);
    void enter_presentation(RaceSessionPlatform &);
    void leave_presentation() noexcept;
    void enter_glow(RaceSessionPlatform &);

  public:
    explicit SourceVehicleDrawKnownPrefix(SourceVehicleDrawStack &remaining) : remaining_(remaining) {}
    SourceVehicleDrawKnownPrefix(const SourceVehicleDrawKnownPrefix &) = delete;
    SourceVehicleDrawKnownPrefix &operator=(const SourceVehicleDrawKnownPrefix &) = delete;
    std::uint32_t word(std::uint32_t, std::uint32_t, std::int32_t) override;
    SourceTransportWord transport(std::uint32_t, std::uint32_t, std::int32_t) override;
};
} // namespace sgn::retail
