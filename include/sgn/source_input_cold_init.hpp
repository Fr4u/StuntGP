#pragma once
#include "sgn/source_input_dispatch.hpp"
#include "sgn/source_force_feedback_state.hpp"
#include "sgn/retail_frontend_launch.hpp"

namespace sgn::retail
{
class RaceSessionPlatform;
// Invocation-only borrow of the SAME world. FF physical words lie INSIDE the
// four cleared records; FF globals, COM owners and upstream do not. The
// source-input-owner.patch adds only38 previously unowned bytes per record.
struct SourceInputColdWorld
{
    SourceInputStorage &input;
    SessionMenuInputState &menu;
    FrontendLaunchState &frontend;
    SourceForceFeedbackStorage &feedback;
};

// Complete45EC30..45EC72:476 forward DWORD clears across62D580..62DCEF,
// then nine DWORD stores for each of the four original physical records.
// No source calls, input discovery, COM, keyboard seed, profile selection,
// vehicle publication or whole-SourceInputStorage assignment occurs here.
// Distinct source bytes bind distinct storage. Single thread, no concurrent
// observers: native scattered byte stores preserve source DWORD order, not
// x86 atomic-memory-store semantics or CPU register/DF/EFLAGS side effects.
// Like the actual leaf, this can be explicitly called again and will erase
// the same physical bindings. No hidden once flag reinterprets source behavior.
void initialize_source_physical_input_45ec30(SourceInputColdWorld) noexcept;

// Invocation-only platform binding, available before Session attachment.
// Uses the same world refs and creating-thread guard as the Session entry.
// No clock, HWND/input attachment,4249B0 or additional reset is performed.
void initialize_platform_source_physical_input_45ec30(RaceSessionPlatform &);

// Actual Session member borrow, checked on the platform's owning thread.
// Explicit entry ONLY. Never automatically invoked by Session construction,
// acquire/dispatch, frontend bind, scene restart, COM init or FF creation.
// Original callsite442157 belongs to cold442120; see exact route evidence.
// Delegates once to the platform binding above; the source leaf remains shared.
void initialize_session_source_physical_input_45ec30(RaceSession &);
} // namespace sgn::retail
