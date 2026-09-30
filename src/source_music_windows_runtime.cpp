#include "sgn/source_music_windows_runtime.hpp"
#include "sgn/source_music_period_host.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/race_session.hpp"
#include "sgn/source_boot_config.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <mmsystem.h>
#include <atomic>
#include <algorithm>
#include <filesystem>
#include <string>

namespace sgn::retail
{
namespace
{
// NEW HOST (audio-r36): ANSI spelling of the relocated native path for the ANSI source APIs
// (GetFullPathNameA, GetDriveTypeA, GetDiskFreeSpaceA, mmioOpenA). A path that the active code page cannot
// represent, or that does not fit the source's 260-byte local, uses its 8.3 short form; if neither is usable
// the source spelling is kept unchanged (the pre-audio-r36 behaviour).
std::string ansi_native_spelling(const std::filesystem::path &path, std::string fallback)
{
    constexpr std::size_t source_local = 260; // 45FB50 local and the 463390 strncpy extent
    const auto convert = [](const std::wstring &wide, std::string &out) {
        if (wide.empty())
            return false;
        BOOL lossy = FALSE;
        const int size =
            WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.c_str(), -1, nullptr, 0, nullptr, &lossy);
        if (size <= 1 || lossy)
            return false;
        std::string bytes(static_cast<std::size_t>(size), '\0');
        if (WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, wide.c_str(), -1, bytes.data(), size, nullptr, &lossy) !=
                size ||
            lossy)
            return false;
        bytes.pop_back(); // terminator
        out = std::move(bytes);
        return true;
    };
    std::string out;
    if (convert(path.native(), out) && out.size() < source_local)
        return out;
    std::wstring shorter(32768, L'\0');
    const auto length = GetShortPathNameW(path.c_str(), shorter.data(), static_cast<DWORD>(shorter.size()));
    if (length != 0 && length < shorter.size())
    {
        shorter.resize(length);
        if (convert(shorter, out) && out.size() < source_local)
            return out;
    }
    return fallback;
}
class MusicOpenWindowsPath final : public SourceMusicOpenPathIO
{
    SourceApplicationLiteralRef literals_;
    const NativeSourceBootFilePlatform &files_; // NEW HOST (audio-r36) relocation of the source base

  public:
    MusicOpenWindowsPath(SourceApplicationLiteralRef literals, const NativeSourceBootFilePlatform &files)
        : literals_(std::move(literals)), files_(files)
    {
    }
    void fullpath_463f7b(std::span<const char> input, std::array<std::optional<std::uint8_t>, 260> &local) override
    {
        if (input.empty() || input.front() == 0)
        {
            // 463F7B ->46A162 ->46A175 with drive0, caller buffer nonnull260.
            // 46A1E4 uses its OWN local260, then copies only on success.
            std::array<char, 260> cwd{}; // host scratch, not the caller's local
            const auto length = ::GetCurrentDirectoryA(260, cwd.data());
            // 46A1EA/46A1F4 return null without copying when the API fails
            // or length+1 exceeds260. The outer45FB50 ignores that return;
            // its subsequent strncpy encounters the unchanged unknown local.
            if (length == 0 || length >= 260)
                return;
            // 46A23A strcpy includes the terminator, not trailing padding.
            for (std::size_t i = 0; i <= length; ++i)
                local[i] = static_cast<std::uint8_t>(cwd[i]);
            return;
        }
        if (std::find(input.begin(), input.end(), '\0') == input.end())
            throw SourceMusicWaveUnknown("463F7B input string lacks qualified terminator");
        std::array<char, 260> native{}; // host scratch, never publish unwritten padding
        char *file_part = nullptr;
        // NEW HOST (audio-r36): the source spelling "<GAMELOCATION>\Sound\Music\TrackN.wav" names the virtual
        // install location. Resolve it to the local retail file like every other loose-file read, so the
        // following 45FE17 GetDriveTypeA / 45FE45 GetDiskFreeSpaceA see the real volume root and 45FFD0 mmioOpenA
        // opens the real WAV. Source call order, arguments and failure branches are unchanged.
        const std::string_view spelling(input.data());
        const auto relocated = ansi_native_spelling(files_.native_path(spelling), std::string(spelling));
        const auto length = GetFullPathNameA(relocated.c_str(), 260, native.data(), &file_part);
        if (length >= 260)
        {
            literals_.write_errno_463fdc(); // actual global, before return/outer copy
            throw SourceMusicWaveUnknown("463F7B oversized fullpath output bytes remain unqualified");
        }
        if (length == 0)
        {
            const auto error = ::GetLastError(); // source reads immediately after failure branch
            literals_.map_os_error_4699d7(error);
            throw SourceMusicWaveUnknown("463F7B failed fullpath output bytes remain unqualified");
        }
        for (std::size_t i = 0; i <= length; ++i)
            local[i] = static_cast<std::uint8_t>(native[i]);
        // Return pointer and file-part local are not subsequently consumed.
    }
};
} // namespace
SourceMusicWindowsRuntime::SourceMusicWindowsRuntime(std::shared_ptr<RaceSessionPlatform> p,
                                                     SourceApplicationLiteralRef literals)
    : owner_(p), literals_(std::move(literals)), wave_(std::make_shared<SourceMusicWaveWinmm>(p)), buffers_(p),
      timers_(p)
{
}
std::shared_ptr<SourceMusicWindowsRuntime> SourceMusicWindowsRuntime::create_runtime(
    std::shared_ptr<RaceSessionPlatform> p)
{
    if (!p)
        throw std::invalid_argument("music runtime requires platform owner");
    p->require_owner_thread();
    const auto literals = p->source_application_literals(); // same existing weak App binding
    return std::shared_ptr<SourceMusicWindowsRuntime>(new SourceMusicWindowsRuntime(std::move(p), std::move(literals)));
}
std::shared_ptr<RaceSessionPlatform> SourceMusicWindowsRuntime::owner() const
{
    auto p = owner_.lock();
    if (!p)
        throw std::logic_error("music runtime platform expired");
    p->require_owner_thread();
    return p;
}
SourceMusicInitializationResult SourceMusicWindowsRuntime::initialize(SourceMusicTimerCapsReceiving &caps,
                                                                      SourceMusicDriveTypeReceiving &drive,
                                                                      SourceMusicDiskFreeSpaceReceiving &disk)
{
    const auto self = shared_from_this(); // spans source callbacks and unwind
    (void)self;
    return source_music_initialize_45fda0(owner(), caps, drive, disk, *wave_, *this);
}
void SourceMusicWindowsRuntime::close_source()
{
    const auto p = owner();
    source_music_close_45fce0(p->state(), *this, *wave_);
}
void SourceMusicWindowsRuntime::stop_music()
{
    const auto self = shared_from_this();
    (void)self;
    const auto p = owner();
    auto &state = p->state();
    if (state.audio_stream.playback_active == 0 || state.audio_level.output_a == 0)
        return;
    (void)source_music_stop_460330(state, *this, *wave_);
    state.audio_stream.playback_active = 0; // 45F9E7 after any ordinary child return
}
SourceMusicOpenResult SourceMusicWindowsRuntime::open_source(std::span<const char> input, SourceMusicOpenArguments args,
                                                             SourceMusicTimerCapsReceiving &caps,
                                                             SourceMusicDriveTypeReceiving &drive,
                                                             SourceMusicDiskFreeSpaceReceiving &disk)
{
    const auto self = shared_from_this();
    (void)self;
    const auto p = owner();
    // NEW HOST (audio-r36): same relocation as NativeSourceBootFilePlatform of the application core.
    const NativeSourceBootFilePlatform files(p->retail_root(), p->state().boot_config);
    MusicOpenWindowsPath path(literals_, files);
    return source_music_open_45fb50(owner(), input, args, path, caps, drive, disk, *wave_, *this);
}
void SourceMusicWindowsRuntime::check_health()
{
    const auto p = owner();
    (void)p;
    timers_.check_health();
}
std::uint32_t SourceMusicWindowsRuntime::create(RaceSessionState &s, SourceMusicBufferDescription &d,
                                                AudioMusicBuffer &out, std::uint32_t outer)
{
    return buffers_.create(s, d, out, outer);
}
std::uint32_t SourceMusicWindowsRuntime::lock(AudioBufferId b, std::uint32_t o, std::uint32_t n,
                                              SourceMusicClearOutputs &out, std::uint32_t f)
{
    return buffers_.lock(b, o, n, out, f);
}
std::uint32_t SourceMusicWindowsRuntime::unlock(AudioBufferId b, std::span<std::uint8_t> p, std::uint32_t n)
{
    return buffers_.unlock(b, p, n);
}
std::uint32_t SourceMusicWindowsRuntime::release(AudioBufferId b)
{
    return buffers_.release(b);
}
std::uint32_t SourceMusicWindowsRuntime::end_period(std::uint32_t n)
{
    return end_owned_music_period(owner(), n);
}
std::uint32_t SourceMusicWindowsRuntime::exchange_flags(std::uint32_t &flags, std::uint32_t value)
{
    const auto p = owner();
    if (&flags != &p->state().audio_stream.flags)
        throw std::logic_error("music flag owner differs");
    if (reinterpret_cast<std::uintptr_t>(&flags) % std::atomic_ref<std::uint32_t>::required_alignment != 0)
        throw std::logic_error("music flags lack atomic exchange alignment");
    return std::atomic_ref<std::uint32_t>(flags).exchange(value, std::memory_order_seq_cst);
}
std::uint32_t SourceMusicWindowsRuntime::kill_timer(std::uint32_t id)
{
    return timers_.kill(id);
}
std::uint32_t SourceMusicWindowsRuntime::stop_buffer(AudioBufferId b)
{
    return buffers_.stop(b);
}
std::uint32_t SourceMusicWindowsRuntime::set_position(AudioBufferId b, std::uint32_t p)
{
    return buffers_.position(b, p);
}
std::uint32_t SourceMusicWindowsRuntime::get_status(AudioBufferId b, std::uint32_t &s)
{
    return buffers_.status(b, s);
}
std::uint16_t SourceMusicWindowsRuntime::volume_word_4897f8()
{
    const auto p = owner();
    (void)p;
    return literals_.read_u16_4602b1();
}
std::uint32_t SourceMusicWindowsRuntime::set_volume(AudioBufferId b, std::int32_t v)
{
    return buffers_.volume(b, v);
}
std::uint32_t SourceMusicWindowsRuntime::play(AudioBufferId b, std::uint32_t a, std::uint32_t c, std::uint32_t f)
{
    return buffers_.play(b, a, c, f);
}
std::uint32_t SourceMusicWindowsRuntime::read_locked(AudioFileId f, std::span<std::uint8_t> p, std::uint32_t n)
{
    return wave_->read_locked(f, p, n);
}
std::uint32_t SourceMusicWindowsRuntime::start_timer(std::uint32_t d, std::uint32_t r,
                                                     std::weak_ptr<RaceSessionPlatform> p, std::uint32_t site,
                                                     std::uint32_t flags)
{
    return timers_.start_source(d, r, p, shared_from_this(), wave_, site, flags);
}
std::uint32_t SourceMusicWindowsRuntime::cursors(AudioBufferId b, SourceMusicCursorOutputs &o)
{
    return buffers_.cursors(b, o);
}
std::uint32_t SourceMusicWindowsRuntime::milliseconds()
{
    const auto p = owner();
    (void)p;
    return timeGetTime();
}
std::uint32_t SourceMusicWindowsRuntime::lock_split(AudioBufferId b, std::uint32_t o, std::uint32_t n,
                                                    SourceMusicSplitLockOutputs &out, std::uint32_t f)
{
    return buffers_.lock_split(b, o, n, out, f);
}
std::uint32_t SourceMusicWindowsRuntime::unlock_split(AudioBufferId b, std::span<std::uint8_t> p, std::uint32_t n,
                                                      std::span<std::uint8_t> q, std::uint32_t m)
{
    return buffers_.unlock_split(b, p, n, q, m);
}
void SourceMusicWindowsRuntime::completion_thread(std::weak_ptr<RaceSessionPlatform> p, std::uint32_t callback,
                                                  std::uint32_t userdata, std::optional<std::uint32_t> &thread_id)
{
    (void)p;
    (void)callback;
    (void)userdata;
    (void)thread_id;
    // Sole observed direct45F8F0 caller supplies completion=userdata=0.
    // Nonzero target has no recovered body; do not invent a worker or success.
    throw SourceMusicWaveUnknown("4605AB/4606C2 nonzero completion function is not bound");
}
} // namespace sgn::retail
