#include "sgn/source_native_crt.hpp"
#include "sgn/source_crt_fread.hpp"
#include "sgn/source_crt_fwrite.hpp"
#include "source_native_crt_seeds.hpp"
#include "sgn/race_session.hpp"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <array>
#include <bit>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <vector>
#include <utility>
namespace sgn::retail
{
using W = std::uint32_t;
namespace
{
W file_time_4634f7(const FILETIME &value, SourceColdTimeState &state, SourceColdTimePlatform &time)
{
    if (value.dwLowDateTime == 0 && value.dwHighDateTime == 0)
        return 0xffffffffu; // 463500..09
    FILETIME local_file;
    if (!::FileTimeToLocalFileTime(&value, &local_file))
        return 0xffffffffu; // 463510..18
    SYSTEMTIME native;
    if (!::FileTimeToSystemTime(&local_file, &native))
        return 0xffffffffu; // 463522..2A
    const SourceSystemTime local{native.wYear, native.wMonth,  native.wDayOfWeek, native.wDay,
                                 native.wHour, native.wMinute, native.wSecond,    native.wMilliseconds};
    // 46354C passes -1, using the SAME lazily initialized timezone/codepage bank.
    // The existing explicit DST/unwritten-local boundary propagates unchanged.
    return convert_source_local_time_468374(state, time, local, 0xffffffffu);
}
void find_word(std::span<std::uint8_t, 0x120> destination, std::size_t offset, W value)
{
    // Existing Windows x64 little-endian scalar transport; caller fixes every offset.
    std::memcpy(destination.data() + offset, &value, sizeof(value));
}
void find_name_468ef0(std::span<std::uint8_t, 0x120> destination, const WIN32_FIND_DATAA &native)
{
    // 468EF0 jumps to shared468F61..DF. For disjoint accessible buffers that
    // body writes exactly the source bytes through the first NUL, no suffix.
    // Native FindFirstFileA supplies a terminated cFileName[260]. Bound only
    // that API-guaranteed field; never invent truncation or a missing NUL.
    for (std::size_t i = 0; i < sizeof(native.cFileName); ++i)
    {
        const auto value = static_cast<std::uint8_t>(native.cFileName[i]);
        destination[0x18 + i] = value;
        if (value == 0)
            return;
    }
    throw std::logic_error("FindFirstFileA filename left its bounded NUL-terminated domain at468EF0");
}
} // namespace
struct NativeSourceMenuCrt::Impl
{
    struct Region
    {
        W base;
        std::vector<std::uint8_t> data, valid;
        bool allocated = false;
    };
    struct FindHandle
    {
        HANDLE value;
        explicit FindHandle(HANDLE handle) noexcept : value(handle) {}
        FindHandle(FindHandle &&other) noexcept : value(std::exchange(other.value, INVALID_HANDLE_VALUE)) {}
        FindHandle(const FindHandle &) = delete;
        FindHandle &operator=(const FindHandle &) = delete;
        // NEW HOST cleanup of abandoned find resources only. No source
        // errno/doserrno writes, source callback, or descriptor CloseHandle.
        ~FindHandle()
        {
            if (value != INVALID_HANDLE_VALUE)
                (void)::FindClose(value);
        }
    };
    SourceApplicationLiteralRef app;
    // NEW lifetime retention of the SAME world; no additional source bank.
    const std::shared_ptr<RaceSessionPlatform> platform;
    const std::thread::id thread = std::this_thread::get_id();
    // NEW HOST temporary aliases only; never store another frontend bank.
    // Present exclusively inside read/write_frontend_replay_bytes on the owner thread.
    std::span<std::uint8_t> frontend_save_scratch;
    std::span<std::uint8_t> frontend_replay_payload;
    std::vector<Region> memory;
    std::map<W, HANDLE> handles;
    // Find tokens are positive for440480 JGE, separate from high-bit
    // descriptor handles and from guest allocation addresses below70000000.
    std::map<W, FindHandle> find_handles;
    W next_find_identity = 0x70000000u;
    W next_address = 0x10000000u, next_handle = 0x80000000u;
    std::uint8_t returning = 0;
    W in_exit = 0, process_exit = 0, onexit_end = 0, onexit_begin = 0;
    SourceCrtFractionStorage fraction;
    bool entered = false, ready = false;
    explicit Impl(SourceApplicationLiteralRef a) : app(a)
    {
        seed();
    }
    Impl(SourceApplicationLiteralRef a, const std::shared_ptr<RaceSessionPlatform> &owner) : app(a), platform(owner)
    {
        if (!platform)
            throw std::logic_error("CRT codepage borrow requires an existing platform");
        seed();
    }
    void seed()
    {
        check();
        add(0x472000, crt_seed::pe_472000);
        add(0x495944, crt_seed::pe_495944);
        add(0x4959dc, crt_seed::pe_4959dc); // actual mutable process-type DWORD
        add(0x495da8, crt_seed::pe_495da8);
        zero(0x6365e0, 24); // single atof pointer/result owner, seeded once
        add(0x495fe0, crt_seed::pe_495fe0);
        zero(0x636818, 4); // actual sentinel and PE-zero allocation counter
        zero(0x63681c, 4); // source commode/default FILEflags, PE virtual zero at466678
        zero(0x636538, 4); // actual mutable umask: PE virtual zero, seeded once; wb reads46BAF9
        add(0x4962e0, crt_seed::pe_4962e0);
        add(0x4965d0, crt_seed::pe_4965d0);
        zero(0x4959e0, 24);
        zero(0x636820, 8);
        zero(0x636848, 4);
        zero(0x636860, 0x22c); // MB codepage/case/type and FILE-table pointer.
        if (!platform)
            zero(0x636840, 4); // Standalone PE zero only; never reset the borrowed slot.
        zero(0x637aa0, 4);
        zero(0x637ac0, 0x104);
        zero(0x637be8, 4);
    }
    void check()
    {
        if (std::this_thread::get_id() != thread)
            throw std::logic_error("CRT owner used from another thread");
        (void)app.crt_error_storage();
        if (platform)
        {
            platform->require_owner_thread();
            if (!(platform->source_application_literals() == app))
                throw std::logic_error("CRT codepage and literals belong to different App owners");
        }
    }
    void add(W a, std::span<const std::uint8_t> bytes)
    {
        memory.push_back({a, {bytes.begin(), bytes.end()}, std::vector<std::uint8_t>(bytes.size(), 1)});
    }
    void zero(W a, std::size_t n)
    {
        memory.push_back({a, std::vector<std::uint8_t>(n), std::vector<std::uint8_t>(n, 1)});
    }
    std::span<std::uint8_t> scalar(W a)
    {
        auto span = [](auto &v) { return std::span<std::uint8_t>(reinterpret_cast<std::uint8_t *>(&v), sizeof(v)); };
        if (a >= 0x627a54 && a < 0x627c54)
        {
            if (frontend_save_scratch.empty())
                throw std::logic_error("frontend save scratch accessed outside its scoped CRT borrow");
            return frontend_save_scratch.subspan(a - 0x627a54);
        }
        if (a >= 0x5e3c48 && a < 0x5f26a8)
        {
            if (frontend_replay_payload.empty())
                throw std::logic_error("frontend replay payload accessed outside its scoped CRT borrow");
            return frontend_replay_payload.subspan(a - 0x5e3c48);
        }
        if (a >= 0x496560 && a < 0x496564)
            return span(fraction.literal_496560).subspan(a - 0x496560);
        if (a >= 0x636528 && a < 0x63652c)
            return span(fraction.bypass_636528).subspan(a - 0x636528);
        if (a >= 0x63652c && a < 0x636530)
            return span(fraction.workaround_63652c).subspan(a - 0x63652c);
        if (a >= 0x6365c0 && a < 0x6365c8)
            return span(fraction.result_6365c0).subspan(a - 0x6365c0);
        if (a == 0x6365c8)
            return span(fraction.register_arguments_6365c8);
        if (a >= 0x636530 && a < 0x636534)
            return span(app.crt_error_storage().errno_636530).subspan(a - 0x636530);
        if (a >= 0x636534 && a < 0x636538)
            return span(app.crt_error_storage().doserrno_636534).subspan(a - 0x636534);
        if (platform && a >= 0x636840 && a < 0x636844)
            return span(platform->state().source_cold_time.code_page).subspan(a - 0x636840);
        if (a == 0x636570)
            return span(returning);
        if (a >= 0x636574 && a < 0x636578)
            return span(in_exit).subspan(a - 0x636574);
        if (a >= 0x636578 && a < 0x63657c)
            return span(process_exit).subspan(a - 0x636578);
        if (a >= 0x637bec && a < 0x637bf0)
            return span(onexit_end).subspan(a - 0x637bec);
        if (a >= 0x637bf0 && a < 0x637bf4)
            return span(onexit_begin).subspan(a - 0x637bf0);
        return {};
    }
    Region &region(W a, std::size_t n)
    {
        for (auto &x : memory)
            if (a >= x.base && std::uint64_t(a - x.base) + n <= x.data.size())
                return x;
        throw std::out_of_range("CRT access outside retained source memory");
    }
    std::span<std::uint8_t> bytes(W a, std::size_t n, bool reading)
    {
        auto v = scalar(a);
        if (!v.empty())
        {
            if (n > v.size())
                throw std::out_of_range("CRT scalar access crosses owner");
            return v.first(n);
        }
        auto &x = region(a, n);
        const auto off = a - x.base;
        if (reading && std::find(x.valid.begin() + off, x.valid.begin() + off + n, 0) != x.valid.begin() + off + n)
            throw std::logic_error("CRT attempted an unwritten malloc-byte read");
        if (!reading)
            std::fill(x.valid.begin() + off, x.valid.begin() + off + n, std::uint8_t{1});
        return std::span{x.data}.subspan(off, n);
    }
    // NEW native-output borrow. Resolving the destination must not mark its
    // unwritten suffix initialized before a partial/failed ReadFile has occurred.
    std::span<std::uint8_t> native_read_destination(W a, std::size_t n)
    {
        // 4690B9 may consume its final requested byte as lookahead, then call
        // ReadFile with count0 at the actual one-past address. Admit that empty
        // view only while its owner is borrowed; a nonzero write still fails.
        if (n == 0)
        {
            if (a == 0x627c54 && !frontend_save_scratch.empty())
                return frontend_save_scratch.last(0);
            if (a == 0x5f26a8 && !frontend_replay_payload.empty())
                return frontend_replay_payload.last(0);
        }
        auto v = scalar(a);
        if (!v.empty())
        {
            if (n > v.size())
                throw std::out_of_range("native read crosses CRT scalar owner");
            return v.first(n);
        }
        auto &x = region(a, n);
        return std::span{x.data}.subspan(a - x.base, n);
    }
    void mark_native_read_prefix(W a, std::size_t n)
    {
        if (n == 0)
            return;
        auto v = scalar(a);
        if (!v.empty())
        {
            if (n > v.size())
                throw std::out_of_range("native read prefix crosses CRT scalar owner");
            return;
        }
        auto &x = region(a, n);
        const auto off = a - x.base;
        std::fill(x.valid.begin() + off, x.valid.begin() + off + n, std::uint8_t{1});
    }
    W allocate(W size, bool clear)
    {
        if (size == 0)
            size = 1;
        if (std::uint64_t(next_address) + size + 16u >= 0x70000000u)
            return 0;
        const auto base = next_address;
        try
        {
            memory.push_back(
                {base, std::vector<std::uint8_t>(size), std::vector<std::uint8_t>(size, clear ? 1 : 0), true});
        }
        catch (const std::bad_alloc &)
        {
            return 0;
        }
        next_address = base + size + 16u;
        return base;
    }
    void release(W address)
    {
        if (address == 0)
            return; // 462FA3..A5 null free has no allocation effect.
        const auto it = std::find_if(memory.begin(), memory.end(),
                                     [address](const Region &r) { return r.base == address && r.allocated; });
        if (it == memory.end())
            throw std::logic_error("CRT free requires an exact live allocation base from this owner");
        // NEW paired host transport for allocate/calloc: original small-block
        // heap467516/467541 and HeapFree637BDC are not emulated. Erasure frees
        // bytes and validity together; monotonic guest addresses are not reused.
        const auto error = ::GetLastError();
        memory.erase(it);
        ::SetLastError(error);
    }
    W token(HANDLE h)
    {
        if (h == INVALID_HANDLE_VALUE)
            return 0xffffffffu;
        if (h == nullptr)
            return 0;
        for (auto [token, value] : handles)
            if (value == h)
                return token;
        if (next_handle == 0xffffffffu)
            throw std::overflow_error("CRT handle identities exhausted");
        const auto token = next_handle++;
        handles.emplace(token, h);
        return token;
    }
    HANDLE native(W token)
    {
        if (token == 0xffffffffu)
            return INVALID_HANDLE_VALUE;
        if (token == 0)
            return nullptr;
        const auto it = handles.find(token);
        if (it == handles.end())
            throw std::logic_error("foreign CRT handle token");
        return it->second;
    }
};
NativeSourceMenuCrt::NativeSourceMenuCrt(SourceApplicationLiteralRef app) : p_(std::make_unique<Impl>(app)) {}
NativeSourceMenuCrt::NativeSourceMenuCrt(SourceApplicationLiteralRef app,
                                         const std::shared_ptr<RaceSessionPlatform> &platform)
    : p_(std::make_unique<Impl>(app, platform))
{
}
NativeSourceMenuCrt::~NativeSourceMenuCrt() =
    default; // No CloseHandle of inherited/std handles; owned abandoned finds use FindClose.
void NativeSourceMenuCrt::require_platform(const RaceSessionPlatform &platform) const
{
    p_->check();
    if (p_->platform.get() != &platform)
        throw std::logic_error("CRT operation requires its retained Core platform");
}
W NativeSourceMenuCrt::fopen_binary_read_462ac3(std::span<const std::uint8_t> path, std::span<const std::uint8_t> mode)
{
    p_->check();
    return source_fopen_binary_read_462ac3(*this, *this, *this, path, mode);
}
W NativeSourceMenuCrt::fopen_binary_write_462ac3(std::span<const std::uint8_t> path, std::span<const std::uint8_t> mode)
{
    p_->check();
    return source_fopen_binary_write_462ac3(*this, *this, *this, path, mode);
}
W NativeSourceMenuCrt::create_file_a(std::span<const std::uint8_t> path, W access, W share, bool inherit, W disposition,
                                     W attributes)
{
    p_->check();
    if (std::find(path.begin(), path.end(), std::uint8_t{0}) == path.end())
        throw std::logic_error("CreateFileA import requires a bounded source NUL pathname");
    // NEW x64 ABI adaptation of source SECURITY_ATTRIBUTES12. The original
    // descriptorNULL/inheritance value and all CreateFileA parameters survive.
    SECURITY_ATTRIBUTES security{static_cast<DWORD>(sizeof(SECURITY_ATTRIBUTES)), nullptr, inherit ? TRUE : FALSE};
    const auto handle = ::CreateFileA(reinterpret_cast<const char *>(path.data()), access, share, &security,
                                      disposition, attributes, nullptr);
    const auto error = ::GetLastError(); // Preserve the import through token bookkeeping.
    if (handle == INVALID_HANDLE_VALUE)
    {
        ::SetLastError(error);
        return 0xffffffffu;
    }
    W identity;
    try
    {
        identity = p_->token(handle);
    } // same map used by descriptor reads and closes
    catch (...)
    {
        // NEW HOST exception cleanup only. No fabricated source failure or errno.
        (void)::CloseHandle(handle);
        throw;
    }
    ::SetLastError(error);
    return identity;
}
W NativeSourceMenuCrt::find_first_46355b(std::span<const std::uint8_t> pattern,
                                         std::span<std::uint8_t, 0x120> destination, SourceColdTimePlatform &time)
{
    p_->check();
    if (!p_->platform)
        throw std::logic_error("source find timestamps require the existing Core platform CRT borrow");
    if (std::find(pattern.begin(), pattern.end(), std::uint8_t{0}) == pattern.end())
        throw std::logic_error("source find pattern lacks its bounded NUL terminator");
    if (p_->next_find_identity >= 0x80000000u)
        throw std::overflow_error("positive source find identities exhausted");
    WIN32_FIND_DATAA native; // Source local is not pre-cleared; read only successful API output.
    const auto handle = ::FindFirstFileA(reinterpret_cast<const char *>(pattern.data()), &native); // 46356F
    if (handle == INVALID_HANDLE_VALUE)
    {
        const auto error = ::GetLastError(); // 46357C, before any other native operation
        const W value = error == 2u || error == 3u || error == 18u ? 2u : error == 8u ? 12u : 22u;
        p_->app.crt_error_storage().errno_636530 = value; // 463598/AA/B6; doserrno untouched
        return 0xffffffffu;
    }
    // NEW HANDLE-width translation. Register the actual find owner before time
    // children; a host boundary keeps the reached data prefix and this resource.
    Impl::FindHandle owned(handle);
    const W identity = p_->next_find_identity;
    p_->find_handles.emplace(identity, std::move(owned));
    ++p_->next_find_identity; // Never recycle a consumed token into another find.
    find_word(destination, 0, native.dwFileAttributes == 0x80u ? 0u : native.dwFileAttributes); // 4635D8
    auto &state = p_->platform->state().source_cold_time;
    find_word(destination, 4, file_time_4634f7(native.ftCreationTime, state, time));   // 4635E1/E6
    find_word(destination, 8, file_time_4634f7(native.ftLastAccessTime, state, time)); // 4635F0/F5
    find_word(destination, 12, file_time_4634f7(native.ftLastWriteTime, state, time)); // 4635FF/63607
    // 463616 multiplies (0:high) by (1:0); ADD/ADC low then writes both DWORDs.
    find_word(destination, 0x10, native.nFileSizeLow);
    find_word(destination, 0x14, native.nFileSizeHigh);
    find_name_468ef0(destination, native); // 463638, including shared copy tail
    return identity;                       // NEW positive token for the actual successful native handle
}
W NativeSourceMenuCrt::find_close_4634d8(W identity)
{
    p_->check();
    const auto found = p_->find_handles.find(identity);
    if (found == p_->find_handles.end())
        throw std::logic_error("source FindClose received a foreign or consumed find-domain token");
    if (!::FindClose(found->second.value))
    {                                                   // 4634DC, not descriptor CloseHandle
        p_->app.crt_error_storage().errno_636530 = 22u; // 4634E6; doserrno untouched
        return 0xffffffffu; // Failed close retains the native owner for an actual later close/cleanup.
    }
    found->second.value = INVALID_HANDLE_VALUE; // Native close succeeded; destructor must not close twice.
    p_->find_handles.erase(found);
    return 0;
}
W NativeSourceMenuCrt::file_size_440480(std::span<const std::uint8_t> pattern, SourceColdTimePlatform &time)
{
    std::array<std::uint8_t, 0x120> data; // Actual source local extent; no clear or copied persistent bank.
    const W identity = find_first_46355b(pattern, data, time); // 440491
    if (std::bit_cast<std::int32_t>(identity) < 0)
        return 0xffffffffu;            // 44049B signed JGE
    (void)find_close_4634d8(identity); // 4404A5; close result ignored, errno retained
    if ((data[0] & 0x10u) != 0)
        return 0xffffffffu; // 4404AA/B3, after the close
    W low;
    std::memcpy(&low, data.data() + 0x10, sizeof(low));
    return low; // 4404BE, no signed-size rejection
}
W NativeSourceMenuCrt::read_frontend_replay_header(FrontendState &frontend, W file)
{
    return read_frontend_replay_bytes(frontend, 0x627b54, 128, file);
}
W NativeSourceMenuCrt::read_frontend_replay_bytes(FrontendState &frontend, W destination, W count, W file)
{
    p_->check();
    if (!p_->platform || frontend.shared_platform().get() != p_->platform.get() ||
        &frontend.world() != &p_->platform->state())
        throw std::logic_error("replay storage and CRT must use the same actual frontend world");
    if (!p_->frontend_save_scratch.empty() || !p_->frontend_replay_payload.empty())
        throw std::logic_error("reentrant frontend replay borrow");
    // Resolve both existing owners before publishing either temporary view.
    // 5E3C48 and5EB178 are adjacent0x7530-byte regions of the same cold_suffix;
    // the source selection copy45CA93 also uses these very bytes. No preclear.
    const auto scratch = frontend.source.source_bytes(0x627a54, 0x200);
    const auto payload = frontend.source.source_bytes(0x5e3c48, 0xea60);
    p_->frontend_save_scratch = scratch;
    p_->frontend_replay_payload = payload;
    struct BorrowEnd
    {
        std::span<std::uint8_t> &scratch;
        std::span<std::uint8_t> &payload;
        ~BorrowEnd()
        {
            scratch = {};
            payload = {};
        }
    } end{p_->frontend_save_scratch, p_->frontend_replay_payload};
    // 46373C handles count0 before any FILE/destination access. Any later
    // owner boundary failure retains earlier source writes and read prefixes.
    return read_source_crt_elements_binary_463729(*this, destination, 1, count, file);
}
W NativeSourceMenuCrt::write_frontend_replay_bytes(FrontendState &frontend, W source, W count, W file)
{
    p_->check();
    if (!p_->platform || frontend.shared_platform().get() != p_->platform.get() ||
        &frontend.world() != &p_->platform->state())
        throw std::logic_error("replay storage and CRT must use the same actual frontend world");
    if (!p_->frontend_save_scratch.empty() || !p_->frontend_replay_payload.empty())
        throw std::logic_error("reentrant frontend replay borrow");
    // Resolve both existing owners before publishing either temporary view.
    // 5E3C48 and5EB178 are adjacent0x7530-byte regions of the same cold_suffix;
    // the source selection copy45CA93 also uses these very bytes. No preclear.
    const auto scratch = frontend.source.source_bytes(0x627a54, 0x200);
    const auto payload = frontend.source.source_bytes(0x5e3c48, 0xea60);
    p_->frontend_save_scratch = scratch;
    p_->frontend_replay_payload = payload;
    struct BorrowEnd
    {
        std::span<std::uint8_t> &scratch;
        std::span<std::uint8_t> &payload;
        ~BorrowEnd()
        {
            scratch = {};
            payload = {};
        }
    } end{p_->frontend_save_scratch, p_->frontend_replay_payload};
    // 463825 handles count0 before any FILE/input access. Reached writes,
    // FILE changes and native partial effects survive later owner failures.
    return write_source_crt_elements_463811(*this, *this, *this, source, 1, count, file);
}
W NativeSourceMenuCrt::word(W a)
{
    p_->check();
    W v;
    auto b = p_->bytes(a, 4, true);
    std::memcpy(&v, b.data(), 4);
    return v;
}
std::uint8_t NativeSourceMenuCrt::byte(W a)
{
    p_->check();
    return p_->bytes(a, 1, true)[0];
}
void NativeSourceMenuCrt::word(W a, W v)
{
    p_->check();
    auto b = p_->bytes(a, 4, false);
    std::memcpy(b.data(), &v, 4);
}
void NativeSourceMenuCrt::byte(W a, std::uint8_t v)
{
    p_->check();
    p_->bytes(a, 1, false)[0] = v;
}
W NativeSourceMenuCrt::read_word(W a, W)
{
    return word(a);
}
void NativeSourceMenuCrt::write_word(W a, W v, W)
{
    word(a, v);
}
W NativeSourceMenuCrt::flush_file_466564(W f)
{
    return flush_source_crt_file_466564(*this, f);
}
W NativeSourceMenuCrt::close_file_462a4d(W f)
{
    return close_source_crt_file_462a4d(*this, *this, *this, f);
}
bool NativeSourceMenuCrt::close_native_handle(W h)
{
    p_->check();
    const auto handle = p_->native(h);
    const auto found = p_->handles.find(h);
    const BOOL ok = ::CloseHandle(handle);
    const auto error = ::GetLastError();
    // NEW x64 identity policy: keep a consumed-token tombstone so an alias
    // cannot address a later unrelated HANDLE recycled by Windows. Failure
    // retains the actual handle; source descriptor invalidation is separate.
    if (ok != FALSE && found != p_->handles.end())
        found->second = INVALID_HANDLE_VALUE;
    ::SetLastError(error);
    return ok != FALSE;
}
bool NativeSourceMenuCrt::set_std_handle(W code, W h)
{
    p_->check();
    const auto handle = p_->native(h);
    return ::SetStdHandle(code, handle) != FALSE;
}
void NativeSourceMenuCrt::free_462f9e(W address)
{
    p_->check();
    p_->release(address);
}
W NativeSourceMenuCrt::write_descriptor_4695e5(W f, W b, W n)
{
    return source_descriptor_write_4695e5(*this, f, b, n);
}
void NativeSourceMenuCrt::allocate_buffer_46b45e(W f)
{
    allocate_source_crt_buffer_46b45e(*this, f);
}
W NativeSourceMenuCrt::fill_buffer_binary_468fe0(W f)
{
    return fill_source_crt_buffer_binary_468fe0(*this, f);
}
W NativeSourceMenuCrt::read_descriptor_binary_4690b9(W f, W b, W n)
{
    return source_descriptor_read_binary_4690b9(*this, *this, f, b, n);
}
bool NativeSourceMenuCrt::read_file(W h, W b, W n, W &transferred)
{
    p_->check();
    const auto handle = p_->native(h);
    auto destination = p_->native_read_destination(b, n);
    DWORD count = 0;
    const BOOL ok = ReadFile(handle, destination.data(), n, &count, nullptr);
    const auto error = GetLastError(); // Retain the import's error across host validity bookkeeping.
    if (count > n)
        throw std::logic_error("ReadFile returned a prefix larger than the retained destination");
    p_->mark_native_read_prefix(b, count);
    transferred = count;
    SetLastError(error);
    return ok != FALSE;
}
W NativeSourceMenuCrt::commit_descriptor_46b968(W f)
{
    return source_descriptor_commit_46b968(*this, f);
}
bool NativeSourceMenuCrt::write_file(W h, W b, W n, W &written)
{
    p_->check();
    auto data = p_->bytes(b, n, true);
    DWORD count;
    const BOOL ok = WriteFile(p_->native(h), data.data(), n, &count, nullptr);
    if (ok)
        written = count;
    return ok != FALSE;
}
bool NativeSourceMenuCrt::write_file_local(W h, std::span<const std::uint8_t> b, W &written)
{
    p_->check();
    DWORD count;
    const BOOL ok = WriteFile(p_->native(h), b.data(), static_cast<DWORD>(b.size()), &count, nullptr);
    if (ok)
        written = count;
    return ok != FALSE;
}
bool NativeSourceMenuCrt::flush_file_buffers(W h)
{
    p_->check();
    return FlushFileBuffers(p_->native(h)) != FALSE;
}
W NativeSourceMenuCrt::set_file_pointer(W h, W offset, W method)
{
    p_->check();
    return SetFilePointer(p_->native(h), std::bit_cast<LONG>(offset), nullptr, method);
}
W NativeSourceMenuCrt::last_error()
{
    return GetLastError();
}
void NativeSourceMenuCrt::map_os_error_4699d7(W e)
{
    p_->check();
    p_->app.map_os_error_4699d7(e);
}
W NativeSourceMenuCrt::allocate_462fcd(W n)
{
    p_->check();
    return p_->allocate(n, false);
}
W NativeSourceMenuCrt::calloc_46d0c0(W n, W width)
{
    p_->check();
    const auto size = std::uint64_t(n) * width;
    if (size > 0xffffffffu)
        return 0;
    return p_->allocate(static_cast<W>(size), true);
}
void NativeSourceMenuCrt::fatal_46412d(W code)
{
    throw std::runtime_error("CRT startup allocation failure at46412D, code=" + std::to_string(code));
}
SourceCrtStartupInfo NativeSourceMenuCrt::get_startup_info_a()
{
    p_->check();
    STARTUPINFOA info{};
    info.cb = sizeof(info);
    GetStartupInfoA(&info);
    if (info.cbReserved2 == 0 || info.lpReserved2 == nullptr)
        return {info.cbReserved2, 0};
    // NEW x64 ABI transport: inherited HANDLEs have pointer width, source records have DWORD tokens.
    if (info.cbReserved2 < 4)
        throw std::runtime_error("truncated inherited CRT header");
    W count;
    std::memcpy(&count, info.lpReserved2, 4);
    const auto required = std::uint64_t{4} + count + std::uint64_t(count) * sizeof(std::intptr_t);
    if (required > info.cbReserved2)
        throw std::runtime_error("truncated native inherited CRT handle block");
    const auto base = allocate_462fcd(4u + count * 5u);
    if (base == 0)
        throw std::bad_alloc();
    word(base, count);
    for (W i = 0; i < count; ++i)
    {
        byte(base + 4u + i, info.lpReserved2[4u + i]);
        std::intptr_t value;
        std::memcpy(&value, info.lpReserved2 + 4u + count + std::size_t(i) * sizeof(value), sizeof(value));
        word(base + 4u + count + i * 4u, p_->token(reinterpret_cast<HANDLE>(value)));
    }
    return {info.cbReserved2, base};
}
W NativeSourceMenuCrt::get_std_handle(W code)
{
    p_->check();
    return p_->token(GetStdHandle(code));
}
W NativeSourceMenuCrt::get_file_type(W h)
{
    p_->check();
    return GetFileType(p_->native(h));
}
void NativeSourceMenuCrt::set_handle_count(W n)
{
    p_->check();
    (void)SetHandleCount(n);
}
SourceCrtFractionStorage &NativeSourceMenuCrt::fraction_storage_46291a()
{
    p_->check();
    if (!p_->ready)
        throw std::logic_error("HUD CRT fraction before completed source startup");
    return p_->fraction;
}
void NativeSourceMenuCrt::initialize()
{
    p_->check();
    if (p_->entered)
        throw std::logic_error("CRT startup already entered; failed startup is not reset");
    p_->entered = true;
    initialize_source_crt_descriptors_469792(*this);
    initialize_source_crt_callbacks_462dcd(*this);
    p_->ready = true;
}
SourceCrtExitWords NativeSourceMenuCrt::exit_words()
{
    p_->check();
    return {p_->returning, p_->in_exit, p_->process_exit, p_->onexit_end, p_->onexit_begin};
}
void NativeSourceMenuCrt::exit_menu(FrontendState &f, SourceApplicationShutdownServices &shutdown, W status)
{
    p_->check();
    if (!p_->ready)
        throw std::logic_error("menu CRT exit before completed startup");
    // Both operations resolve the same weak App error owner; reject a foreign App.
    if (p_->platform && f.shared_platform().get() != p_->platform.get())
        throw std::logic_error("menu and CRT codepage belong to different platform worlds");
    const auto other = f.shared_platform()->source_application_literals();
    if (&other.crt_error_storage() != &p_->app.crt_error_storage())
        throw std::logic_error("menu and CRT belong to different App owners");
    SourceHostUnwindBoundary host;
    execute_source_menu_exit_462dfa(f, exit_words(), *this, host, shutdown, status);
}
extern "C" W sgn_crt_probe_464188();
extern "C" void sgn_crt_precision_464176();
void NativeSourceMenuCrt::invoke_startup_callback(W a)
{
    if (a != 0x4627bc)
        throw std::logic_error("unreconstructed mutated CRT startup callback " + std::to_string(a));
    word(0x4959e4, 0x464249);
    word(0x4959e0, 0x4645b4);
    word(0x4959e8, 0x4642af);
    word(0x4959ec, 0x4641ef);
    word(0x4959f0, 0x464297);
    word(0x4959f4, 0x4645b4);
    const auto module = GetModuleHandleA("KERNEL32");
    const auto symbol = module ? GetProcAddress(module, "IsProcessorFeaturePresent") : nullptr;
    // Exact optional import name/value is pinned in the receipt. See fallback source probe.
    W result;
    if (symbol)
    {
        using Probe = BOOL(WINAPI *)(DWORD);
        Probe probe;
        static_assert(sizeof(probe) == sizeof(symbol));
        std::memcpy(&probe, &symbol, sizeof(probe));
        result = static_cast<W>(probe(0));
    }
    else
        result = sgn_crt_probe_464188();
    word(0x63652c, result);
    sgn_crt_precision_464176();
}
void NativeSourceMenuCrt::invoke_registered_callback(W a)
{
    throw std::logic_error("unreconstructed dynamically registered CRT callback " + std::to_string(a));
}
void NativeSourceMenuCrt::close_streams_46d13d()
{
    throw std::logic_error("returning CRT cleanup is outside the native nonreturning menu exit domain");
}
} // namespace sgn::retail

namespace sgn::retail
{
namespace
{
W cp_locale(W cp)
{
    switch (cp)
    {
    case 932:
        return 0x411;
    case 936:
        return 0x804;
    case 949:
        return 0x412;
    case 950:
        return 0x404;
    default:
        return 0;
    }
}
// NEW HOST compatibility for the source default-ANSI request only. The
// retained byte classification/case banks cannot represent UTF-8. Select the
// ANSI page declared by the actual system locale, never a fixed Western page
// or the user's formatting/UI locale. This does not change the process ACP,
// the borrowed C-locale word636840, filesystem encoding, or source table code.
W source_default_ansi_codepage()
{
    const auto active = GetACP();
    if (active != CP_UTF8)
        return active;
    std::array<wchar_t, LOCALE_NAME_MAX_LENGTH> locale{};
    if (GetSystemDefaultLocaleName(locale.data(), static_cast<int>(locale.size())) == 0)
    {
        const auto error = GetLastError();
        // R25: clang-format off keeps this message one string literal, as in the base (no literal split).
        // clang-format off
        throw std::runtime_error(
            "NativeCRT host compatibility: ACP65001 requires the system locale; GetSystemDefaultLocaleName failed, Win32=" +
            std::to_string(error));
        // clang-format on
    }
    DWORD legacy{};
    constexpr int number_chars = static_cast<int>(sizeof(legacy) / sizeof(wchar_t));
    if (GetLocaleInfoEx(locale.data(), LOCALE_IDEFAULTANSICODEPAGE | LOCALE_RETURN_NUMBER,
                        reinterpret_cast<wchar_t *>(&legacy), number_chars) != number_chars)
    {
        const auto error = GetLastError();
        // R25: clang-format off keeps this message one string literal, as in the base (no literal split).
        // clang-format off
        throw std::runtime_error(
            "NativeCRT host compatibility: ACP65001 requires the system locale ANSI page; GetLocaleInfoEx failed, Win32=" +
            std::to_string(error));
        // clang-format on
    }
    // A zero result denotes a Unicode-only locale, not permission to choose
    // 1252. Reject pseudo/Unicode pages instead of recursing through CP_ACP.
    if (legacy <= CP_THREAD_ACP || legacy == CP_SYMBOL || legacy == 1200u || legacy == 1201u || legacy == CP_UTF7 ||
        legacy == CP_UTF8)
        throw std::runtime_error(
            "NativeCRT host compatibility: system locale has no usable legacy ANSI page, codepage=" +
            std::to_string(legacy));
    CPINFO info{};
    if (GetCPInfo(legacy, &info) == 0)
    {
        const auto error = GetLastError();
        throw std::runtime_error("NativeCRT host compatibility: system locale ANSI page is unavailable, codepage=" +
                                 std::to_string(legacy) + ", Win32=" + std::to_string(error));
    }
    // Capacity admission only; MaxCharSize does not itself identify an ANSI
    // encoding. That identity came from LOCALE_IDEFAULTANSICODEPAGE above.
    if (info.MaxCharSize == 0 || info.MaxCharSize > 2)
        throw std::runtime_error(
            "NativeCRT host compatibility: system locale ANSI page exceeds the retained byte-table domain, codepage=" +
            std::to_string(legacy) + ", MaxCharSize=" + std::to_string(info.MaxCharSize));
    return legacy;
}
void clear_mb(NativeSourceMenuCrt &m)
{
    for (W i = 0; i < 257; ++i)
        m.byte(0x636980 + i, 0);
    m.word(0x636860, 0);
    m.word(0x63687c, 0);
    m.word(0x636a84, 0);
    for (W a = 0x636870; a < 0x63687c; a += 4)
        m.word(a, 0);
}
void update_case(NativeSourceMenuCrt &m)
{
    const auto cp = m.word(0x636860);
    CPINFO info{};
    if (GetCPInfo(cp, &info) != 1)
    {
        for (W i = 0; i < 256; ++i)
        {
            if (i >= 65 && i <= 90)
            {
                m.byte(0x636981 + i, static_cast<std::uint8_t>(m.byte(0x636981 + i) | 0x10));
                m.byte(0x636880 + i, static_cast<std::uint8_t>(i + 32));
            }
            else if (i >= 97 && i <= 122)
            {
                m.byte(0x636981 + i, static_cast<std::uint8_t>(m.byte(0x636981 + i) | 0x20));
                m.byte(0x636880 + i, static_cast<std::uint8_t>(i - 32));
            }
            else
                m.byte(0x636880 + i, 0);
        }
        return;
    }
    std::array<char, 256> input;
    for (W i = 0; i < 256; ++i)
        input[i] = static_cast<char>(i);
    input[0] = 32;
    for (unsigned i = 0; i + 1 < MAX_LEADBYTES && info.LeadByte[i] != 0; i += 2)
        for (unsigned c = info.LeadByte[i]; c <= info.LeadByte[i + 1]; ++c)
            input[c] = 32;
    const auto actual_cp = cp != 0 ? cp : m.word(0x636840);
    // 46C1DE/46C4AD Unicode branches on modern Windows, same MB_PRECOMPOSED and source W2MB flags.
    if (m.word(0x636820) == 0)
    {
        WORD probe;
        if (GetStringTypeW(1, L"", 1, &probe) == 0)
            throw std::runtime_error("Windows Unicode classification unavailable");
        m.word(0x636820, 1);
    }
    if (m.word(0x636820) != 1)
        throw std::runtime_error("legacy ANSI locale dispatch is outside modern Unicode host");
    const int needed = MultiByteToWideChar(actual_cp, 1, input.data(), 256, nullptr, 0);
    if (needed <= 0 || needed > 256)
        throw std::runtime_error("source locale conversion leaves uninitialized case locals on this codepage");
    std::vector<wchar_t> wide(static_cast<std::size_t>(needed));
    if (MultiByteToWideChar(actual_cp, 1, input.data(), 256, wide.data(), needed) == 0)
        throw std::runtime_error("source locale input conversion failed");
    std::array<WORD, 256> types; // Source local initialized only by actual API.
    if (GetStringTypeW(1, wide.data(), needed, types.data()) == 0 || needed != 256)
        throw std::runtime_error("source locale classification has an unwritten256-entry tail");
    const auto map_case = [&](DWORD flags) {
        if (m.word(0x636824) == 0)
        {
            if (LCMapStringW(0, 0x100, L"", 1, nullptr, 0) == 0)
                throw std::runtime_error("Windows Unicode mapping unavailable");
            m.word(0x636824, 1);
        }
        if (m.word(0x636824) != 1)
            throw std::runtime_error("legacy ANSI map dispatch is outside modern Unicode host");
        const auto locale = m.word(0x636a84);
        const int count = LCMapStringW(locale, flags, wide.data(), needed, nullptr, 0);
        if (count <= 0)
            throw std::runtime_error("source locale case sizing failed");
        std::vector<wchar_t> mapped(static_cast<std::size_t>(count));
        if (LCMapStringW(locale, flags, wide.data(), needed, mapped.data(), count) == 0)
            throw std::runtime_error("source locale case conversion failed");
        std::array<char, 256> result;
        if (WideCharToMultiByte(actual_cp, 0x220, mapped.data(), count, result.data(), 256, nullptr, nullptr) != 256)
            throw std::runtime_error("source locale case output has an unwritten256-byte tail");
        return result;
    };
    const auto lower = map_case(0x100);
    const auto upper = map_case(0x200);
    for (W i = 0; i < 256; ++i)
    {
        if ((types[i] & 1) != 0)
        {
            m.byte(0x636981 + i, static_cast<std::uint8_t>(m.byte(0x636981 + i) | 0x10));
            m.byte(0x636880 + i, static_cast<std::uint8_t>(lower[i]));
        }
        else if ((types[i] & 2) != 0)
        {
            m.byte(0x636981 + i, static_cast<std::uint8_t>(m.byte(0x636981 + i) | 0x20));
            m.byte(0x636880 + i, static_cast<std::uint8_t>(upper[i]));
        }
        else
            m.byte(0x636880 + i, 0);
    }
}
} // namespace
void NativeSourceMenuCrt::set_multibyte_codepage_46c823(W requested)
{
    p_->check();
    word(0x636848, 0);
    W cp = requested;
    if (requested == 0xfffffffeu)
    {
        word(0x636848, 1);
        cp = GetOEMCP();
    }
    else if (requested == 0xfffffffdu)
    {
        word(0x636848, 1);
        cp = source_default_ansi_codepage();
    }
    else if (requested == 0xfffffffcu)
    {
        cp = word(0x636840);
        word(0x636848, 1);
    }
    if (cp == word(0x636860))
        return;
    if (cp == 0)
    {
        clear_mb(*this);
        update_case(*this);
        return;
    }
    W found = 0xffffffffu;
    for (W i = 0; i < 5; ++i)
        if (word(0x4965d8 + i * 48u) == cp)
        {
            found = i;
            break;
        }
    if (found != 0xffffffffu)
    {
        for (W i = 0; i < 257; ++i)
            byte(0x636980 + i, 0);
        for (W group = 0; group < 4; ++group)
        {
            auto cursor = 0x4965e8 + found * 48u + group * 8u;
            while (byte(cursor) != 0)
            {
                const auto first = byte(cursor), last = byte(cursor + 1);
                for (W c = first; c <= last; ++c)
                    byte(0x636981 + c, static_cast<std::uint8_t>(byte(0x636981 + c) | byte(0x4965d0 + group)));
                cursor += 2;
            }
        }
        word(0x63687c, 1);
        word(0x636860, cp);
        word(0x636a84, cp_locale(cp));
        for (W i = 0; i < 3; ++i)
            word(0x636870 + i * 4u, word(0x4965dc + found * 48u + i * 4u));
    }
    else
    {
        CPINFO info{};
        if (GetCPInfo(cp, &info) != 1)
        {
            if (word(0x636848) == 0)
                return; // Source returns-1; caller46CBE7 discards it.
            clear_mb(*this);
            update_case(*this);
            return;
        }
        word(0x636860, cp);
        for (W i = 0; i < 257; ++i)
            byte(0x636980 + i, 0);
        word(0x636a84, 0);
        if (info.MaxCharSize > 1)
        {
            for (unsigned i = 0; i + 1 < MAX_LEADBYTES && info.LeadByte[i] != 0; i += 2)
            {
                if (info.LeadByte[i + 1] == 0)
                    break;
                for (W c = info.LeadByte[i]; c <= info.LeadByte[i + 1]; ++c)
                    byte(0x636981 + c, static_cast<std::uint8_t>(byte(0x636981 + c) | 4u));
            }
            for (W c = 1; c < 255; ++c)
                byte(0x636981 + c, static_cast<std::uint8_t>(byte(0x636981 + c) | 8u));
            word(0x636a84, cp_locale(cp));
            word(0x63687c, 1);
        }
        else
            word(0x63687c, 0);
        for (W a = 0x636870; a < 0x63687c; a += 4)
            word(a, 0);
    }
    update_case(*this);
}
} // namespace sgn::retail
