#pragma once
#include "sgn/menu_directinput_acquisition.hpp"
#include "sgn/retail_pause_input.hpp"
#include "sgn/retail_pause_toggle.hpp"
#include "sgn/vehicle.hpp"
#include "sgn/source_force_feedback_state.hpp"
#include <array>
#include <functional>

namespace sgn::retail
{
class RaceSession;
class FrontendState;
class Win32MenuDirectInputStartup;
struct SourceForceFeedbackWorld;

// Five original banks, each four 13-byte profiles. DWORD backing permits the
// actual473984 DWORD and473982/3 byte aliases in bank473978. Four bytes between
// the source banks are not profile storage. No second menu-selector owner.
using SourceInputProfiles = std::array<std::array<std::uint32_t, 13>, 5>;
std::span<const std::uint8_t, 13> source_input_profile(const SourceInputProfiles &, unsigned bank, std::uint32_t index);
MenuDeviceSelectorsView source_menu_selectors(const SourceInputProfiles &) noexcept;
struct SourcePhysicalInputExtension
{
    std::uint32_t activity_kind{}; // physical+10,45F157..45F1C6
    std::uint32_t profile_index{}; // physical+14,45EC54 initializes zero
    std::uint32_t feedback_mode{}; // physical+20,45EF1A
    // Existing raw payload ends at+167. Old-button lookup+88+uint8 can reach
    //+187; preserve those additional32 bytes without overlapping menu edges.
    std::array<std::uint8_t, 32> bytes_168_187{};
    // Remaining physical bytes cleared by45EC30, with no extra semantics.
    // +1B8..1D7 belongs to force_feedback.physical_words, NOT this tail.
    std::array<std::uint8_t, 32> bytes_188_1a7{};
    std::array<std::uint8_t, 2> bytes_1b6_1b7{};
    std::array<std::uint8_t, 4> bytes_1d8_1db{};
};
// Missing source fields owned once by RaceSessionState::source_input.
// The shared platform retains this owner for the whole input lifetime.
// Loader startup borrows acquisition; do not clear it again after4249B0.
struct SourceInputStorage
{
    SourceInputStorage(); // literal pinned PE profile bytes, all other additions zero
    SourceInputStorage(const SourceInputStorage &) = delete;
    SourceInputStorage &operator=(const SourceInputStorage &) = delete;
    SourceInputStorage(SourceInputStorage &&) = delete;
    SourceInputStorage &operator=(SourceInputStorage &&) = delete;
    MenuKeyboardRawState acquisition;
    SourceForceFeedbackStorage force_feedback; // sole missing FF owners, same persistent input lifetime
    std::array<SourcePhysicalInputExtension, 4> physical;
    SourceInputProfiles profiles;
    std::array<std::array<std::uint8_t, 13>, 6> vehicle_profiles{}; // 2284..2290
    std::uint32_t previous_activity{}; // 61296C;612970 is existingSession.pause.raw_activity
};

struct SourcePhysicalInputView
{
    const std::uint32_t &active;              //+0,existingSession.menu_input
    const std::uint32_t &blocked;             //+8,existingacquisition.physical
    const std::uint32_t &kind;                //+C,actual raw kind, not frontend+1C
    std::uint32_t &activity_kind;             //+10
    const std::uint32_t &profile_index;       //+14
    const FrontendInputBinding &binding;      //+18/+1C,existingfrontend sole owner
    std::uint32_t &feedback_mode;             //+20
    const std::uint32_t &feedback_request;    //+24,existingraw payload extension
    std::uint32_t &previous_feedback_request; //+28,existingraw extension
    const MenuPhysicalRawExtension &raw;
    const std::array<std::uint8_t, 32> &tail;
    MenuInputBankView edges; // same seven current/previous physical bytes as40D860
};
struct SourceVehicleInputView
{
    const std::uint32_t &enabled; // 2280,existingVehicleRaceStartupState
    VehicleInput &current;
    std::array<std::reference_wrapper<std::uint8_t>, 10> previous; // existing split owners22A0..A9
    std::span<std::uint8_t, 13> profile;                           // only missing2284..2290 owner
    std::int32_t &control_kind;                                    // existingVehicleRaceState227C
    float &steering_sensitivity;                                   // existingVehicleParameters254
};
struct SourceInputWorld
{
    const std::uint32_t &frame_pause;
    std::span<const std::uint8_t, 256> keyboard;
    std::span<const std::uint8_t, 256> previous_keyboard;
    std::span<const SourcePhysicalInputView, 4> physical;
    std::span<const SourceVehicleInputView, 6> vehicles;
    const SourceInputProfiles &profiles;
    std::uint32_t &current_vehicle;   // normalized4D5BB8,existingcurrent_slot
    std::uint32_t &source_device;     // 576B6C,existingacquisition owner
    std::uint32_t &confirm;           // 612968,existingnormal_finish.confirm_pressed
    std::uint32_t &previous_confirm;  // 612964,existingnormal_finish.confirm_consumed
    std::uint32_t &activity;          // 612970,existingpause.raw_activity
    std::uint32_t &previous_activity; // 61296C
    PauseToggleWorld toggle;          // actual already-qualified40C750 children
    MenuInputProducerWorld menu;      // actual already-proposed complete40D860
};

enum class SourceInputStatus
{
    completed,
    force_feedback_410a10_required
};
struct SourceInputResult
{
    SourceInputStatus status{SourceInputStatus::completed};
    std::uint32_t physical_slot{0xffffffffu};
};
// With a real same-owner feedback world,40DCE1 calls complete410A10 and then
// continues the SAME dispatcher invocation. Without it a reached FF branch
// retains the explicit required-child outcome and MUST stop the outer prefix.
// Paused exactly1 calls existing40D860 and returns BEFORE all vehicle writes.
[[nodiscard]] SourceInputResult dispatch_source_input_40dad0(SourceInputWorld &,
                                                             SourceForceFeedbackWorld *feedback = nullptr);
void source_keyboard_input_40c8f0(SourceInputWorld &, unsigned physical_slot);
void source_analog_input_40c780(SourceInputWorld &, unsigned physical_slot);
void source_button_input_40ca60(SourceInputWorld &, unsigned physical_slot);
void publish_input_feedback_kind_45ef00(std::span<const SourcePhysicalInputView, 4>);
void update_source_input_activity_45f110(SourceInputWorld &); // includes full45EF30/45F020

// Exact two binary64 operations: source263.0/signed_range then *signed_axis;
// source CW53, round-nearest, masked divide/invalid exceptions. Range0 yields
// FISTP64 indefinite lowDWORD0. No x87 status/exception flags are modelled.
std::uint32_t source_analog_axis_40c780(std::uint32_t axis, std::uint32_t range);

// Same actual frontend/platform input owner for Core442810. Runs full45F210
// then40DAD0, including the existing force-feedback implementation. No input
// reset, copied profile/keyboard, preview Session or lazy OS owner is created.
[[nodiscard]] SourceInputResult acquire_dispatch_source_platform_input(FrontendState &, MenuKeyboardAcquisition &,
                                                                       Win32MenuDirectInputStartup &);

// Actual Session binding, invocation-only views. Full45F210 composition:
// existing acquisition/publish prefix,45EF00,45F110,literalRET45EC20,45EE60;
// then40DAD0. No new OS stub. Call ONCE per source outer442810 invocation,
// after4023F0 and before442490/442838 catch-up loop; never per display draw or
// each world tick. Input owners, startup and the actual registry must be live.
// Does not call45EC30 or4249B0 and does not invent frontend bindings.
[[nodiscard]] SourceInputResult acquire_dispatch_session_input(RaceSession &, SourceInputStorage &,
                                                               MenuKeyboardAcquisition &,
                                                               const MenuDirectInputRegistry &);
// New host binding: selects the persistent input owner of this Session.
// Both overloads reject foreign-thread use; the explicit overload also
// rejects another input owner before any source write or OS acquisition.
// Keyboard and registry authenticity remain the caller's precondition.
[[nodiscard]] SourceInputResult acquire_dispatch_session_input(RaceSession &, MenuKeyboardAcquisition &,
                                                               const MenuDirectInputRegistry &);
// Complete FF composition: obtains this startup's actual registry and verifies
// its raw owner before acquisition; no arbitrary cross-Session registry input.
[[nodiscard]] SourceInputResult acquire_dispatch_session_input(RaceSession &, SourceInputStorage &,
                                                               MenuKeyboardAcquisition &,
                                                               Win32MenuDirectInputStartup &);
[[nodiscard]] SourceInputResult acquire_dispatch_session_input(RaceSession &, MenuKeyboardAcquisition &,
                                                               Win32MenuDirectInputStartup &);
} // namespace sgn::retail
