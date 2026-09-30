#pragma once
#include <cstdint>
#include <Windows.h>
namespace sgn::retail
{
struct RaceInitializationState;
// Recovered registration branch44E8AC..44E8FB. Caller handles previous
// instance and zero ATOM; callback must route to its retained44E6D0 world.
ATOM register_source_window_class_44e8ac(HINSTANCE instance, WNDPROC procedure, const char *menu_name_486474,
                                         const char *class_name_48647c);
// Requires the real registered class (44E6D0 callback bound to its world).
// Parameters are borrowed ANSI strings from the application literal owner.
std::uintptr_t create_source_window_422700(std::uintptr_t instance, const char *class_name, const char *title);
void acquire_source_keyboard_44e870(RaceInitializationState &);
} // namespace sgn::retail
