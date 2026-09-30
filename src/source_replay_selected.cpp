#include "sgn/source_replay_selected.hpp"
#include "sgn/retail_frontend_results.hpp"
#include "sgn/source_replay_compress.hpp"
#include <bit>

namespace sgn::retail
{
std::uint32_t compress_selected_native_replay_44dc5b(FrontendSourceStorage &frontend, const ReplayBank &recordings,
                                                     std::uint32_t captured_native_identity, std::uint32_t capacity)
{
    // 43B64A..66F precedes the first source read. In particular, a null or
    // unmapped source must not change a capacity0 call into a host exception.
    if (std::bit_cast<std::int32_t>(capacity) < 0xac)
        return 0;
    const auto selected = frontend_replay_handle(captured_native_identity);
    // Null retains the original invalid-read boundary at the codec's first
    // source access. Do not turn a null recording into a successful empty save.
    const auto source =
        selected ? recordings.bytes().subspan(ReplayBank::base(*selected)) : std::span<const std::uint8_t>{};
    return compress_source_replay_to_frontend_43b640(frontend, capacity, source);
}
} // namespace sgn::retail
