#pragma once
#include "sgn/source_world_span.hpp"
#include "sgn/source_transport_word.hpp"
#include "sgn/retail_screen_polygons.hpp"
#include <functional>
namespace sgn::retail
{
// Keeps the existing owner alive, preserving allocation generation and byte
// extent at each reached access. Constructing does not inspect payload.
class SourceTransportBuffer
{
    SourceWorldBufferBinding binding_;
    std::shared_ptr<RaceSessionPlatform> owner_;
    std::uint32_t extent(std::uint32_t, std::uint32_t, std::uint32_t) const;

  public:
    explicit SourceTransportBuffer(SourceWorldBufferBinding);
    SourceTransportWord load(std::uint32_t, std::uint32_t) const;
    void store(std::uint32_t, const SourceTransportWord &, std::uint32_t);
    void known(std::uint32_t, std::span<const std::uint8_t>, std::uint32_t);
    std::uint32_t number(std::uint32_t at, std::uint32_t site) const
    {
        return load(at, site).numeric(site);
    }
    std::span<std::uint8_t> writable_known(std::uint32_t, std::uint32_t, std::uint32_t);
    std::span<const std::uint8_t> read(std::uint32_t, std::uint32_t, std::uint32_t) const;
};
using SourceScreenWordRead = std::function<SourceTransportWord(std::uint32_t, std::uint32_t)>;
bool append_source_screen_words_425830(SourceScreenPolygonQueue, const SourceScreenWordRead &, const ViewportState &);
bool append_source_screen_captured_425838(SourceWorldBufferBinding, std::uint32_t captured, std::uint32_t &live_count,
                                          const SourceScreenWordRead &, const ViewportState &);
} // namespace sgn::retail
