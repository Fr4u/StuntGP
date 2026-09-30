#pragma once
#include "sgn/source_application_entry.hpp"
#include <exception>
#include <memory>

namespace sgn::retail
{

class Win32SourceApplicationMessages final : public SourceApplicationMessages
{
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    // NEW HOST borrow of the actual App.window_failure lvalue. The slot
    // outlives this adapter and is written only on its owner thread. No copy,
    // clearing, replacement signal or inferred source-initialization state.
    explicit Win32SourceApplicationMessages(const std::exception_ptr &window_failure);
    Win32SourceApplicationMessages(std::exception_ptr &&) = delete;
    Win32SourceApplicationMessages(const std::exception_ptr &&) = delete;
    ~Win32SourceApplicationMessages() override;
    Win32SourceApplicationMessages(const Win32SourceApplicationMessages &) = delete;
    Win32SourceApplicationMessages &operator=(const Win32SourceApplicationMessages &) = delete;
    bool peek_removed() override;
    std::uint32_t message_id() const override;
    std::uint32_t source_wparam() const override;
    void translate() override;
    void dispatch() override;
};

} // namespace sgn::retail
