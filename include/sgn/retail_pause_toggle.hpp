#pragma once
#include "sgn/retail_pause_lifecycle.hpp"

namespace sgn::retail
{
struct PauseToggleWorld
{
    const std::uint32_t &scene; // 612BE8; only exact1 proceeds
    PauseLifecycleWorld lifecycle;
};
// Full40C750..40C777:40bytes/10instructions; typed void. The incoming token
// represents retail ECX (0=null,1..4=physical input slots0..3), not a host key.
// Scene!=1 reads only scene. Frame_pause==1 invokes the real resume child;
// every other raw value publishes the incoming identity BEFORE real entry,
// even if inhibit==1 refuses entry. Previous_pause612950 is never accessed.
// Synchronous distinct coherent borrowed storage; nested lifecycle contracts
// apply only when their respective child is reached. No callback/concurrent
// mutation. Volatile preserves source access order, not thread synchronization.
void toggle_pause_40c750(PauseToggleWorld, PausePhysicalInputIdentity incoming);
} // namespace sgn::retail
