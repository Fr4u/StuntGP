#include "sgn/source_music_wave_winmm.hpp"
#include "sgn/source_music_initialization.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <bit>
#include <climits>
#include <limits>
#include <map>
#include <vector>

namespace sgn::retail
{
namespace
{
std::array<DWORD, 5> words(const MMCKINFO &c)
{
    return {c.ckid, c.cksize, c.fccType, c.dwDataOffset, c.dwFlags};
}
MMCKINFO native_chunk(const SourceMusicWaveChunkAccess &access, bool all_known)
{
    std::array<DWORD, 5> v{};
    for (std::size_t i = 0; i < 5; ++i)
        v[i] = all_known ? access.require(i) : access.read(i).value_or(0);
    return {v[0], v[1], v[2], v[3], v[4]};
}
void publish(SourceMusicWaveChunkAccess &access, const MMCKINFO &chunk, bool succeeded)
{
    const auto v = words(chunk);
    for (std::size_t i = 0; i < 5; ++i)
    {
        // On failure an unknown zero output cannot be distinguished from the
        // unused zero scratch seed. Leave it unknown; caller closes and returns.
        if (succeeded || access.read(i) || v[i] != 0)
            access.store(i, v[i]);
    }
}
} // namespace
struct SourceMusicWaveWinmm::Impl
{
    std::weak_ptr<RaceSessionPlatform> owner;
    std::map<std::uint64_t, HMMIO> files;
    std::uint64_t next = 1;
    explicit Impl(const std::shared_ptr<RaceSessionPlatform> &value) : owner(value) {}
    ~Impl()
    {
        for (const auto &[id, handle] : files)
        {
            (void)id;
            if (handle)
                (void)mmioClose(handle, 0);
        }
    }
    std::shared_ptr<RaceSessionPlatform> require_owner() const
    {
        auto retained = owner.lock();
        if (!retained)
            throw std::logic_error("MMIO owner expired");
        retained->require_owner_thread();
        return retained;
    }
    HMMIO require_file(AudioFileId id) const
    {
        const auto retained = require_owner();
        (void)retained;
        const auto found = files.find(id.value);
        if (found == files.end() || !found->second)
            throw SourceMusicWaveUnknown("MMIO foreign or already closed source handle");
        return found->second;
    }
};
SourceMusicWaveWinmm::SourceMusicWaveWinmm(std::shared_ptr<RaceSessionPlatform> owner)
{
    if (!owner)
        throw std::invalid_argument("MMIO requires platform owner");
    owner->require_owner_thread();
    impl_ = std::make_unique<Impl>(owner);
}
SourceMusicWaveWinmm::~SourceMusicWaveWinmm() = default;
std::uint32_t SourceMusicWaveWinmm::open_source_file()
{
    const auto owner = impl_->require_owner();
    return source_music_wave_open_45ffd0(owner->state().audio_stream, *this);
}
std::uint32_t SourceMusicWaveWinmm::read_source_stream(std::span<std::uint8_t> destination, std::uint32_t requested)
{
    const auto owner = impl_->require_owner();
    return source_music_read_4604e0(owner->state().audio_stream, destination, requested, *this);
}
AudioFileId SourceMusicWaveWinmm::open(AudioStreamState &s, std::array<std::uint32_t, 18> &info, std::uint32_t flags)
{
    const auto owner = impl_->require_owner();
    if (&s != &owner->state().audio_stream)
        throw std::logic_error("MMIO stream owner differs");
    if (flags != 0x10000u || info[5] != 0x400u)
        throw std::invalid_argument("MMIO unexpected open mode");
    for (std::size_t i = 0; i < info.size(); ++i)
        if (i != 5 && info[i] != 0)
            throw std::invalid_argument("MMIO unqualified nonzero source MMIOINFO");
    auto filename = s.filename();
    if (std::find(filename.begin(), filename.end(), std::uint8_t{0}) == filename.end())
        throw SourceMusicWaveUnknown("MMIO filename lacks terminator in source260 bytes");
    if (impl_->next == std::numeric_limits<std::uint64_t>::max())
        throw std::overflow_error("MMIO identities exhausted");
    const auto id = impl_->next++;
    // Allocate accounting BEFORE opening; there is no throwing insertion after acquisition.
    auto [slot, inserted] = impl_->files.emplace(id, nullptr);
    if (!inserted)
        throw std::logic_error("MMIO duplicate identity");
    MMIOINFO native{};
    native.cchBuffer = 1024;
    const auto handle = mmioOpenA(reinterpret_cast<char *>(filename.data()), &native, flags);
    slot->second = handle;
    s.filename(filename);       // actual ANSI buffer writes, including failure
    info[3] = native.wErrorRet; // the only scalar error output consumed by callers
    // Pointer-width output slots are not projected into fake retail DWORDs.
    if (!handle)
    {
        impl_->files.erase(slot);
        return {};
    }
    return {id};
}
std::uint32_t SourceMusicWaveWinmm::descend(AudioFileId file, SourceMusicWaveChunkAccess &output,
                                            SourceMusicWaveChunkAccess *parent, std::uint32_t flags)
{
    const auto handle = impl_->require_file(file);
    if (flags != 0 && flags != 0x10u)
        throw std::invalid_argument("MMIO descend flags outside source calls");
    if (flags == 0x10u)
        (void)output.require(0);
    // WinMM reads only search ID when FINDCHUNK is set; other destination
    // members are outputs. No source local is marked zero by this scratch seed.
    auto chunk = native_chunk(output, false);
    auto parent_chunk = parent ? native_chunk(*parent, true) : MMCKINFO{};
    const auto result = mmioDescend(handle, &chunk, parent ? &parent_chunk : nullptr, flags);
    publish(output, chunk, result == 0);
    return result;
}
std::uint32_t SourceMusicWaveWinmm::ascend(AudioFileId file, SourceMusicWaveChunkAccess &output, std::uint32_t flags)
{
    const auto handle = impl_->require_file(file);
    if (flags != 0)
        throw std::invalid_argument("MMIO ascend flags outside source calls");
    auto chunk = native_chunk(output, true);
    const auto result = mmioAscend(handle, &chunk, flags);
    publish(output, chunk, true); // fully known before call, including error path
    return result;
}
std::uint32_t SourceMusicWaveWinmm::read_locked(AudioFileId file, std::span<std::uint8_t> destination,
                                                std::uint32_t requested)
{
    const auto handle = impl_->require_file(file);
    if (requested > destination.size() || requested > static_cast<std::uint32_t>(LONG_MAX))
        throw SourceMusicWaveUnknown("MMIO read outside actual host allocation or signed LONG domain");
    const auto result = mmioRead(handle, reinterpret_cast<char *>(destination.data()), static_cast<LONG>(requested));
    return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(result));
}
std::uint32_t SourceMusicWaveWinmm::read(AudioFileId file, SourceMusicWaveBytes &output, std::uint32_t requested)
{
    if (requested > output.size() || requested > static_cast<std::uint32_t>(LONG_MAX))
        throw SourceMusicWaveUnknown("MMIO full read exceeds source low16 allocation");
    std::vector<std::uint8_t> scratch(requested);
    const auto result = read_locked(file, scratch, requested);
    if (std::bit_cast<std::int32_t>(result) >= 0)
    {
        if (result > requested)
            throw std::logic_error("MMIO returned count beyond request");
        for (std::uint32_t i = 0; i < result; ++i)
            output.store(i, scratch[i]);
    }
    return result; // untouched tail remains unknown, not zero-filled PCM padding
}
std::uint32_t SourceMusicWaveWinmm::seek(AudioFileId file, std::uint32_t offset, std::uint32_t origin)
{
    const auto handle = impl_->require_file(file);
    if (origin != 0)
        throw std::invalid_argument("MMIO unexpected seek origin");
    const auto result = mmioSeek(handle, std::bit_cast<std::int32_t>(offset), SEEK_SET);
    return std::bit_cast<std::uint32_t>(static_cast<std::int32_t>(result));
}
std::uint32_t SourceMusicWaveWinmm::close(AudioFileId file, std::uint32_t flags)
{
    const auto handle = impl_->require_file(file);
    if (flags != 0)
        throw std::invalid_argument("MMIO close flags outside source calls");
    const auto result = mmioClose(handle, flags);
    // Do not retry in destructor after a failed Close: liveness is unresolved.
    impl_->files.at(file.value) = nullptr;
    return result;
}
} // namespace sgn::retail
