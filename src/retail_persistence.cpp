#include "sgn/retail_persistence.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>

namespace sgn::retail
{
std::uint32_t persistence_word(std::span<const std::uint8_t> bytes, std::size_t offset)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::out_of_range("persistence word outside owner");
    return std::uint32_t(bytes[offset]) | (std::uint32_t(bytes[offset + 1]) << 8) |
           (std::uint32_t(bytes[offset + 2]) << 16) | (std::uint32_t(bytes[offset + 3]) << 24);
}
void persistence_word(std::span<std::uint8_t> bytes, std::size_t offset, std::uint32_t value)
{
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw std::out_of_range("persistence word outside owner");
    for (unsigned n = 0; n < 4; ++n)
        bytes[offset + n] = static_cast<std::uint8_t>(value >> (8 * n));
}
PersistenceHeader persistence_header(std::span<const std::uint8_t, persistence_size> bytes)
{
    return {persistence_word(bytes, 0), persistence_word(bytes, 4)};
}
std::span<std::uint8_t, 52> persistence_unlocks(PersistenceView state) noexcept
{
    return state.live.subspan<8, 52>();
}
std::span<std::uint8_t, 0x160> persistence_campaign_slot(PersistenceView state, unsigned slot)
{
    if (slot >= 8)
        throw std::out_of_range("campaign save slot outside original bank");
    return std::span<std::uint8_t, 0x160>{state.live.data() + 0xf1c + slot * 0x160, 0x160};
}
std::span<std::uint8_t, 32> persistence_campaign_records(PersistenceView state, unsigned track)
{
    if (track >= 25)
        throw std::out_of_range("campaign track record outside original bank");
    return std::span<std::uint8_t, 32>{state.live.data() + 0x1a1c + track * 32, 32};
}
std::uint32_t persistence_checksum(std::span<const std::uint8_t> bytes) noexcept
{
    std::uint32_t result{};
    for (const auto byte : bytes)
        result = std::rotl(result, 3) + byte;
    return result;
}
void copy_persistence_comparison(PersistenceView state) noexcept
{
    std::copy_n(state.live.begin() + 8, 52, state.live.begin() + 0x3c);
    std::copy(state.live.begin() + 8, state.live.end(), state.saved_body.begin());
}
bool persistence_changed(PersistenceView state) noexcept
{
    return !std::equal(state.live.begin() + 8, state.live.end(), state.saved_body.begin());
}
void clear_persistence_record_changes(PersistenceView state) noexcept
{
    constexpr std::size_t base = 0x5df560;
    for (std::size_t at = 0x5df683; at < 0x5df8e3; at += 32)
        for (const int delta : {-8, 0, 8, 16})
            state.live[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(at) + delta) - base] = 0;
    for (std::size_t at = 0x5df8db; at < 0x5dfbfb; at += 8)
        state.live[at - base] = 0;
    for (std::size_t at = 0x5e1363; at < 0x5e1683; at += 8)
        state.live[at - base] = 0;
    for (std::size_t at = 0x5dfbff; at < 0x5e0433; at += 0x54)
        for (unsigned n = 0; n < 10; ++n)
            state.live[at + 8 * n - base] = 0;
    for (std::size_t at = 0x5e0f8b; at < 0x5e12ab; at += 32)
        for (const int delta : {-8, 0, 8, 16})
            state.live[static_cast<std::size_t>(static_cast<std::ptrdiff_t>(at) + delta) - base] = 0;
    for (std::size_t at = 0x5e12c3; at < 0x5e1313; at += 8)
        state.live[at - base] = 0;
    for (std::size_t at = 0x5e1313; at < 0x5e1363; at += 8)
        state.live[at - base] = 0;
}
void acknowledge_persistence(PersistenceView state) noexcept
{
    clear_persistence_record_changes(state);
    copy_persistence_comparison(state);
}
void read_persistence_auxiliary(PersistenceView state, PersistenceServices &service)
{
    if (const auto file = service.open({PersistenceFileKind::detail, 0}, PersistenceAccess::read_text))
    {
        auto value = std::bit_cast<std::int32_t>(persistence_word(state.live, 0x84));
        service.scan_decimal(file, value);
        value = std::clamp(value, 0, 100);
        value -= value % 10;
        persistence_word(state.live, 0x84, std::bit_cast<std::uint32_t>(value));
        service.close(file);
    }
    if (const auto file = service.open({PersistenceFileKind::language, 0}, PersistenceAccess::read_text))
    {
        auto value = std::bit_cast<std::int32_t>(persistence_word(state.live, 0x90));
        service.scan_decimal(file, value);
        if (value < 1 || value > 8)
            value = 0;
        persistence_word(state.live, 0x90, std::bit_cast<std::uint32_t>(value));
        service.close(file);
    }
}
void write_persistence_auxiliary(PersistenceView state, PersistenceServices &service)
{
    if (const auto file = service.open({PersistenceFileKind::detail, 0}, PersistenceAccess::write_text))
    {
        service.print_decimal(file, std::bit_cast<std::int32_t>(persistence_word(state.live, 0x84)));
        service.close(file);
    }
    if (const auto file = service.open({PersistenceFileKind::language, 0}, PersistenceAccess::write_text))
    {
        service.print_decimal(file, std::bit_cast<std::int32_t>(persistence_word(state.live, 0x90)));
        service.close(file);
    }
}
PersistenceStatus read_persistence_setup(PersistenceView state, PersistenceServices &service, std::uint32_t card)
{
    if (!service.card_directory_exists(card))
        return PersistenceStatus::no_card;
    const PersistenceFile name{PersistenceFileKind::setup, card};
    if (service.file_size(name) != static_cast<std::int32_t>(persistence_size))
        return PersistenceStatus::invalid_data;
    const auto file = service.open(name, PersistenceAccess::read_binary);
    if (!file)
        return PersistenceStatus::open_failed;
    const auto transferred = service.read(file, state.scratch);
    service.close(file);
    return transferred == persistence_size ? PersistenceStatus::success : PersistenceStatus::invalid_data;
}
PersistenceStatus write_persistence_setup(PersistenceView state, PersistenceServices &service, std::uint32_t card)
{
    if (!service.card_directory_exists(card))
        return PersistenceStatus::no_card;
    const PersistenceFile name{PersistenceFileKind::setup, card};
    const auto size = service.file_size(name);
    if (size >= 0 && size != static_cast<std::int32_t>(persistence_size))
        return PersistenceStatus::invalid_data;
    const auto file = service.open(name, PersistenceAccess::write_binary);
    if (!file)
        return PersistenceStatus::open_failed;
    const auto transferred = service.write(file, state.live);
    service.close(file);
    return transferred == persistence_size ? PersistenceStatus::success : PersistenceStatus::invalid_data;
}
PersistenceStatus save_persistence(PersistenceView state, PersistenceServices &service, std::uint32_t card)
{
    persistence_word(state.live, 0, static_cast<std::uint32_t>(persistence_size));
    persistence_word(state.live, 4, 0);
    persistence_word(state.live, 4, persistence_checksum(state.live));
    const auto result = write_persistence_setup(state, service, card);
    write_persistence_auxiliary(state, service);
    if (result == PersistenceStatus::success)
        acknowledge_persistence(state);
    return result;
}
PersistenceStatus load_persistence(PersistenceView state, PersistenceServices &service, std::uint32_t card)
{
    const auto result = read_persistence_setup(state, service, card);
    if (result != PersistenceStatus::success)
        return result;
    const auto header = persistence_header(state.scratch);
    if (header.size != persistence_size)
        return PersistenceStatus::invalid_data;
    persistence_word(state.scratch, 4, 0);
    if (header.checksum != persistence_checksum(state.scratch))
        return PersistenceStatus::invalid_data;
    std::copy(state.scratch.begin(), state.scratch.end(), state.live.begin());
    read_persistence_auxiliary(state, service);
    acknowledge_persistence(state);
    return PersistenceStatus::success;
}
bool scan_persistence_decimal(std::span<const std::uint8_t> bytes, std::int32_t &value) noexcept
{
    std::size_t at{};
    while (at < bytes.size() && (bytes[at] == ' ' || bytes[at] == '\t' || bytes[at] == '\r' || bytes[at] == '\n' ||
                                 bytes[at] == '\v' || bytes[at] == '\f'))
        ++at;
    bool negative{};
    if (at < bytes.size() && (bytes[at] == '+' || bytes[at] == '-'))
        negative = bytes[at++] == '-';
    const auto start = at;
    std::uint32_t result{};
    while (at < bytes.size() && bytes[at] >= '0' && bytes[at] <= '9')
        result = result * 10u + std::uint32_t(bytes[at++] - '0');
    if (at == start)
        return false;
    if (negative)
        result = 0u - result;
    value = std::bit_cast<std::int32_t>(result);
    return true;
}
std::string format_persistence_decimal(std::int32_t value)
{
    return std::to_string(value) + "\r\n";
}
} // namespace sgn::retail
