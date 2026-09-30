#include "sgn/source_music_buffer_directsound.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/platform_audio.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dsound.h>
#include <bit>
#include <limits>
#include <list>
#include <algorithm>
#include <thread>

namespace sgn::retail
{
namespace
{
std::uint32_t bits(HRESULT hr)
{
    return static_cast<std::uint32_t>(hr);
}
std::span<std::uint8_t> region(void *pointer, DWORD length)
{
    if (!pointer && length)
        throw SourceMusicWaveUnknown("DirectSound NULL region with nonzero length");
    return {static_cast<std::uint8_t *>(pointer), length};
}
} // namespace
struct SourceMusicBufferDirectSound::Impl
{
    std::weak_ptr<RaceSessionPlatform> owner;
    const std::thread::id thread = std::this_thread::get_id();
    std::list<IDirectSoundBuffer *> buffers; // acquired COM references, not a token registry
    explicit Impl(const std::shared_ptr<RaceSessionPlatform> &p) : owner(p) {}
    ~Impl()
    {
        if (std::this_thread::get_id() != thread)
            std::terminate();
        for (auto *buffer : buffers)
            if (buffer)
                buffer->Release();
    }
    std::shared_ptr<RaceSessionPlatform> owning() const
    {
        auto p = owner.lock();
        if (!p)
            throw std::logic_error("DirectSound music owner expired");
        p->require_owner_thread();
        return p;
    }
    IDirectSoundBuffer *get(AudioBufferId id) const
    {
        const auto p = owning();
        (void)p;
        auto *pointer = reinterpret_cast<IDirectSoundBuffer *>(static_cast<std::uintptr_t>(id.value));
        const auto entry = std::find(buffers.begin(), buffers.end(), pointer);
        if (!pointer || entry == buffers.end())
            throw SourceMusicWaveUnknown("DirectSound foreign or released music identity");
        return *entry;
    }
};
SourceMusicBufferDirectSound::SourceMusicBufferDirectSound(std::shared_ptr<RaceSessionPlatform> owner)
{
    if (!owner)
        throw std::invalid_argument("DirectSound music requires owner");
    owner->require_owner_thread();
    impl_ = std::make_unique<Impl>(owner);
}
SourceMusicBufferDirectSound::~SourceMusicBufferDirectSound() = default;
void SourceMusicBufferDirectSound::clear_source_buffer()
{
    const auto owner = impl_->owning();
    auto &state = owner->state();
    source_music_clear_45fd40(state.audio_stream, state.audio_maintenance.music.buffer.borrowed(), *this);
}
std::uint32_t SourceMusicBufferDirectSound::create(RaceSessionState &state, SourceMusicBufferDescription &source,
                                                   AudioMusicBuffer &output, std::uint32_t outer)
{
    const auto owner = impl_->owning();
    if (&state != &owner->state() || &output != &state.audio_maintenance.music.buffer)
        throw std::logic_error("DirectSound music output owner differs");
    if (source.size != 20 || source.flags != 0xe8 || source.reserved != 0 || !source.format || outer != 0)
        throw std::invalid_argument("DirectSound descriptor differs from45FF57");
    if (source.format != &state.audio_stream.format)
        throw std::logic_error("DirectSound format owner differs");
    const auto identity = owner->source_audio_owner().source_device_identity();
    if (identity == 0 || identity != state.audio_initialization.device.value)
        throw std::logic_error("DirectSound requires same retained source device");
    auto *device = reinterpret_cast<IDirectSound *>(static_cast<std::uintptr_t>(identity));
    // A released stale source cell is still the actual output argument. Do
    // not dereference or implicitly retain/release its previous pointer.
    auto *prior = output.value.native_value();
    const auto &f = *source.format;
    // Caller supplies a format owner containing only18 recovered bytes.
    // Nonzero cbSize would make DirectSound inspect an unqualified extension.
    if (f.extra_bytes != 0)
        throw SourceMusicWaveUnknown("DirectSound format extension bytes unbound");
    WAVEFORMATEX native_format{f.tag,         f.channels,        f.samples_per_second, f.bytes_per_second,
                               f.block_align, f.bits_per_sample, f.extra_bytes};
    DSBUFFERDESC desc{};
    desc.dwSize = sizeof(desc); // native64-bit SDK descriptor, source size stays20
    desc.dwFlags = source.flags;
    desc.dwBufferBytes = source.bytes;
    desc.dwReserved = source.reserved;
    desc.lpwfxFormat = &native_format;
    impl_->buffers.push_back(nullptr); // allocate bookkeeping before native acquisition
    const auto slot = std::prev(impl_->buffers.end());
    const auto hr = device->CreateSoundBuffer(&desc, output.value.native_address(), nullptr);
    auto *created = output.value.native_value(); // native writes occurred in actual owner cell
    if (created == prior && FAILED(hr))
    {
        impl_->buffers.erase(slot);
        return bits(hr);
    }
    if (created)
        *slot = created;
    else
        impl_->buffers.erase(slot);
    // Keep nonnull output owned even on a failure HRESULT. Prior references
    // remain accounted for; a source overwrite does not imply Release.
    return bits(hr);
}
std::uint32_t SourceMusicBufferDirectSound::lock(AudioBufferId id, std::uint32_t offset, std::uint32_t count,
                                                 SourceMusicClearOutputs &output, std::uint32_t flags)
{
    auto *buffer = impl_->get(id);
    void *first = nullptr;
    DWORD bytes = 0;
    const auto hr = buffer->Lock(offset, count, &first, &bytes, nullptr, nullptr, flags);
    if (SUCCEEDED(hr))
    {
        output.first = region(first, bytes);
        output.first_bytes = bytes;
    }
    // Failed outputs are not promoted from initialized native scratch to known
    // source values. A caller that consumes them will stop explicitly.
    return bits(hr);
}
std::uint32_t SourceMusicBufferDirectSound::lock_split(AudioBufferId id, std::uint32_t offset, std::uint32_t count,
                                                       SourceMusicSplitLockOutputs &output, std::uint32_t flags)
{
    auto *buffer = impl_->get(id);
    void *first = nullptr;
    void *second = nullptr;
    DWORD first_bytes = 0, second_bytes = 0;
    const auto hr = buffer->Lock(offset, count, &first, &first_bytes, &second, &second_bytes, flags);
    if (SUCCEEDED(hr))
    {
        output.first = region(first, first_bytes);
        output.first_bytes = first_bytes;
        output.second = region(second, second_bytes);
        output.second_bytes = second_bytes;
    }
    return bits(hr);
}
std::uint32_t SourceMusicBufferDirectSound::unlock(AudioBufferId id, std::span<std::uint8_t> first, std::uint32_t bytes)
{
    return unlock_split(id, first, bytes, {}, 0);
}
std::uint32_t SourceMusicBufferDirectSound::unlock_split(AudioBufferId id, std::span<std::uint8_t> first,
                                                         std::uint32_t first_bytes, std::span<std::uint8_t> second,
                                                         std::uint32_t second_bytes)
{
    auto *buffer = impl_->get(id);
    if (first_bytes > first.size() || second_bytes > second.size())
        throw SourceMusicWaveUnknown("DirectSound Unlock count exceeds qualified region");
    return bits(buffer->Unlock(first.data(), first_bytes, second.data(), second_bytes));
}
std::uint32_t SourceMusicBufferDirectSound::release(AudioBufferId id)
{
    auto *buffer = impl_->get(id);
    const auto remaining = buffer->Release();
    auto *pointer = reinterpret_cast<IDirectSoundBuffer *>(static_cast<std::uintptr_t>(id.value));
    const auto slot = std::find(impl_->buffers.begin(), impl_->buffers.end(), pointer);
    *slot = nullptr; // our reference is gone even if others remain
    return remaining;
}
std::uint32_t SourceMusicBufferDirectSound::status(AudioBufferId id, std::uint32_t &output)
{
    DWORD value = output;
    const auto hr = impl_->get(id)->GetStatus(&value);
    output = value;
    return bits(hr);
}
std::uint32_t SourceMusicBufferDirectSound::cursors(AudioBufferId id, SourceMusicCursorOutputs &output)
{
    DWORD play = 0, write = 0;
    const auto hr = impl_->get(id)->GetCurrentPosition(&play, &write);
    if (SUCCEEDED(hr))
    {
        output.play = play;
        output.write_or_thread_id = write;
    }
    return bits(hr);
}
std::uint32_t SourceMusicBufferDirectSound::volume(AudioBufferId id, std::int32_t value)
{
    return bits(impl_->get(id)->SetVolume(value));
}
std::uint32_t SourceMusicBufferDirectSound::pan(AudioBufferId id, std::int32_t value)
{
    return bits(impl_->get(id)->SetPan(value));
}
std::uint32_t SourceMusicBufferDirectSound::play(AudioBufferId id, std::uint32_t a, std::uint32_t b,
                                                 std::uint32_t flags)
{
    return bits(impl_->get(id)->Play(a, b, flags));
}
std::uint32_t SourceMusicBufferDirectSound::stop(AudioBufferId id)
{
    return bits(impl_->get(id)->Stop());
}
std::uint32_t SourceMusicBufferDirectSound::position(AudioBufferId id, std::uint32_t value)
{
    return bits(impl_->get(id)->SetCurrentPosition(value));
}
} // namespace sgn::retail
