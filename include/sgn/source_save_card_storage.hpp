#pragma once
#include <array>
#include <cstdint>
namespace sgn::retail
{
struct SourceSaveCardStorage
{
    std::uint32_t selected_card_4798bc{0xffffffffu}; // pinnedPE initial value
    std::array<char, 1024> path_5ff4bc{};
    std::uint32_t changed_5ff8bc{};
    // Actual mutable PE caller literals, including original alignment bytes.
    std::array<std::uint8_t, 8> file_path_format_479a50{0x25, 0x73, 0x5c, 0x25, 0x73, 0, 0, 0};
    std::array<std::uint8_t, 4> read_binary_mode_479a58{0x72, 0x62, 0, 0};
    std::array<std::uint8_t, 16> ghost_name_format_479a88{0x67, 0x68, 0x6f, 0x73, 0x74, 0x25, 0x30, 0x32,
                                                          0x64, 0x2e, 0x62, 0x69, 0x6e, 0,    0,    0};
    // Separate actual write-path literals; never alias equal read-path bytes.
    std::array<std::uint8_t, 8> write_path_format_479a68{0x25, 0x73, 0x5c, 0x25, 0x73, 0, 0, 0};
    std::array<std::uint8_t, 4> write_binary_mode_479a70{0x77, 0x62, 0, 0};
    std::array<std::uint8_t, 16> write_ghost_format_479a98{0x67, 0x68, 0x6f, 0x73, 0x74, 0x25, 0x30, 0x32,
                                                           0x64, 0x2e, 0x62, 0x69, 0x6e, 0,    0,    0};
    // Sole mutable PE479A74..479A87, including padding. 4405F8 passes this owner.
    std::array<std::uint8_t, 20> replay_path_format_479a74{
        0x25, 0x73, 0x5c, 0x67, 0x68, 0x6f, 0x73, 0x74, 0x25, 0x30,
        0x32, 0x64, 0x2e, 0x62, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00,
    };
};
} // namespace sgn::retail
