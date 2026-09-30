#pragma once
#include "sgn/source_pmd_model.hpp"
#include "sgn/retail_track_part_preparation.hpp"

namespace sgn::retail
{
class RaceSessionPlatform;
// Full 425DF0 -> 425B50 -> 425AE0 on reached source-bank fields and the SAME
// persistent5775D0..57764F scratch. Descriptor and part are separate captured
// identities;62448C is the independently published last-loader normal base.
// No geometry/table mirror or whole-part preflight. Reached writes survive a
// later host bounds/finite-input failure. PC53/nearest arithmetic; original
// float spills and retained interpolation accumulator. No FP-trap parity.
// Admitted domain: half of the low-WORD polygon count is nonzero, reached
// numerical operands/intermediates are finite, and actual owners contain
// reached fields. Zero divisor, stale owner and out-of-range access fail at
// the reached operation; there is no synthetic geometry or fallback input.
void prepare_source_animated_track_part_425df0(SourceAllocationBank &, const SourcePmdLoadingState &,
                                               SourcePmdByteView captured_descriptor, SourcePmdLocation captured_part,
                                               std::uint32_t kind, TrackPartPreparationScratch &);
// Full 426260, concrete same-platform owner binding. Scene 2 and absent track
// return before any PMD/bank/record access; count<=1 returns before+48.
// The real authored624484 records are reloaded after geometry preparation.
// An absent optional bank is materialized once from its proven PE-zero static
// storage only at the first reached record store, AFTER reading the authored
// kind. That is host storage construction, never replacement authored data.
// Existing records are never recreated; +3C/+40 and untouched slots survive.
// The admitted source static bank has 32 records; a reached 33rd store fails.
// Does not touch576CC8, cold tail, resource guards, phase tick or warm entry.
void initialize_source_track_parts_426260(RaceSessionPlatform &);
} // namespace sgn::retail
