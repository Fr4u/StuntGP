#include "sgn/retail_campaign_data.hpp"
#include <algorithm>
#include <array>
#include <limits>
#include <stdexcept>

namespace sgn::retail
{
namespace
{
bool horizontal(char c) noexcept
{
    return c == ' ' || c == '\t';
}
bool newline(char c) noexcept
{
    return c == '\r' || c == '\n';
}
bool decimal_space(char c) noexcept
{
    return horizontal(c) || newline(c) || c == '\v' || c == '\f';
}
bool overlap(const void *a, std::size_t an, const void *b, std::size_t bn) noexcept
{
    if (an == 0 || bn == 0)
        return false;
    const auto x = reinterpret_cast<std::uintptr_t>(a);
    const auto y = reinterpret_cast<std::uintptr_t>(b);
    return x <= y ? y - x < an : x - y < bn;
}
struct Parser
{
    CampaignDataViews views;
    std::string_view input;
    std::uint32_t &cursor;
    bool publish;

    char current() const noexcept
    {
        return cursor == input.size() ? '\0' : input[cursor];
    }

    // 43C680. Unterminated quotes return without adding NUL or trimming.
    // Caller 43C770 nevertheless scans the retained numeric buffer.
    void token(std::span<std::byte> output)
    {
        output[0] = std::byte{0};
        while (horizontal(current()))
            ++cursor;
        if (current() == '\0')
            return;
        std::size_t length = 0;
        auto append = [&] {
            if (length == output.size())
                throw std::invalid_argument("campaign token exceeds its host buffer");
            output[length++] = static_cast<std::byte>(static_cast<unsigned char>(current()));
            ++cursor;
        };
        if (current() == '"')
        {
            ++cursor;
            while (current() != '"' && current() != '\0')
                append();
            if (current() != '"')
                return;
            ++cursor;
        }
        else
        {
            while (current() != ',' && !newline(current()) && current() != '\0')
                append();
        }
        if (length == output.size())
            throw std::invalid_argument("campaign token has no room for its terminator");
        output[length] = std::byte{0};
        while (length > 0 && output[length - 1] == std::byte{' '})
            output[--length] = std::byte{0};
        if (current() == ',')
            ++cursor;
    }

    // 43C5C0 followed by 43C520, including CR-only and CRCRLF lines.
    void next_line() noexcept
    {
        while (current() != '\0' && !newline(current()))
            ++cursor;
        while (horizontal(current()) || newline(current()))
            ++cursor;
    }

    void number(std::size_t offset)
    {
        token(views.numeric_token);
        const auto end = std::find(views.numeric_token.begin(), views.numeric_token.end(), std::byte{0});
        if (end == views.numeric_token.end())
            throw std::invalid_argument("campaign numeric scratch has no bounded terminator");
        const auto length = static_cast<std::size_t>(end - views.numeric_token.begin());
        auto character = [&](std::size_t at) {
            return static_cast<char>(std::to_integer<unsigned char>(views.numeric_token[at]));
        };
        std::size_t at = 0;
        while (at < length && decimal_space(character(at)))
            ++at;
        bool negative = false;
        if (at < length && (character(at) == '+' || character(at) == '-'))
            negative = character(at++) == '-';
        const auto first = at;
        std::uint32_t value = 0;
        while (at < length && character(at) >= '0' && character(at) <= '9')
        {
            value = value * 10u + static_cast<std::uint32_t>(character(at++) - '0');
        }
        if (at == first)
            return; // sscanf conversion failure leaves destination untouched.
        if (negative)
            value = 0u - value;
        if (publish)
        {
            for (unsigned byte = 0; byte < 4; ++byte)
                views.rules[offset + byte] = static_cast<std::byte>((value >> (byte * 8u)) & 255u);
        }
    }

    void row(std::size_t offset, std::size_t words)
    {
        // 43C600's comparison result is ignored by every call in 435B60.
        std::array<std::byte, 64> label{};
        token(label);
        for (std::size_t word = 0; word < words; ++word)
            number(offset + word * 4);
        next_line();
    }

    void run()
    {
        row(0, 1); // 435B6D..435B8B
        for (std::size_t i = 0; i < 4; ++i)
            row(4 + i * 4, 1);
        for (std::size_t i = 0; i < 6; ++i)
            row(0x14 + i * 4, 1);
        row(0x2C, 1);
        row(0x30, 1);
        next_line(); // 435C68..435C6C: sponsor header
        for (std::size_t i = 0; i < 6; ++i)
            row(0x3C + i * 8, 2);
        next_line(); // 435CCD..435CD1: award header
        for (std::size_t i = 0; i < 6; ++i)
            row(0x6C + i * 0x10, 4);
        next_line(); // 435D40..435D44: track header
        for (std::size_t i = 0; i < 21; ++i)
            row(0xCC + i * 0x44, 2);
        next_line(); // 435D9D..435DA1: rival header
        for (std::size_t round = 0; round < 21; ++round)
        {
            for (std::size_t car = 0; car < 5; ++car)
                row(0xD4 + round * 0x44 + car * 12, 3);
        }
    }
};
} // namespace

std::uint32_t parse_campaign_data_435b60(CampaignDataViews views, std::string_view input, std::uint32_t &cursor)
{
    if (input.size() > std::numeric_limits<std::uint32_t>::max() || cursor > input.size())
    {
        throw std::invalid_argument("campaign cursor or input exceeds its bounded 32-bit range");
    }
    if (overlap(views.rules.data(), views.rules.size(), views.numeric_token.data(), views.numeric_token.size()) ||
        overlap(input.data(), input.size(), views.rules.data(), views.rules.size()) ||
        overlap(input.data(), input.size(), views.numeric_token.data(), views.numeric_token.size()) ||
        overlap(&cursor, sizeof(cursor), views.rules.data(), views.rules.size()) ||
        overlap(&cursor, sizeof(cursor), views.numeric_token.data(), views.numeric_token.size()) ||
        overlap(&cursor, sizeof(cursor), input.data(), input.size()))
    {
        throw std::invalid_argument("campaign source owners, input and cursor must not overlap");
    }
    // New host preflight: no duplicate rules owner and no published partial
    // mutation on rejection. Only the 256-byte temporary token state is copied.
    std::array<std::byte, campaign_numeric_token_size> checked_token{};
    std::copy(views.numeric_token.begin(), views.numeric_token.end(), checked_token.begin());
    auto checked_cursor = cursor;
    Parser{{views.rules, checked_token}, input, checked_cursor, false}.run();
    Parser{views, input, cursor, true}.run();
    return 1; // 435E26..435E2F
}
} // namespace sgn::retail
