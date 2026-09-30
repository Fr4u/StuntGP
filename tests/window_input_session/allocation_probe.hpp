#pragma once
#include <atomic>

// Receiving-only observation of the actual world's last vector allocation.
// No game callback, world destructor or production allocator is patched.
namespace allocation_probe {
inline std::atomic<void*> watched{};
inline std::atomic<bool> unloaded{},freed{},freed_after_unload{};
inline void arm(void* p) noexcept {
    unloaded=false;freed=false;freed_after_unload=false;watched=p;
}
inline void on_free(void* p) noexcept {
    if(p && watched.load()==p){
        freed_after_unload=unloaded.load();freed=true;watched=nullptr;
    }
}
}
