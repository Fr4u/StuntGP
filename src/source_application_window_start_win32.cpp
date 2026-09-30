#include "sgn/source_application_window_start_win32.hpp"
#include "sgn/menu_keyboard_acquisition.hpp"
#include <Windows.h>
#include <stdexcept>

namespace sgn::retail
{
ATOM register_source_window_class_44e8ac(HINSTANCE instance, WNDPROC procedure, const char *menu_name,
                                         const char *class_name)
{
    if (!procedure || !menu_name || !class_name)
        throw std::logic_error("44E8AC requires bound source callback and literal strings");
    WNDCLASSA window_class{};
    window_class.style = 0x20;
    window_class.lpfnWndProc = procedure;
    window_class.cbClsExtra = 0;
    window_class.cbWndExtra = 0;
    window_class.hInstance = instance;
    window_class.hIcon = LoadIconA(instance, MAKEINTRESOURCEA(0x7f00));
    window_class.hCursor = nullptr;
    window_class.hbrBackground = static_cast<HBRUSH>(GetStockObject(5));
    window_class.lpszMenuName = menu_name;
    window_class.lpszClassName = class_name;
    // Do not treat ERROR_CLASS_ALREADY_EXISTS as success. Source branches on
    // the actual ATOM, including a repeated registration returning zero.
    return RegisterClassA(&window_class);
}
std::uintptr_t create_source_window_422700(std::uintptr_t instance, const char *class_name, const char *title)
{
    // Original evaluates height first. Do not replace with desktop rectangle,
    // requested render resolution, or cached dimensions.
    const int height = GetSystemMetrics(0x11);
    const int width = GetSystemMetrics(0x10);
    const HWND window = CreateWindowExA(0x40000u, class_name, title, 0x80000000u, 0, 0, width, height, nullptr, nullptr,
                                        reinterpret_cast<HINSTANCE>(instance), nullptr);
    if (!window)
        return 0;
    (void)ShowWindow(window, 1);
    (void)UpdateWindow(window);
    return reinterpret_cast<std::uintptr_t>(window);
}
void acquire_source_keyboard_44e870(RaceInitializationState &startup)
{
    // The same retained 62D480 payload consumed by existing menu acquisition.
    // A failed API call does not cause an invented zero buffer or early exit.
    auto bytes = menu_keyboard_bytes(startup);
    for (unsigned remaining = 10; remaining != 0; --remaining)
        (void)GetKeyboardState(bytes.data());
}
} // namespace sgn::retail
