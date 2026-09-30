#include "sgn/menu_directinput_startup.hpp"
#include "menu_directinput_source_data.hpp"
#include <dinput.h>
#include <bit>
#include <cstddef>
#include <cstring>
#include <exception>
#include <optional>
#include <vector>

namespace sgn::retail
{
namespace
{
constexpr std::uint32_t bank_address = 0x572ce8u;
constexpr std::uint32_t record_stride = 0x5a4u;
static_assert(sizeof(GUID) == 16 && sizeof(DWORD) == 4);
static_assert(sizeof(DIPROPDWORD) == 20 && sizeof(DIPROPRANGE) == 24);
static_assert(sizeof(DIDEVICEINSTANCEA) == 0x244);
static_assert(offsetof(DIDEVICEINSTANCEA, guidInstance) == 4);
static_assert(offsetof(DIDEVICEINSTANCEA, dwDevType) == 0x24);
static_assert(offsetof(DIEFFECTINFOA, guid) == 4);

using LegacyCreate = HRESULT(WINAPI *)(HINSTANCE, DWORD, LPDIRECTINPUTA *, LPUNKNOWN);
struct SystemDirectInputModule
{
    HMODULE handle{};
    SystemDirectInputModule()
    {
        handle = ::LoadLibraryExW(L"dinput.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (!handle)
            throw FormatError("system dinput.dll is unavailable");
    }
    ~SystemDirectInputModule()
    {
        if (handle)
            (void)::FreeLibrary(handle);
    }
    SystemDirectInputModule(const SystemDirectInputModule &) = delete;
    SystemDirectInputModule &operator=(const SystemDirectInputModule &) = delete;
};

// Native descriptor sizes account for x64 pointer width. Every object GUID,
// offset, type and flag is copied from pinned462780/4624C0, not an SDK default.
struct NativeSourceFormat
{
    std::array<DIOBJECTDATAFORMAT, 44> objects{};
    DIDATAFORMAT format{};
    NativeSourceFormat() noexcept
    {
        for (std::size_t i = 0; i != objects.size(); ++i)
        {
            const auto &source = direct_input_source_data::objects[i];
            objects[i] = {source.guid_index < 0
                              ? nullptr
                              : &direct_input_source_data::object_guids[static_cast<std::size_t>(source.guid_index)],
                          source.offset, source.type, source.flags};
        }
        format = {static_cast<DWORD>(sizeof(DIDATAFORMAT)),
                  static_cast<DWORD>(sizeof(DIOBJECTDATAFORMAT)),
                  1u,
                  80u,
                  44u,
                  objects.data()};
    }
};

// Complete4246C0, including ignored zeroed remainder and exact HRESULT==0.
std::uint32_t device_kind_4246c0(IDirectInputDeviceA &device)
{
    DIDEVICEINSTANCEA info{};
    info.dwSize = 0x244u;
    if (device.GetDeviceInfo(&info) != 0)
        return 5u;
    const auto subtype = (info.dwDevType >> 8u) & 0xffu;
    return subtype == 6u ? 2u : 5u; // 424720: five default targets, then42470E
}

// Both source callbacks use the same four requests in this exact order, but
// call through DIFFERENT interface receivers. Failure leaves earlier effects.
bool set_axes_and_deadzone(IDirectInputDeviceA &receiver)
{
    DIPROPRANGE range{{24u, 16u, 0u, 1u}, -5000, 5000};
    if (receiver.SetProperty(DIPROP_RANGE, &range.diph) != 0)
        return false;
    range.diph.dwObj = 4u;
    if (receiver.SetProperty(DIPROP_RANGE, &range.diph) != 0)
        return false;
    DIPROPDWORD deadzone{{20u, 16u, 0u, 1u}, 1250u};
    if (receiver.SetProperty(DIPROP_DEADZONE, &deadzone.diph) != 0)
        return false;
    deadzone.diph.dwObj = 4u;
    return receiver.SetProperty(DIPROP_DEADZONE, &deadzone.diph) == 0;
}
} // namespace

struct Win32MenuDirectInputStartup::Impl
{
    enum class Phase
    {
        fresh,
        starting,
        returned,
        host_failed,
        closing,
        closed,
        finalized
    };
    MenuKeyboardRawState &raw;
    MenuDirectInputRegistry registry;
    MenuDirectInputStartupState state;
    std::array<std::optional<Win32MenuDirectInputDevice>, 10> adapters;
    // Each entry owns ONE returned COM reference, even if pointers compare
    // equal. Source releases consume entries; finalization consumes leftovers.
    struct Reference
    {
        IUnknown *pointer{};
        bool effect{};
    };
    std::vector<Reference> references; // one ledger, typed provenance only
    SystemDirectInputModule module;
    NativeSourceFormat source_format;
    LegacyCreate factory{};
    HINSTANCE instance{};
    HWND window{};
    DWORD owner_thread{};
    Phase phase{Phase::fresh};
    std::exception_ptr callback_exception;

    Impl(MenuKeyboardRawState &raw_owner, std::uintptr_t native_instance, std::uintptr_t native_window)
        : raw(raw_owner), instance(reinterpret_cast<HINSTANCE>(native_instance)),
          window(reinterpret_cast<HWND>(native_window))
    {
        DWORD process{};
        owner_thread = ::GetWindowThreadProcessId(window, &process);
        if (!instance || !owner_thread || process != ::GetCurrentProcessId() ||
            owner_thread != ::GetCurrentThreadId() || ::GetAncestor(window, GA_ROOT) != window)
            throw FormatError("DirectInput startup requires a local top-level window on its message thread");
        const auto symbol = ::GetProcAddress(module.handle, "DirectInputCreateA");
        if (!symbol)
            throw FormatError("system dinput.dll has no DirectInputCreateA");
        static_assert(sizeof(symbol) == sizeof(factory));
        factory = std::bit_cast<LegacyCreate>(symbol);
    }

    ~Impl()
    {
        finalize();
    }

    void require_thread_and_window() const
    {
        if (::GetCurrentThreadId() != owner_thread || ::GetWindowThreadProcessId(window, nullptr) != owner_thread)
            throw FormatError("DirectInput window or message thread changed");
    }
    std::size_t cursor_index() const
    {
        const auto offset = state.cursor_576550 - bank_address;
        if (offset % record_stride != 0 || offset / record_stride >= state.records.size())
            throw FormatError("DirectInput cursor leaves the ten-record source memory domain");
        return offset / record_stride;
    }
    IDirectInputA &root() const
    {
        if (!state.root_576b68)
            throw FormatError("source DirectInput root is null");
        return *state.root_576b68;
    }
    IDirectInputDevice2A &device_at(std::size_t index) const
    {
        if (index >= state.records.size() || !state.records[index].device)
            throw FormatError("source DirectInput device slot is invalid");
        return *state.records[index].device;
    }
    std::size_t reserve_reference(bool effect = false)
    {
        // Allocate BEFORE the external acquisition, so bad_alloc cannot lose
        // a successfully returned native reference. Index survives relocation.
        references.push_back({nullptr, effect});
        return references.size() - 1u;
    }
    void remember(std::size_t index, HRESULT result, IUnknown *pointer) noexcept
    {
        // Positive HRESULTs are source failures but still COM successes. Any
        // returned reference is kept for the separate host finalizer.
        if (SUCCEEDED(result))
            references[index].pointer = pointer;
    }
    void release_source(IUnknown *pointer)
    {
        if (!pointer)
            throw FormatError("source Release receiver is null");
        for (auto it = references.rbegin(); it != references.rend(); ++it)
        {
            if (it->pointer == pointer)
            {
                it->pointer = nullptr;
                (void)pointer->Release();
                return;
            }
        }
        // Compatibility with explicit externally supplied source references.
        // A pointer cannot reach force-feedback Set/Start through this fallback.
        (void)pointer->Release();
    }
    void finalize() noexcept
    {
        if (phase == Phase::finalized)
            return;
        phase = Phase::finalized;
        // New host behavior, never counted as a424930 call or source store.
        // Device/effect/root/count identities in state remain untouched.
        for (auto it = references.rbegin(); it != references.rend(); ++it)
        {
            if (auto *pointer = it->pointer)
            {
                it->pointer = nullptr;
                (void)pointer->Release();
            }
        }
    }

    void require_feedback_owner(MenuKeyboardRawState &supplied) const
    {
        require_thread_and_window();
        if (&supplied != &raw || phase != Phase::returned)
            throw FormatError("force feedback requires the same initialized input lifetime");
    }
    void require_owned_effect(IDirectInputEffect &receiver) const
    {
        const auto *pointer = static_cast<IUnknown *>(&receiver);
        for (const auto &entry : references)
            if (entry.effect && entry.pointer == pointer)
                return;
        throw FormatError("force feedback receiver is not an effect reference owned by this startup");
    }
    std::uint32_t create_feedback(MenuKeyboardRawState &supplied, unsigned index, unsigned slot,
                                  const std::array<std::uint32_t, 4> &source_guid, const DIEFFECT &descriptor)
    {
        require_feedback_owner(supplied);
        auto &device = device_at(index); // current record, never acquisition's cached adapter
        if (slot >= state.records[index].effects.size())
            throw FormatError("force feedback output leaves the source effect bank");
        GUID guid{};
        std::memcpy(&guid, source_guid.data(), sizeof(guid));
        const auto entry = reserve_reference(true); // before external COM; index survives relocation
        auto &output = state.records[index].effects[slot].effect;
        // Real pointer-sized source destination. No clear, local out-copy,
        // reinterpret_cast<IUnknown**>, AddRef or Release of the old value.
        const auto result = device.CreateEffect(guid, &descriptor, &output, nullptr);
        remember(entry, result, output); // COM success includes positive HRESULTs
        return static_cast<std::uint32_t>(result);
    }
    std::uint32_t set_feedback(MenuKeyboardRawState &supplied, IDirectInputEffect &receiver, const DIEFFECT &descriptor,
                               std::uint32_t flags)
    {
        require_feedback_owner(supplied);
        require_owned_effect(receiver);
        return static_cast<std::uint32_t>(receiver.SetParameters(&descriptor, flags));
    }
    std::uint32_t start_feedback(MenuKeyboardRawState &supplied, IDirectInputEffect &receiver, std::uint32_t iterations,
                                 std::uint32_t flags)
    {
        require_feedback_owner(supplied);
        require_owned_effect(receiver);
        return static_cast<std::uint32_t>(receiver.Start(iterations, flags));
    }

    HRESULT create_device(const DIDEVICEINSTANCEA &info, IDirectInputDeviceA *&basic)
    {
        const auto entry = reserve_reference();
        const auto result = root().CreateDevice(info.guidInstance, &basic, nullptr);
        remember(entry, result, basic);
        return result;
    }
    HRESULT query_device2(IDirectInputDeviceA &basic)
    {
        const auto index = cursor_index(); // actual QI output destination
        const auto entry = reserve_reference();
        const auto result = basic.QueryInterface(direct_input_source_data::device2_iid,
                                                 reinterpret_cast<void **>(&state.records[index].device));
        remember(entry, result, state.records[index].device);
        // Borrowed native adapters never AddRef/Release. A failed QI can clear
        // the source slot, without releasing the previously orphaned pointer.
        adapters[index].reset();
        registry.devices[index] = nullptr;
        if (state.records[index].device)
        {
            adapters[index].emplace(*state.records[index].device);
            registry.devices[index] = &*adapters[index];
        }
        return result;
    }

    struct EffectContext
    {
        Impl &self;
        GUID last_guid;
    };
    static BOOL CALLBACK enumerate_effect_423a70(LPCDIEFFECTINFOA info, LPVOID context) noexcept
    {
        auto &target = *static_cast<EffectContext *>(context);
        // Native API guarantees a valid effect info during this callback.
        target.last_guid = info->guid;
        target.self.state.flag_576b74 = 1u;
        return TRUE; // 423A92/423A97; pseudocode's void result is incorrect.
    }
    std::uint32_t enumerate_effects_423aa0()
    {
        // Source GUID is an uninitialized local; callback only overwrites it,
        // and no caller consumes it. It is not a persistent effect selection.
        EffectContext context{*this, {}};
        const auto result = device_at(cursor_index()).EnumEffects(&enumerate_effect_423a70, &context, 0u);
        return result == 0 ? 1u : 0u;
    }
    void clear_effects_423950()
    {
        // Source clears all12*(pointer+29DWORD) records WITHOUT Release.
        auto &effects = state.records[cursor_index()].effects;
        for (auto &effect : effects)
        {
            effect.effect = nullptr;
            effect.payload.fill(0u);
        }
    }

    BOOL enumerate_first_423b00(const DIDEVICEINSTANCEA &info)
    {
        IDirectInputDeviceA *basic{};
        if (create_device(info, basic) != 0)
            return TRUE; // 423B1E
        if (!basic)
            throw FormatError("CreateDevice succeeded with a null receiver");
        if (query_device2(*basic) != 0)
        {                          // 423B44: QI BEFORE configuration
            release_source(basic); // 423B50
            return FALSE;
        }
        auto &extended = device_at(cursor_index()); // 423B6C: latched ESI
        if (extended.SetCooperativeLevel(window, 5u) != 0)
        {                          // 423B71
            release_source(basic); // 423B7E; leave source QI slot intact
            return TRUE;
        }
        if (extended.SetDataFormat(&source_format.format) != 0)
        {                          // 423B96
            release_source(basic); // 423BA3
            return TRUE;
        }
        DIPROPDWORD autocenter{{20u, 16u, 0u, 0u}, 0u};
        (void)extended.SetProperty(DIPROP_AUTOCENTER, &autocenter.diph); // 423BD3 ignored
        (void)enumerate_effects_423aa0();                                // 423BDC ignored; current cursor reread
        if (!set_axes_and_deadzone(extended))
        {                          // 423C10/3A/7D/A7
            release_source(basic); // 423C1D/47/8A/B4
            return FALSE;
        }
        clear_effects_423950(); // 423CC7: current cursor reread
        const auto upstream_slot = registry.device_count + 1u;
        if (upstream_slot < 10u)
        {
            auto &output = raw.upstream[upstream_slot].words;
            output[0] = 1u;                         // 423CE7, before external GetDeviceInfo
            output[1] = device_kind_4246c0(*basic); // 423CED/F2: BASIC receiver
            output[2] = 2u;                         // 423CF8
        }
        const auto next_cursor = state.cursor_576550 + record_stride; // 423D02/0D
        const auto next_count = registry.device_count + 1u;           // 423D08/13
        registry.device_count = next_count;                           // 423D14
        state.flag_576b74 = 1u;                                       // 423D19
        state.cursor_576550 = next_cursor;                            // 423D22
        return TRUE; // No successful basic Release exists in the source.
    }

    BOOL enumerate_second_424740(const DIDEVICEINSTANCEA &info)
    {
        IDirectInputDeviceA *basic{};
        if (create_device(info, basic) != 0)
            return TRUE; // 42475C
        if (!basic)
            throw FormatError("CreateDevice succeeded with a null receiver");
        if (basic->SetDataFormat(&source_format.format) != 0)
        {                          // 424772: BASIC first
            release_source(basic); // 42477F
            return TRUE;
        }
        if (basic->SetCooperativeLevel(window, 6u) != 0)
        {                          // 42479A
            release_source(basic); // 4247A7
            return TRUE;
        }
        if (!set_axes_and_deadzone(*basic))
        {                          // 4247ED/424819/5D/89
            release_source(basic); // 4247FA/424826/6A/96
            return FALSE;
        }
        if (query_device2(*basic) != 0)
        {                          // 4248B1: QI AFTER configuration
            release_source(basic); // 4248BD
            return FALSE;
        }
        const auto upstream_slot = registry.device_count + 1u;
        if (upstream_slot < 10u)
        {
            auto &output = raw.upstream[upstream_slot].words;
            output[0] = 1u;                         // 4248E1
            output[1] = device_kind_4246c0(*basic); // 4248EB/F0
            output[2] = 0u;                         // 4248F6
        }
        const auto next_cursor = state.cursor_576550 + record_stride;
        const auto next_count = registry.device_count + 1u;
        state.cursor_576550 = next_cursor;  // 424912: BEFORE count, unlike423B00
        registry.device_count = next_count; // 424918
        return TRUE;                        // No flag store, effects clear or successful basic Release.
    }

    static BOOL CALLBACK first_callback(LPCDIDEVICEINSTANCEA info, LPVOID context) noexcept
    {
        auto &self = *static_cast<Impl *>(context);
        try
        {
            return self.enumerate_first_423b00(*info);
        }
        catch (...)
        {
            self.callback_exception = std::current_exception();
            return FALSE;
        }
    }
    static BOOL CALLBACK second_callback(LPCDIDEVICEINSTANCEA info, LPVOID context) noexcept
    {
        auto &self = *static_cast<Impl *>(context);
        try
        {
            return self.enumerate_second_424740(*info);
        }
        catch (...)
        {
            self.callback_exception = std::current_exception();
            return FALSE;
        }
    }
    void check_callback_exception()
    {
        if (callback_exception)
            std::rethrow_exception(callback_exception);
    }

    std::uint32_t initialize()
    {
        require_thread_and_window();
        if (phase != Phase::fresh)
            throw FormatError("DirectInput source startup already entered");
        phase = Phase::starting;
        try
        {
            // 4249BD then4249C9. Do not clear physical/key/edge owners or flag576B74.
            for (auto &record : state.records)
                record = {};
            for (auto &upstream : raw.upstream)
                upstream.words.fill(0u);
            state.root_576b68 = nullptr;                  // 4249D0
            registry.device_count = 0u;                   // 4249DA: sole count owner
            state.cursor_576550 = bank_address;           // 4249E4
            raw.upstream[0].words[0] = 1u;                // 4249EE
            raw.upstream[0].words[1] = 1u;                // 4249F4
            raw.source_device_pointer_576b6c = 0x5766acu; // 4249FA
            // 4239B0 exact16 DWORD transfer from the four pinned GUIDs.
            static_assert(sizeof(state.effect_guids_576558) == 64);
            std::memcpy(state.effect_guids_576558.data(), direct_input_source_data::effect_guids.data(), 64u);
            for (auto &record : state.records)
                record.device = nullptr; // 424A0E..1E

            const auto root_entry = reserve_reference();
            const auto create_result = factory(instance, 0x500u, &state.root_576b68, nullptr);
            remember(root_entry, create_result, state.root_576b68);
            if (create_result != 0)
            {
                phase = Phase::returned;
                return 0u;
            }
            // Original callbacks ignore pvRef (the source passes its root).
            // Native relocation context carries the SAME owners, no snapshot.
            (void)root().EnumDevices(4u, &first_callback, this, 0x101u); // 424A54 ignored
            check_callback_exception();
            if (registry.device_count == 0u)
            {                                                                           // 424A57..5E
                const auto result = root().EnumDevices(4u, &second_callback, this, 1u); // 424A71
                check_callback_exception();
                if (result != 0)
                {
                    phase = Phase::returned;
                    return 0u;
                }
            }
            phase = Phase::returned;
            return 1u;
        }
        catch (...)
        {
            phase = Phase::host_failed;
            throw;
        }
    }

    void shutdown()
    {
        // 442180 calls422A60/DestroyWindow BEFORE424930. COM ownership and
        // the original message thread survive; no live HWND admission here.
        if (::GetCurrentThreadId() != owner_thread)
            throw FormatError("DirectInput shutdown belongs to its original message thread");
        if (phase != Phase::returned && phase != Phase::host_failed)
            throw FormatError("DirectInput source shutdown requires one entered startup and no previous shutdown");
        phase = Phase::closing;                    // host guard also prevents retry after a partial host failure
        auto *root_to_release = state.root_576b68; // 424934, latched for count0
        if (root_to_release)
        {
            if (registry.device_count != 0u)
            {
                std::uint32_t index = 0u;
                do
                {
                    if (index >= state.records.size())
                        throw FormatError("DirectInput shutdown exceeds the source record bank");
                    for (auto &effect : state.records[index].effects)
                    {
                        if (effect.effect)
                        {
                            release_source(effect.effect); // 424967
                            effect.effect = nullptr;       // 42496A only after Release
                        }
                    }
                    (void)device_at(index).Unacquire(); // 424976..7B
                    release_source(&device_at(index));  // 42497E..83: reread
                    ++index;
                } while (index < registry.device_count); // 424989: reread every time
                root_to_release = state.root_576b68; // 42499D only after device loop
            }
            release_source(root_to_release); // 4249A8
        }
        phase = Phase::closed;
    }
};

Win32MenuDirectInputStartup::Win32MenuDirectInputStartup(MenuKeyboardRawState &raw, std::uintptr_t instance,
                                                         std::uintptr_t window)
    : impl_(std::make_unique<Impl>(raw, instance, window))
{
}
Win32MenuDirectInputStartup::~Win32MenuDirectInputStartup() = default;
std::uint32_t Win32MenuDirectInputStartup::initialize_4249b0()
{
    return impl_->initialize();
}
const MenuDirectInputRegistry &Win32MenuDirectInputStartup::registry_for_acquisition() const
{
    impl_->require_thread_and_window();
    if (impl_->phase != Impl::Phase::returned)
        throw FormatError("DirectInput acquisition requested outside the initialized source lifetime");
    return impl_->registry;
}
const MenuDirectInputStartupState &Win32MenuDirectInputStartup::source_state() const noexcept
{
    return impl_->state;
}
MenuDirectInputStartupState &Win32MenuDirectInputStartup::borrow_force_feedback_state(MenuKeyboardRawState &raw)
{
    impl_->require_feedback_owner(raw);
    return impl_->state;
}
std::uint32_t Win32MenuDirectInputStartup::create_force_feedback_effect(MenuKeyboardRawState &raw, unsigned record,
                                                                        unsigned slot,
                                                                        const std::array<std::uint32_t, 4> &guid,
                                                                        const DIEFFECT &descriptor)
{
    return impl_->create_feedback(raw, record, slot, guid, descriptor);
}
std::uint32_t Win32MenuDirectInputStartup::set_force_feedback_parameters(MenuKeyboardRawState &raw,
                                                                         IDirectInputEffect &receiver,
                                                                         const DIEFFECT &descriptor,
                                                                         std::uint32_t flags)
{
    return impl_->set_feedback(raw, receiver, descriptor, flags);
}
std::uint32_t Win32MenuDirectInputStartup::start_force_feedback(MenuKeyboardRawState &raw, IDirectInputEffect &receiver,
                                                                std::uint32_t iterations, std::uint32_t flags)
{
    return impl_->start_feedback(raw, receiver, iterations, flags);
}
void Win32MenuDirectInputStartup::shutdown_424930()
{
    impl_->shutdown();
}
void Win32MenuDirectInputStartup::finalize_host() noexcept
{
    impl_->finalize();
}
} // namespace sgn::retail
