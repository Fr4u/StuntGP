#include "sgn/platform_audio.hpp"
#include <windows.h>
#include <mmsystem.h>
#include <algorithm>
#include <exception>
#include <limits>
#include <map>
#include <system_error>

namespace sgn
{
using namespace retail;
namespace
{
std::wstring wide(std::string_view text)
{
    if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        throw FormatError("music path length");
    const auto count =
        MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!count && !text.empty())
        throw FormatError("music path is not UTF8");
    std::wstring result(static_cast<std::size_t>(count), L'\0');
    if (count && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()),
                                     result.data(), count) != count)
        throw FormatError("music path conversion failed");
    return result;
}
MMCKINFO native_chunk(const AudioWaveChunk &c)
{
    return {c.id, c.size, c.type, c.data_offset, c.flags};
}
AudioWaveChunk source_chunk(const MMCKINFO &c)
{
    return {c.ckid, c.cksize, c.fccType, c.dwDataOffset, c.dwFlags};
}
} // namespace
struct WindowsAudioStreamPlatform::Impl
{
    WindowsAudioDevice &device;
    std::filesystem::path root;
    DWORD thread{GetCurrentThreadId()};
    HWND window{};
    std::uint32_t next_callback{1};
    std::uint64_t next_file{1};
    std::map<std::uint32_t, std::function<void()>> timers, openers;
    std::map<std::uint32_t, std::uint32_t> periods;
    std::map<std::uint64_t, HMMIO> files;
    std::exception_ptr failure;
    static constexpr UINT open_message = WM_APP + 37;
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wp, LPARAM lp) noexcept
    {
        Impl *self = reinterpret_cast<Impl *>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        if (message == WM_NCCREATE)
        {
            self = static_cast<Impl *>(reinterpret_cast<CREATESTRUCTW *>(lp)->lpCreateParams);
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self && (message == WM_TIMER || message == open_message))
        {
            const auto id = static_cast<std::uint32_t>(wp);
            try
            {
                auto &callbacks = message == WM_TIMER ? self->timers : self->openers;
                const auto found = callbacks.find(id);
                if (found != callbacks.end())
                {
                    auto callback = found->second;
                    if (message == open_message)
                        callbacks.erase(found);
                    callback();
                }
            }
            catch (...)
            {
                if (!self->failure)
                    self->failure = std::current_exception();
                KillTimer(hwnd, id);
                self->timers.erase(id);
                self->openers.erase(id);
            }
            return 0;
        }
        return DefWindowProcW(hwnd, message, wp, lp);
    }
    Impl(WindowsAudioDevice &target, const std::filesystem::path &path) : device(target), root(path)
    {
        if (!root.empty())
            root = std::filesystem::weakly_canonical(std::filesystem::absolute(root));
        WNDCLASSW cls{};
        cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpfnWndProc = procedure;
        cls.lpszClassName = L"StuntGPNewMusicCallbacks";
        if (!RegisterClassW(&cls) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "register music callback window");
        window = CreateWindowExW(0, cls.lpszClassName, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, cls.hInstance, this);
        if (!window)
            throw std::system_error(static_cast<int>(GetLastError()), std::system_category(),
                                    "create music callback window");
    }
    ~Impl()
    {
        cancel();
        if (window)
            DestroyWindow(window);
        for (const auto &[id, file] : files)
        {
            (void)id;
            mmioClose(file, 0);
        }
        for (const auto &[period, count] : periods)
            for (std::uint32_t i = 0; i < count; ++i)
                timeEndPeriod(period);
    }
    void owner() const
    {
        if (GetCurrentThreadId() != thread)
            throw std::logic_error("music source state called from another thread");
    }
    void cancel() noexcept
    {
        for (const auto &[id, callback] : timers)
        {
            (void)callback;
            KillTimer(window, id);
        }
        timers.clear();
        openers.clear();
    }
    std::uint32_t callback_id()
    {
        if (!next_callback)
            throw FormatError("music callback identity exhausted");
        return next_callback++;
    }
    HMMIO file(AudioFileId id) const
    {
        const auto found = files.find(id.value);
        return found == files.end() ? nullptr : found->second;
    }
    std::filesystem::path resource(std::string_view name) const
    {
        if (root.empty() || name.empty() || name.find('\0') != std::string_view::npos)
            throw FormatError("music resource root/path missing");
        const std::filesystem::path relative(wide(name));
        if (relative.is_absolute() || relative.has_root_name())
            throw FormatError("music resource path must be relative");
        for (const auto &piece : relative)
            if (piece == L"..")
                throw FormatError("music resource leaves retail root");
        return root / relative;
    }
};
WindowsAudioStreamPlatform::WindowsAudioStreamPlatform(WindowsAudioDevice &device)
    : WindowsAudioStreamPlatform(device, {})
{
}
WindowsAudioStreamPlatform::WindowsAudioStreamPlatform(WindowsAudioDevice &device, const std::filesystem::path &root)
    : impl_(std::make_unique<Impl>(device, root))
{
}
WindowsAudioStreamPlatform::~WindowsAudioStreamPlatform() = default;
void WindowsAudioStreamPlatform::cancel_pending_callbacks() noexcept
{
    impl_->cancel();
}
void WindowsAudioStreamPlatform::check_health() const
{
    impl_->owner();
    if (impl_->failure)
        std::rethrow_exception(impl_->failure);
}
std::size_t WindowsAudioStreamPlatform::live_files() const noexcept
{
    return impl_->files.size();
}
std::size_t WindowsAudioStreamPlatform::live_timers() const noexcept
{
    return impl_->timers.size();
}
std::uint32_t WindowsAudioStreamPlatform::resource_length(std::string_view name)
{
    impl_->owner();
    std::error_code error;
    const auto size = std::filesystem::file_size(impl_->resource(name), error);
    if (error)
        return 0;
    if (size > std::numeric_limits<std::uint32_t>::max())
        throw FormatError("music resource exceeds source length word");
    return static_cast<std::uint32_t>(size);
}
bool WindowsAudioStreamPlatform::full_path(std::string_view name, std::span<std::uint8_t, 260> output)
{
    impl_->owner();
    const auto full = std::filesystem::absolute(impl_->resource(name)).lexically_normal().u8string();
    if (full.size() >= output.size())
        return false;
    std::fill(output.begin(), output.end(), std::uint8_t{});
    std::copy(full.begin(), full.end(), output.begin());
    return true;
}
std::uint32_t WindowsAudioStreamPlatform::timer_capabilities(std::uint32_t &minimum, std::uint32_t &maximum)
{
    impl_->owner();
    TIMECAPS caps{};
    const auto result = timeGetDevCaps(&caps, sizeof(caps));
    if (result == 0)
    {
        minimum = caps.wPeriodMin;
        maximum = caps.wPeriodMax;
    }
    return result;
}
std::uint32_t WindowsAudioStreamPlatform::begin_timer_period(std::uint32_t period)
{
    impl_->owner();
    const auto result = timeBeginPeriod(period);
    if (result == 0)
        ++impl_->periods[period];
    return result;
}
std::uint32_t WindowsAudioStreamPlatform::end_timer_period(std::uint32_t period)
{
    impl_->owner();
    const auto result = timeEndPeriod(period);
    if (result == 0)
    {
        const auto found = impl_->periods.find(period);
        if (found != impl_->periods.end() && --found->second == 0)
            impl_->periods.erase(found);
    }
    return result;
}
std::uint32_t WindowsAudioStreamPlatform::drive_type(std::string_view root)
{
    impl_->owner();
    return GetDriveTypeW(wide(root).c_str());
}
bool WindowsAudioStreamPlatform::disk_sector_bytes(std::string_view root, std::uint32_t &bytes)
{
    impl_->owner();
    DWORD clusters{}, sector{}, free{}, total{};
    if (!GetDiskFreeSpaceW(wide(root).c_str(), &clusters, &sector, &free, &total))
        return false;
    bytes = sector;
    return true;
}
AudioFileId WindowsAudioStreamPlatform::open_wave(std::span<const std::uint8_t, 260> filename,
                                                  std::uint32_t buffer_bytes, std::uint32_t flags)
{
    impl_->owner();
    const auto end = std::find(filename.begin(), filename.end(), std::uint8_t{});
    if (end == filename.end() || buffer_bytes > static_cast<std::uint32_t>(std::numeric_limits<LONG>::max()))
        return {};
    auto path =
        wide({reinterpret_cast<const char *>(filename.data()), static_cast<std::size_t>(end - filename.begin())});
    MMIOINFO info{};
    info.cchBuffer = static_cast<LONG>(buffer_bytes);
    const auto file = mmioOpenW(path.data(), &info, flags);
    if (!file)
        return {};
    if (!impl_->next_file)
    {
        mmioClose(file, 0);
        throw FormatError("music file identity exhausted");
    }
    const AudioFileId id{impl_->next_file++};
    try
    {
        impl_->files.emplace(id.value, file);
    }
    catch (...)
    {
        mmioClose(file, 0);
        throw;
    }
    return id;
}
std::uint32_t WindowsAudioStreamPlatform::descend_wave(AudioFileId id, AudioWaveChunk &chunk,
                                                       const AudioWaveChunk *parent, std::uint32_t flags)
{
    impl_->owner();
    const auto file = impl_->file(id);
    if (!file)
        return MMSYSERR_INVALHANDLE;
    auto current = native_chunk(chunk);
    MMCKINFO outer{};
    if (parent)
        outer = native_chunk(*parent);
    const auto result = mmioDescend(file, &current, parent ? &outer : nullptr, flags);
    chunk = source_chunk(current);
    return result;
}
std::uint32_t WindowsAudioStreamPlatform::ascend_wave(AudioFileId id, AudioWaveChunk &chunk, std::uint32_t flags)
{
    impl_->owner();
    const auto file = impl_->file(id);
    if (!file)
        return MMSYSERR_INVALHANDLE;
    auto current = native_chunk(chunk);
    const auto result = mmioAscend(file, &current, flags);
    chunk = source_chunk(current);
    return result;
}
std::int32_t WindowsAudioStreamPlatform::read_wave(AudioFileId id, std::span<std::uint8_t> output)
{
    impl_->owner();
    const auto file = impl_->file(id);
    if (!file || output.size() > static_cast<std::size_t>(std::numeric_limits<LONG>::max()))
        return -1;
    return mmioRead(file, reinterpret_cast<char *>(output.data()), static_cast<LONG>(output.size()));
}
std::int32_t WindowsAudioStreamPlatform::seek_wave(AudioFileId id, std::int32_t offset, std::int32_t origin)
{
    impl_->owner();
    const auto file = impl_->file(id);
    return file ? mmioSeek(file, offset, origin) : -1;
}
std::uint32_t WindowsAudioStreamPlatform::close_wave(AudioFileId id, std::uint32_t flags)
{
    impl_->owner();
    const auto file = impl_->file(id);
    if (!file)
        return MMSYSERR_INVALHANDLE;
    const auto result = mmioClose(file, flags);
    if (result == 0)
        impl_->files.erase(id.value);
    return result;
}
std::uint32_t WindowsAudioStreamPlatform::start_timer(std::uint32_t delay, std::uint32_t resolution,
                                                      std::function<void()> callback, std::uint32_t flags)
{
    impl_->owner();
    (void)resolution;
    if (!delay || flags != 1 || !callback)
        return 0;
    const auto id = impl_->callback_id();
    impl_->timers.emplace(id, std::move(callback));
    if (!SetTimer(impl_->window, id, delay, nullptr))
    {
        impl_->timers.erase(id);
        return 0;
    }
    return id;
}
std::uint32_t WindowsAudioStreamPlatform::kill_timer(std::uint32_t id)
{
    impl_->owner();
    if (!impl_->timers.contains(id))
        return TIMERR_NOCANDO;
    const bool killed = KillTimer(impl_->window, id) != FALSE;
    impl_->timers.erase(id);
    return killed ? 0u : TIMERR_NOCANDO;
}
AudioResult WindowsAudioStreamPlatform::set_play_cursor(AudioBufferId id, std::uint32_t offset)
{
    impl_->owner();
    return impl_->device.set_play_cursor(id, offset);
}
void WindowsAudioStreamPlatform::start_open_worker(std::function<void()> callback)
{
    impl_->owner();
    if (!callback)
        throw FormatError("empty asynchronous music opener");
    const auto id = impl_->callback_id();
    impl_->openers.emplace(id, std::move(callback));
    if (!PostMessageW(impl_->window, Impl::open_message, id, 0))
    {
        impl_->openers.erase(id);
        throw AudioError(static_cast<AudioResult>(HRESULT_FROM_WIN32(GetLastError())), "queue music opener");
    }
}
void WindowsAudioStreamPlatform::invoke_completion(std::uint32_t identity, std::uint32_t userdata)
{
    impl_->owner();
    (void)userdata;
    if (identity)
        throw FormatError("unregistered source music completion callback identity");
}
std::uint32_t WindowsAudioStreamPlatform::milliseconds()
{
    impl_->owner();
    return timeGetTime();
}
} // namespace sgn
