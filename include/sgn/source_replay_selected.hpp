#pragma once
#include <cstdint>

namespace sgn::retail
{
class ReplayBank;
struct FrontendSourceStorage;
// Invocation-only adapter for the native result word captured at44DC4E.
// Existing439E90 representation: 0=null, slot+1=recording identity. This is
// NOT a raw PE address. Caller supplies that captured word from result+8
// (61C1F8 for44D740), and the same persistent ReplayBank used by439E90's world.
// Capacity below172 signed returns0 without resolving/dereferencing identity.
// Otherwise the source borrows the complete accessible suffix of that bank,
// preserving next-slot terminal-frame aliases and raw header+10 address bits.
// No replay copy, output clear, cursor rebasing or sentinel manufacture.
// Source-completed TimeTrial selection443330 selects slots3/4. A caller naming
// slot5 is bounded by the real bank end; external570AC0..570AEB is NOT padding.
// This leaf does not implement44D740's phases,437DA0,output copy or page switch.
std::uint32_t compress_selected_native_replay_44dc5b(FrontendSourceStorage &, const ReplayBank &,
                                                     std::uint32_t captured_native_identity, std::uint32_t capacity);
} // namespace sgn::retail
