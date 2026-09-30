#pragma once
#include "sgn/source_application_entry.hpp"
#include "sgn/source_application_shutdown.hpp"
#include <cstdint>
namespace sgn::retail
{
// Borrow the actual CRT words. No reset, snapshot or second retained registry.
struct SourceCrtExitWords
{
    std::uint8_t &return_to_caller_636570;
    std::uint32_t &in_exit_636574;
    std::uint32_t &process_exit_636578;
    const std::uint32_t &onexit_end_637bec;
    const std::uint32_t &onexit_begin_637bf0;
};
struct SourceCrtExitMemory
{
    virtual ~SourceCrtExitMemory() = default;
    virtual std::uint32_t read_word(std::uint32_t address, std::uint32_t instruction) = 0;
    virtual void invoke_registered_callback(std::uint32_t address) = 0;
    virtual std::uint32_t flush_file_466564(std::uint32_t file) = 0;
    virtual void close_streams_46d13d() = 0;
};
// NEW host termination signal, caught only outside the source application frame.
// No std::exit, OS process termination, fabricated WM_QUIT or successful return to the menu.
struct SourceHostProcessExit final
{
    std::uint32_t status;
    bool terminate_process;
};
struct SourceCrtProcessBoundary
{
    virtual ~SourceCrtProcessBoundary() = default;
    virtual std::uintptr_t get_current_process() = 0;
    virtual void terminate_process(std::uintptr_t, std::uint32_t) = 0; // May return: preserve source fallthrough.
    virtual void exit_process(std::uint32_t) = 0;
};
class SourceHostUnwindBoundary final : public SourceCrtProcessBoundary
{
  public:
    std::uintptr_t get_current_process() final;
    void terminate_process(std::uintptr_t, std::uint32_t) final;
    void exit_process(std::uint32_t) final;
};
void execute_source_crt_exit_462e1c(SourceCrtExitWords, SourceCrtExitMemory &, SourceCrtProcessBoundary &,
                                    std::uint32_t status, std::uint32_t quick, std::uint32_t return_to_caller);
void execute_source_menu_exit_462dfa(FrontendState &, SourceCrtExitWords, SourceCrtExitMemory &,
                                     SourceCrtProcessBoundary &, SourceApplicationShutdownServices &,
                                     std::uint32_t status);
struct SourceHostApplicationResult
{
    bool process_exit;
    std::uint32_t process_status;
    bool forced;
    SourceApplicationEntryResult ordinary;
};
SourceHostApplicationResult run_source_application_host_boundary(SourceApplicationEntryWorld &,
                                                                 const SourceApplicationEntryArguments &,
                                                                 SourceApplicationEntryServices &,
                                                                 SourceApplicationMessages &);
} // namespace sgn::retail
