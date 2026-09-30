#include "sgn/source_car_key_formatter.hpp"
#include "sgn/source_script_path_formatter.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
namespace
{
std::uint32_t car_format_return_site(std::uint32_t format)
{
    // NEW source-call metadata. These are six immediate PUSH operands, not
    // a hardcoded output key table or a restriction on mutable format contents.
    switch (format)
    {
    case 0x47782c:
        return 0x42c9e7;
    case 0x47783c:
        return 0x42ca57;
    case 0x47784c:
        return 0x42cac7;
    case 0x47785c:
        return 0x42cb37;
    case 0x47786c:
        return 0x42cbb0;
    case 0x47787c:
        return 0x42cbe3;
    default:
        throw std::invalid_argument("car key adapter requires an actual reconstructed callsite format address");
    }
}
} // namespace
std::uint32_t SourceCarKeyFormatterImpl::classify_number_4631f4(RaceSessionPlatform &platform, std::uint8_t character,
                                                                std::uint32_t mask)
{
    platform.require_owner_thread();
    return platform.source_application_literals().classify_number_4631f4(character, mask);
}
void SourceCarKeyFormatterImpl::format_car_key_46280c(RaceSessionPlatform &platform, std::uint32_t format_address,
                                                      std::uint32_t car_number, SourceCarKeyCall &output)
{
    platform.require_owner_thread();
    const auto return_site = car_format_return_site(format_address);
    SourceScriptPathCall frame; // New CRT invocation; the caller retains its same64 bytes.
    const auto app = platform.source_application_literals();
    auto destination =
        std::span<std::uint8_t, 64>(reinterpret_cast<std::uint8_t *>(output.bytes.data()), output.bytes.size());
    try
    {
        (void)format_integer_key_46280c(platform.state().boot_config, app, format_address, car_number, destination,
                                        frame, return_site);
    }
    catch (...)
    {
        // Host observation only. Actual local writes are already visible and
        // remain so on the reached read/write/dispatch failure; no rollback.
        output.initialized_prefix = static_cast<std::uint32_t>(frame.initialized_prefix);
        throw;
    }
    output.initialized_prefix = static_cast<std::uint32_t>(frame.initialized_prefix);
}
} // namespace sgn::retail
