#pragma once
#include "sgn/retail_frontend_initialization.hpp"
#include <cstddef>
#include <span>

namespace sgn::retail
{
struct WorldMaterialPreludeEntry;
inline constexpr std::size_t frontend_descriptor_stride = 0x30;
inline constexpr std::size_t frontend_descriptor_count = 387;
inline constexpr std::size_t frontend_descriptor_bytes = 0x4890;
using FrontendDescriptor = std::span<std::uint8_t, frontend_descriptor_stride>;
using FrontendDescriptorBank = std::span<std::uint8_t, frontend_descriptor_bytes>;
FrontendDescriptorBank frontend_descriptor_bank(FrontendSourceStorage &);
std::uint32_t frontend_descriptor_absolute_43d9b0(std::uint32_t) noexcept;
// Full441380: IDs1..386 return1; all other32-bit IDs return0 without writes.
// Byte views also support the unaligned destinations accepted by source x86.
std::uint32_t initialize_frontend_descriptor_441380(FrontendDescriptor, std::uint32_t id);
void initialize_frontend_descriptors_441340(FrontendDescriptorBank); // full bank clear plus386 actual child calls
struct FrontendMaterialInitializationBoundary
{
    FrontendInitializationStop stop;
    std::size_t prelude_entries; // new host diagnostic, not a source return value
};
// 441190..4411AC only. Borrow the existing material list; never create/upload
// textures here. The source tail4411C0 is intentionally outside this packet.
FrontendMaterialInitializationBoundary initialize_frontend_material_prefix_441190(
    FrontendSourceStorage &, std::span<const WorldMaterialPreludeEntry>);
} // namespace sgn::retail
