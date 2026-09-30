#include "sgn/source_application_messages_win32.hpp"
#include <Windows.h>
#include <stdexcept>

namespace sgn::retail
{

struct Win32SourceApplicationMessages::Impl
{
    const std::exception_ptr &window_failure;
    MSG message{};
    bool received{};
    explicit Impl(const std::exception_ptr &failure) : window_failure(failure) {}
    const DWORD owner_thread{GetCurrentThreadId()};
    void require_thread() const
    {
        if (GetCurrentThreadId() != owner_thread)
        {
            throw std::logic_error("Application message queue used from another thread");
        }
    }
    void rethrow_native_failure() const
    {
        if (window_failure)
            std::rethrow_exception(window_failure);
    }
    void require_message() const
    {
        require_thread();
        if (!received)
        {
            throw std::logic_error("Application message accessed without successful PeekMessage");
        }
    }
};

Win32SourceApplicationMessages::Win32SourceApplicationMessages(const std::exception_ptr &window_failure)
    : impl_(std::make_unique<Impl>(window_failure))
{
}
Win32SourceApplicationMessages::~Win32SourceApplicationMessages() = default;

bool Win32SourceApplicationMessages::peek_removed()
{
    impl_->require_thread();
    impl_->rethrow_native_failure();
    impl_->received = PeekMessageA(&impl_->message, nullptr, 0, 0, PM_REMOVE) != FALSE;
    // PeekMessage may dispatch sent messages even when it returns false.
    // Callbacks dispatched by this call have returned. The outer App cleanup
    // must still independently require its complete callback_depth to be zero.
    impl_->rethrow_native_failure();
    return impl_->received;
}

std::uint32_t Win32SourceApplicationMessages::message_id() const
{
    impl_->require_message();
    return impl_->message.message;
}

std::uint32_t Win32SourceApplicationMessages::source_wparam() const
{
    impl_->require_message();
    // Only the source's returned DWORD is projected. Translate/Dispatch below
    // receive the untouched native MSG, including 64-bit WPARAM and LPARAM.
    return static_cast<std::uint32_t>(impl_->message.wParam);
}

void Win32SourceApplicationMessages::translate()
{
    impl_->require_message();
    (void)TranslateMessage(&impl_->message);
}

void Win32SourceApplicationMessages::dispatch()
{
    impl_->require_message();
    impl_->rethrow_native_failure();
    (void)DispatchMessageA(&impl_->message);
    impl_->rethrow_native_failure();
}

} // namespace sgn::retail
