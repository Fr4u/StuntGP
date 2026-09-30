#include "sgn/source_menu_crt_exit.hpp"
#include "sgn/race_session.hpp"
#include <bit>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
class Callbacks final : public SourceCrtExitMemory
{
    FrontendState &frontend_;
    SourceCrtExitWords words_;
    SourceCrtExitMemory &memory_;
    SourceApplicationShutdownServices &shutdown_;

  public:
    Callbacks(FrontendState &f, SourceCrtExitWords w, SourceCrtExitMemory &m, SourceApplicationShutdownServices &s)
        : frontend_(f), words_(w), memory_(m), shutdown_(s)
    {
    }
    W read_word(W a, W site) final
    {
        return memory_.read_word(a, site);
    }
    W flush_file_466564(W file) final
    {
        return memory_.flush_file_466564(file);
    }
    void close_streams_46d13d() final
    {
        memory_.close_streams_46d13d();
    }
    void invoke_registered_callback(W address) final
    {
        if (address == 0x442180)
        {
            shutdown_source_application_442180(frontend_, shutdown_);
            return;
        }
        if (address != 0x46b56d)
        {
            memory_.invoke_registered_callback(address);
            return;
        }
        // 46B56D ->4665FB ->466604(1). Live count/table/FILE flags, in source order.
        W index = 0;
        if (std::bit_cast<std::int32_t>(read_word(0x637aa0, 0x46660d)) > 0)
        {
            do
            {
                const auto table = read_word(0x636a88, 0x466615);
                const auto file = read_word(table + index * 4u, 0x46661a);
                if (file != 0 && (read_word(file + 0xcu, 0x466621) & 0x83u) != 0)
                    (void)flush_file_466564(file);
                ++index;
            } while (std::bit_cast<std::int32_t>(index) < std::bit_cast<std::int32_t>(read_word(0x637aa0, 0x46665a)));
        }
        // Flush count return is discarded by46B56D; callback rereads live byte afterwards.
        if (words_.return_to_caller_636570 != 0)
            close_streams_46d13d();
    }
};
void forward(SourceCrtExitMemory &memory, W first, W end)
{
    while (first < end)
    {
        const auto callback = memory.read_word(first, 0x462ec0);
        if (callback != 0)
            memory.invoke_registered_callback(callback);
        first += 4u;
    }
}
} // namespace
std::uintptr_t SourceHostUnwindBoundary::get_current_process()
{
    return ~std::uintptr_t{0};
}
void SourceHostUnwindBoundary::terminate_process(std::uintptr_t, W status)
{
    throw SourceHostProcessExit{status, true};
}
void SourceHostUnwindBoundary::exit_process(W status)
{
    throw SourceHostProcessExit{status, false};
}
void execute_source_crt_exit_462e1c(SourceCrtExitWords w, SourceCrtExitMemory &memory,
                                    SourceCrtProcessBoundary &process, W status, W quick, W return_to_caller)
{
    if (w.process_exit_636578 == 1)
    {
        const auto current = process.get_current_process();
        process.terminate_process(current, status);
    }
    w.in_exit_636574 = 1;
    w.return_to_caller_636570 = static_cast<std::uint8_t>(return_to_caller);
    if (quick == 0)
    {
        const auto begin = w.onexit_begin_637bf0;
        if (begin != 0)
        {
            auto cursor = w.onexit_end_637bec - 4u;
            if (cursor >= begin)
                do
                {
                    const auto callback = memory.read_word(cursor, 0x462e68);
                    if (callback != 0)
                        memory.invoke_registered_callback(callback);
                    cursor -= 4u;
                } while (cursor >= w.onexit_begin_637bf0);
        }
        forward(memory, 0x472018, 0x472020);
    }
    forward(memory, 0x472024, 0x472028);
    if (return_to_caller == 0)
    {
        w.process_exit_636578 = 1;
        process.exit_process(status);
    }
}
void execute_source_menu_exit_462dfa(FrontendState &f, SourceCrtExitWords words, SourceCrtExitMemory &memory,
                                     SourceCrtProcessBoundary &process, SourceApplicationShutdownServices &shutdown,
                                     W status)
{
    f.shared_platform()->require_owner_thread();
    Callbacks callbacks(f, words, memory, shutdown);
    execute_source_crt_exit_462e1c(words, callbacks, process, status, 0, 0);
    throw FormatError("source process termination returned to PageMain462DFA");
}
SourceHostApplicationResult run_source_application_host_boundary(SourceApplicationEntryWorld &world,
                                                                 const SourceApplicationEntryArguments &args,
                                                                 SourceApplicationEntryServices &services,
                                                                 SourceApplicationMessages &messages)
{
    try
    {
        return {false, 0, false, run_source_application_44ea60(world, args, services, messages)};
    }
    catch (const SourceHostProcessExit &exit)
    {
        return {true, exit.status, exit.terminate_process, {}};
    }
}
} // namespace sgn::retail
