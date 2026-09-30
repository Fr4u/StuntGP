#pragma once
#include <cstddef>
#include <limits>
#include <span>

namespace sgn::audio_host
{
// NEW host layout helper, not a DirectSound DSP reconstruction. XAudio2 stores
// all source contributions for one destination contiguously: S + SrcCount*D.
// Gain is supplied by the existing caller; no pan formula or channel-mask
// interpretation belongs here. Invalid dimensions leave the matrix unchanged.
inline bool scale_output_matrix_destination(std::span<float> matrix, std::size_t source_count,
                                            std::size_t destination_count, std::size_t destination, float gain) noexcept
{
    if (source_count == 0 || destination_count == 0 || destination >= destination_count ||
        source_count > std::numeric_limits<std::size_t>::max() / destination_count ||
        matrix.size() != source_count * destination_count)
        return false;
    for (auto &coefficient : matrix.subspan(destination * source_count, source_count))
        coefficient *= gain;
    return true;
}
} // namespace sgn::audio_host
