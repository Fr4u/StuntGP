#include "sgn/source_audio_enumeration_native.hpp"
#include <bit>
#include <cstring>
#include <exception>
#include <type_traits>

namespace sgn::retail
{
namespace
{
static_assert(sizeof(GUID) == 16 && sizeof(HRESULT) == 4 && sizeof(AudioResult) == 4);
static_assert(std::endian::native == std::endian::little);
struct CallbackContext
{
    AudioEnumerationReceiver &receiver;
    std::exception_ptr error;
    bool stopped{};
};
BOOL CALLBACK receive_ansi(LPGUID guid, LPCSTR description, LPCSTR module, LPVOID opaque) noexcept
{
    // DirectSound returns the exact supplied context synchronously. No borrowed
    // SDK pointer survives this callback; the source callback ignores module.
    (void)module;
    auto &context = *static_cast<CallbackContext *>(opaque);
    if (context.stopped)
        return FALSE;
    try
    {
        std::optional<std::array<std::uint32_t, 4>> words;
        if (guid)
        {
            words.emplace();
            std::memcpy(words->data(), guid, sizeof(GUID));
        }
        const bool more = context.receiver.visit_ansi(words, description);
        if (!more)
            context.stopped = true;
        return more ? TRUE : FALSE;
    }
    catch (...)
    {
        context.error = std::current_exception();
        context.stopped = true;
        return FALSE;
    }
}
static_assert(std::is_same_v<decltype(static_cast<LPDSENUMCALLBACKA>(&receive_ansi)), LPDSENUMCALLBACKA>);
} // namespace
Win32SourceAudioEnumeration::Win32SourceAudioEnumeration(DirectSoundEnumerateAEntry entry) : entry_(entry)
{
    if (!entry_)
        throw SourceAudioEnumerationError(0x46210eu, "46210E missing DirectSoundEnumerateA entry");
}
AudioResult Win32SourceAudioEnumeration::enumerate(AudioEnumerationReceiver &receiver)
{
    CallbackContext context{receiver, {}, false};
    const auto result = entry_(&receive_ansi, &context);
    // A callback exception must not escape through the Win32 ABI or turn into
    // S_OK merely because FALSE stopped enumeration. Re-throw after API return.
    if (context.error)
        std::rethrow_exception(context.error);
    return static_cast<AudioResult>(result);
}
} // namespace sgn::retail
