#include "sgn/source_music_wave_open.hpp"
#include <memory>

namespace sgn::retail
{
void SourceMusicWaveBytes::store(std::size_t offset, std::uint8_t value)
{
    if (offset >= bytes_.size())
        throw SourceMusicWaveUnknown("45FFD0 write outside low16 allocation");
    bytes_[offset] = value;
}
std::uint32_t SourceMusicWaveBytes::load(std::size_t offset, std::size_t width) const
{
    if (width > 4 || offset > bytes_.size() || width > bytes_.size() - offset)
        throw SourceMusicWaveUnknown("45FFD0 read outside recovered allocation");
    std::uint32_t value = 0;
    for (std::size_t i = 0; i < width; ++i)
    {
        if (!bytes_[offset + i])
            throw SourceMusicWaveUnknown("45FFD0 unwritten heap byte");
        value |= std::uint32_t(*bytes_[offset + i]) << (i * 8);
    }
    return value;
}
std::optional<std::uint32_t> SourceMusicWaveChunkAccess::read(std::size_t field) const
{
    if (field >= 5)
        throw std::out_of_range("MMCKINFO field");
    if (!stream_)
        return local_[field];
    switch (field)
    {
    case 0:
        return stream_->chunk_id;
    case 1:
        return stream_->chunk_size;
    case 2:
        return stream_->chunk_type;
    case 3:
        return stream_->data_offset;
    default:
        return stream_->chunk_flags;
    }
}
std::uint32_t SourceMusicWaveChunkAccess::require(std::size_t field) const
{
    const auto value = read(field);
    if (!value)
        throw SourceMusicWaveUnknown("45FFD0 unwritten local MMCKINFO field");
    return *value;
}
void SourceMusicWaveChunkAccess::store(std::size_t field, std::uint32_t value)
{
    if (field >= 5)
        throw std::out_of_range("MMCKINFO field");
    if (!stream_)
    {
        local_[field] = value;
        return;
    }
    switch (field)
    {
    case 0:
        stream_->chunk_id = value;
        break;
    case 1:
        stream_->chunk_size = value;
        break;
    case 2:
        stream_->chunk_type = value;
        break;
    case 3:
        stream_->data_offset = value;
        break;
    default:
        stream_->chunk_flags = value;
        break;
    }
}
std::uint32_t source_music_wave_open_45ffd0(AudioStreamState &s, SourceMusicWaveIO &io)
{
    // 45FFD9..45FFFA: REP STOSD 18, cchBuffer at +14, MMIO_ALLOCBUF.
    std::array<std::uint32_t, 18> info{};
    info[5] = 0x400;
    s.file = io.open(s, info, 0x10000);
    if (!s.file.value)
    {
        s.result = 0x80070057;
        return s.result;
    }
    const auto fail = [&]() -> std::uint32_t {
        (void)io.close(s.file, 0);
        s.file = {};
        s.result = 0x88780064;
        return s.result;
    };
    SourceMusicWaveChunkAccess riff(s);
    if (io.descend(s.file, riff, nullptr, 0) != 0)
        return fail();
    if (riff.require(0) != 0x46464952 || riff.require(2) != 0x45564157)
        return fail();
    SourceMusicWaveChunkAccess local; // EBP-14, only ckid is initialized.
    local.store(0, 0x20746d66);
    if (io.descend(s.file, local, &riff, 0x10) != 0)
        return fail();
    const auto full_size = local.require(1);
    // 46008B..4600A6: EDI retains full cksize; allocation uses EAX & FFFF.
    // Bounds/unknown stops preserve the distinction from retail return paths.
    auto allocation = std::make_unique<SourceMusicWaveBytes>(full_size & 0xffffu);
    if (io.read(s.file, *allocation, full_size) != full_size)
    {
        allocation.reset(); // 4600B1 before close; global handle remains stale.
        (void)io.close(s.file, 0);
        s.result = 0x88780064;
        return s.result;
    }
    // Five individual loads/stores, not an all-or-nothing format replacement.
    // 4600DB,4600E3,4600EB,4600F4,4600FC: final load is always two bytes.
    auto word = allocation->load(0, 4);
    s.format.tag = static_cast<std::uint16_t>(word);
    s.format.channels = static_cast<std::uint16_t>(word >> 16);
    s.format.samples_per_second = allocation->load(4, 4);
    s.format.bytes_per_second = allocation->load(8, 4);
    word = allocation->load(12, 4);
    s.format.block_align = static_cast<std::uint16_t>(word);
    s.format.bits_per_sample = static_cast<std::uint16_t>(word >> 16);
    // NEW repair (PORT-FIX-015): every shipped music WAV has a 16-byte PCM fmt chunk, so 4600A6 allocates 16
    // bytes and 4600FC reads the WORD at +16/+17 from heap memory nothing wrote (the 46786C block tail). That
    // WORD only becomes WAVEFORMATEX.cbSize of the 45FF57 music buffer, which DirectSound ignores for PCM. NEW
    // defines exactly those two never-written bytes as 0 (the cbSize of a plain PCM format) instead of stopping
    // the game. A fmt chunk of 18 or more bytes keeps the source load unchanged.
    if (allocation->size() == 16)
        s.format.extra_bytes = 0;
    else
        s.format.extra_bytes = static_cast<std::uint16_t>(allocation->load(16, 2));
    allocation.reset(); // 460107; before PCM tag validation.
    if (s.format.tag != 1)
        return fail();
    if (io.ascend(s.file, local, 0) != 0)
        return fail();
    if (io.seek(s.file, s.data_offset + 4u, 0) == 0xffffffffu)
        return fail();
    local.store(0, 0x61746164); // same frame and remaining fields as fmt/ascend
    if (io.descend(s.file, local, &riff, 0x10) != 0)
        return fail();
    for (std::size_t i = 0; i < 5; ++i)
        riff.store(i, local.require(i)); // 4601C2
    s.field_df30 = 0;
    s.result = 0;
    return 0;
}
} // namespace sgn::retail
