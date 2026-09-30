#pragma once
#include "sgn/retail_audio_startup.hpp"
#include <array>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace sgn::retail
{
// A host stop is not an HRESULT returned by retail. Unwritten stack/heap bytes
// remain absent, including the two bytes beyond a 16-byte PCM fmt chunk.
struct SourceMusicWaveUnknown : std::runtime_error
{
    using std::runtime_error::runtime_error;
};
// Specific4600FC stop. Physical small-heap capacity and the historical
// payload contents are not equivalent to the requested/read chunk size.
struct SourceMusicFmtTailUnknown final : SourceMusicWaveUnknown
{
    static constexpr std::uint32_t source_site = 0x4600fc;
    static constexpr std::uint32_t user_offset = 16, read_width = 2;
    static constexpr std::uint32_t small_heap_payload_if_success = 24;
    const std::uint32_t chunk_size;
    explicit SourceMusicFmtTailUnknown(std::uint32_t size)
        : SourceMusicWaveUnknown("4600FC fmt tail: unresolved allocator branch and prior bytes at user+16/+17"),
          chunk_size(size)
    {
    }
};
class SourceMusicWaveBytes
{
    std::vector<std::optional<std::uint8_t>> bytes_;

  public:
    explicit SourceMusicWaveBytes(std::size_t count) : bytes_(count) {}
    std::size_t size() const noexcept
    {
        return bytes_.size();
    }
    void store(std::size_t offset, std::uint8_t value);
    std::uint32_t load(std::size_t offset, std::size_t width) const;
};
// The same local MMCKINFO is reused by fmt descend, ascend and data descend.
// A global view writes the stream immediately, including on API failure.
class SourceMusicWaveChunkAccess
{
    AudioStreamState *stream_{};
    std::array<std::optional<std::uint32_t>, 5> local_;

  public:
    SourceMusicWaveChunkAccess() = default;
    explicit SourceMusicWaveChunkAccess(AudioStreamState &stream) : stream_(&stream) {}
    std::optional<std::uint32_t> read(std::size_t field) const;
    std::uint32_t require(std::size_t field) const;
    void store(std::size_t field, std::uint32_t value);
};
// Synchronous transport. Borrowed references must not be retained. filename_owner
// names the actual stream filename, not a copied path captured before callbacks.
// Native MMIOINFO layout is not used on a 64-bit host: these are 18 retail DWORDs.
// read receives BOTH allocated capacity and the original full requested DWORD.
struct SourceMusicWaveIO
{
    virtual ~SourceMusicWaveIO() = default;
    virtual AudioFileId open(AudioStreamState &filename_owner, std::array<std::uint32_t, 18> &info,
                             std::uint32_t flags) = 0;
    virtual std::uint32_t descend(AudioFileId, SourceMusicWaveChunkAccess &, SourceMusicWaveChunkAccess *parent,
                                  std::uint32_t flags) = 0;
    virtual std::uint32_t read(AudioFileId, SourceMusicWaveBytes &allocation, std::uint32_t requested_bits) = 0;
    virtual std::uint32_t ascend(AudioFileId, SourceMusicWaveChunkAccess &, std::uint32_t flags) = 0;
    virtual std::uint32_t seek(AudioFileId, std::uint32_t offset_bits, std::uint32_t origin) = 0;
    virtual std::uint32_t close(AudioFileId, std::uint32_t flags) = 0;
};
struct SourceMusicLockedReadIO
{
    virtual ~SourceMusicLockedReadIO() = default;
    virtual std::uint32_t read_locked(AudioFileId, std::span<std::uint8_t>, std::uint32_t) = 0;
};
std::uint32_t source_music_wave_open_45ffd0(AudioStreamState &, SourceMusicWaveIO &);
} // namespace sgn::retail
