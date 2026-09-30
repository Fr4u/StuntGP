#pragma once
#include "sgn/menu_input_producer.hpp"
#include "sgn/retail_race_initialization.hpp"
#include "sgn/session_menu_input_state.hpp"
#include <array>
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Source576618 + slot*88, ten actual upstream records. No inferred device
// presence: PE construction is zero; source startup publication is separate.
struct MenuUpstreamRawRecord
{
    std::array<std::uint32_t, 34> words{}; //+0,+4,+8, then31 DWORDs from+C
};
static_assert(sizeof(MenuUpstreamRawRecord) == 0x88);

// Only source data missing from SessionMenuInputState. Existing+0/+4, paired
// edges and frontend+18/+1C remain in their accepted sole owners.
struct MenuPhysicalRawExtension
{
    std::uint32_t word_8{}, word_0c{}, word_24{}, word_28{};
    // Source+2C..167. Axes+34/+38/+3C are indices2/3/4 of THIS array.
    // First31 DWORDs are4244C0's destination. Byte+68 starts at array+3C
    // and has256 retained bytes through+167, without a second button bank.
    std::array<std::uint32_t, 79> words_from_2c{};
    MenuInputExtraEdges extra_edges;
};
struct MenuKeyboardRawState
{
    MenuKeyboardRawState() = default;
    MenuKeyboardRawState(const MenuKeyboardRawState &) = delete;
    MenuKeyboardRawState &operator=(const MenuKeyboardRawState &) = delete;
    MenuKeyboardRawState(MenuKeyboardRawState &&) = delete;
    MenuKeyboardRawState &operator=(MenuKeyboardRawState &&) = delete;
    std::array<MenuUpstreamRawRecord, 10> upstream;
    std::array<MenuPhysicalRawExtension, 4> physical;
    std::uint32_t source_device_pointer_576b6c{};
    // No keyboard array here: borrow startup.fields_62d480/fields_62dcf0.
};

// Typed byte views into the EXISTING64-DWORD buffers, not snapshots.
std::span<std::uint8_t, 256> menu_keyboard_bytes(RaceInitializationState &) noexcept;
std::span<const std::uint8_t, 256> menu_keyboard_bytes(const RaceInitializationState &) noexcept;

std::array<AcquiredPhysicalInputView, 4> menu_upstream_publication_views(const MenuKeyboardRawState &) noexcept;
std::array<PhysicalInputPublicationView, 4> menu_physical_publication_views(MenuKeyboardRawState &,
                                                                            SessionMenuInputState &) noexcept;
std::array<MenuPhysicalInputView, 4> menu_physical_producer_views(MenuKeyboardRawState &,
                                                                  SessionMenuInputState &) noexcept;

// Explicit owned-state PROJECTION of4249C9,4249EE,4249F4,4249FA, not full
// 4249B0. Clears ONLY the10 upstream records, then source slot0 active/kind1
// and576B6C=5766AC. Never called automatically by acquisition or per-frame.
// Does not reset physical records, keyboard, menu edges or accepted bindings.
// Full device initialization is exposed by menu_directinput_startup.hpp.
void initialize_keyboard_upstream_projection_4249b0(MenuKeyboardRawState &) noexcept;

// Platform boundary: the real implementation calls GetKeyboardState on these
// exact256 bytes. It ignores the OS return just as the source does; no clear,
// success fabrication, action mapping or normalization on failure.
class MenuKeyboardAcquisition
{
  public:
    virtual ~MenuKeyboardAcquisition() = default;
    virtual void acquire_45ec00(std::span<std::uint8_t, 256>) = 0;
};

enum class MenuKeyboardDispatchStatus
{
    completed,
    direct_input_424520_required
};
struct MenuKeyboardDispatchResult
{
    MenuKeyboardDispatchStatus status{MenuKeyboardDispatchStatus::completed};
    std::uint32_t missing_slot{0xffffffffu}; // host diagnostic, not a source word
    std::uint32_t missing_raw_kind{};
};
// Source4243F0 dispatcher over10 records. Only exact active1 is eligible.
// Kind1 calls keyboard ONLY at slot0; other kind1 slots do nothing. Raw
// kinds outside1..6 do nothing. An eligible kind2..6 returns a mandatory
// missing-child result AT its call site, preserving earlier keyboard writes.
// Caller must halt the source prefix on that result; there is no fallback,
// skip-DirectInput success, rollback, or speculative all-record pre-scan.
[[nodiscard]] MenuKeyboardDispatchResult acquire_keyboard_domain_4243f0(MenuKeyboardRawState &,
                                                                        RaceInitializationState &,
                                                                        MenuKeyboardAcquisition &);

// Actual45F210 prefix through4244C0 (45F221..45F232), within the keyboard
// domain: clear existing612958, copy64 keyboard DWORDs to existing previous,
// acquire through4243F0, then reuse complete4244C0 from the earlier proposal.
// If blocked,4244C0 is NOT called. This is not full45F210:45EF00,45F110,
// 45EC20 and45EE60 must follow separately before40DAD0/40D860 at source cadence.
[[nodiscard]] MenuKeyboardDispatchResult acquire_publish_keyboard_prefix_45f210(MenuKeyboardRawState &,
                                                                                SessionMenuInputState &,
                                                                                RaceInitializationState &,
                                                                                std::uint32_t &second_pause_612958,
                                                                                MenuKeyboardAcquisition &);

// Windows platform adapter. window is a borrowed HWND represented at native
// pointer width; construct and call on that window's message-pump thread.
// Thread/window validation is new host behavior outside source45EC00.
class Win32MenuKeyboardAcquisition final : public MenuKeyboardAcquisition
{
  public:
    explicit Win32MenuKeyboardAcquisition(std::uintptr_t window);
    Win32MenuKeyboardAcquisition(const Win32MenuKeyboardAcquisition &) = delete;
    Win32MenuKeyboardAcquisition &operator=(const Win32MenuKeyboardAcquisition &) = delete;
    void acquire_45ec00(std::span<std::uint8_t, 256>) override;

  private:
    std::uintptr_t window_;
    std::uint32_t message_thread_;
};
} // namespace sgn::retail
