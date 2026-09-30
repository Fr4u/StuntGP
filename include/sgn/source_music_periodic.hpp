#pragma once
#include "sgn/source_music_initialization.hpp"

namespace sgn::retail
{
struct SourceMusicCursorOutputs
{
    std::optional<std::uint32_t> play;
    // EBP-18 is later reused as CreateThread's output thread-ID cell.
    std::optional<std::uint32_t> write_or_thread_id;
};
struct SourceMusicSplitLockOutputs
{
    std::optional<std::span<std::uint8_t>> first;
    std::optional<std::uint32_t> first_bytes;
    // Original second pointer slot aliases the incoming userdata DWORD.
    // Missing means its original owner-pointer bits have NOT been replaced;
    // never turn that state into a null region or a truncated host pointer.
    std::optional<std::span<std::uint8_t>> second;
    std::optional<std::uint32_t> second_bytes;
};
struct SourceMusicPeriodicIO : SourceMusicInitializationIO
{
    virtual std::uint32_t cursors(AudioBufferId, SourceMusicCursorOutputs &) = 0;
    virtual std::uint32_t milliseconds() = 0;
    virtual std::uint32_t lock_split(AudioBufferId, std::uint32_t offset, std::uint32_t bytes,
                                     SourceMusicSplitLockOutputs &, std::uint32_t flags) = 0;
    virtual std::uint32_t unlock_split(AudioBufferId, std::span<std::uint8_t> first, std::uint32_t first_bytes,
                                       std::span<std::uint8_t> second, std::uint32_t second_bytes) = 0;
    // Source CreateThread(NULL,0,callback,userdata,0,&local_write_cursor).
    // Source identities are resolved by the host, never cast to native pointers.
    // Host owns and closes its thread handle even though retail ignores it.
    virtual void completion_thread(std::weak_ptr<RaceSessionPlatform>, std::uint32_t callback, std::uint32_t userdata,
                                   std::optional<std::uint32_t> &thread_id_output) = 0;
};
// Owner-thread timer dispatch for source460510; zero/missing owners are handled
// by the dispatcher before invoking. Pending native callback/thread bindings
// must retain their own lifetime and marshal game state access to this owner.
void source_music_periodic_460510(std::shared_ptr<RaceSessionPlatform>, SourceMusicPeriodicIO &, SourceMusicWaveIO &);
} // namespace sgn::retail
