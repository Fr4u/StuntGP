#pragma once
#include <cstdint>
#include <span>

namespace sgn::retail
{
struct FrontendSourceStorage;
// Complete43B640 and its integer writer/quantizer children on accessible byte
// views. Both views borrow existing owners; no replay bank or output staging.
// capacity is the signed x86 EDX word, NOT the physical destination view size.
// capacity<172 (signed) returns0 before any source/destination access. Otherwise
// the source copies172 bytes, writes a control byte at destination+172, then
// checks128-byte headroom before each subsequent frame. Partial writes survive
// return0 or a host access boundary. Result is payload bytes only, excluding172.
// Source header pointer words are copied opaquely, never interpreted/relocated.
// A quantizer delta INT_MIN hits the source's nonterminating SAR loop and is an
// explicit host boundary. No fabricated value or automatic terminating frame.
std::uint32_t compress_source_replay_43b640(std::span<std::uint8_t> destination, std::uint32_t capacity,
                                            std::span<const std::uint8_t> recorded_source);

// 44DC56 destination: borrow the existing Frontend buffer5EB178..5F26A7 only.
//  Caller must borrow the actual recording selected by61C1F8; this API does not
//  invent that unresolved owner mapping, activate recording, copy to5E3C48,
//  publish save-page words, or add a game-loop call. Real437DA0 gives0 or30000.
std::uint32_t compress_source_replay_to_frontend_43b640(FrontendSourceStorage &, std::uint32_t capacity,
                                                        std::span<const std::uint8_t> recorded_source);
} // namespace sgn::retail
