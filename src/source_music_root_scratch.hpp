#pragma once
#include "sgn/source_music_timer_entry_prefix.hpp"
#include "sgn/retail_audio_startup.hpp"
#include <array>
#include <bit>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::retail {
static_assert(CHAR_BIT == 8 && sizeof(std::uint32_t) == 4);
static_assert(std::endian::native == std::endian::little);

// Private source local, not a source-state member, public scratch or token.
// Only this one prefix can create/use it. The separately authored test friend
// inspects this exact implementation; it is not present in production code.
class SourceMusicRootScratch final {
    std::array<std::uint8_t, 5> bytes_;
    SourceMusicRootScratch() noexcept {
        bytes_[0] = 0; //45FDB3 byte[EBP-18]=0
        //45FDB8 DWORD[EBP-17]=0: safe bytes, no misaligned uint32_t lvalue.
        bytes_[1] = 0;
        bytes_[2] = 0;
        bytes_[3] = 0;
        bytes_[4] = 0;
    }
    SourceMusicRootScratch(const SourceMusicRootScratch&) = delete;
    SourceMusicRootScratch& operator=(const SourceMusicRootScratch&) = delete;

    void copy_filename(const AudioStreamState& actual) noexcept {
        // The view aliases the actual first filename DWORD; creating a view
        // performs no snapshot. Only indices0..2 are read, on demand below.
        const auto filename = std::as_bytes(std::span{&actual.field_df54, std::size_t{1}});
        bool pad = false;
        for (std::size_t i = 0; i != 3; ++i) {
            if (pad) {
                bytes_[i] = 0;
            } else {
                // PE4633D5/4633D8: forward live byte read, then its write.
                // The ordinary62DF54 source is aligned; for count3 the other
                // alignment paths have the same bounded byte memory events.
                const auto value = std::to_integer<std::uint8_t>(filename[i]);
                bytes_[i] = value;
                pad = value == 0; //4633DB/4633DD; remaining writes zero-pad
            }
        }
        // Offsets3/4 retain prologue zero; no read of filename[3] or tail.
    }
    friend SourceMusicTimerPrefixObservation receive_source_music_timer_prefix_45fda0(
        std::shared_ptr<RaceSessionPlatform>, SourceMusicTimerCapsReceiving&);
    friend struct SourceMusicPrefixExecution;
    friend class SourceMusicDriveRootAccess;
    friend struct SourceMusicRootScratchTestAccess;
};
}
