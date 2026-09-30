#pragma once
#include <cstdint>
#include <memory>

namespace sgn::retail
{
class RaceSessionPlatform;
struct RaceInitializationState;

struct SourceSceneKeyboardPrefixResult
{
    std::uint32_t next_required_instruction{0x42e55a};
};

// NEW synchronous read-only observation after each completed source DWORD.
// Throwing retains that write and prevents subsequent writes in this call.
struct SourceSceneKeyboardPrefixObserver
{
    virtual ~SourceSceneKeyboardPrefixObserver() = default;
    virtual void cleared(std::uint32_t instruction, std::uint32_t address, const RaceInitializationState &) = 0;
};

// 42E542 and42E54E: forward REP STOSD, EAX=0,64 DWORDs per bank.
// The caller reaches this leaf after the separate42E53C indexed-base store.
// Uses the platform's sole keyboard owners; stops before61C5F4 at42E55A.
// Retains the platform for all stores/observers and enforces its owner thread.
SourceSceneKeyboardPrefixResult reset_source_scene_keyboard_42e542(const std::weak_ptr<RaceSessionPlatform> &,
                                                                   SourceSceneKeyboardPrefixObserver * = nullptr);
} // namespace sgn::retail
