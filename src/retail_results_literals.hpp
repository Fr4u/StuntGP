#pragma once
#include "sgn/assets.hpp"
#include <array>
#include <bit>
#include <cstdint>
namespace sgn::retail::results_source {
// Generated only from pinned PE literals; no modeled values.
inline double scalar(std::uint32_t at){switch(at){
case 0x46e1d8:return double(std::bit_cast<float>(std::uint32_t{0x3f000000}));
case 0x46e20c:return double(std::bit_cast<float>(std::uint32_t{0x40800000}));
case 0x46e294:return double(std::bit_cast<float>(std::uint32_t{0x3c23d70a}));
case 0x46e298:return double(std::bit_cast<float>(std::uint32_t{0x40a00000}));
case 0x46e2f0:return double(std::bit_cast<float>(std::uint32_t{0x42c80000}));
case 0x46e438:return double(std::bit_cast<float>(std::uint32_t{0x424ccccd}));
case 0x46e43c:return double(std::bit_cast<float>(std::uint32_t{0x41400000}));
case 0x46e49c:return double(std::bit_cast<float>(std::uint32_t{0x41800000}));
case 0x46e4cc:return double(std::bit_cast<float>(std::uint32_t{0xbf000000}));
case 0x46e568:return double(std::bit_cast<float>(std::uint32_t{0x42100000}));
case 0x46ead8:return double(std::bit_cast<float>(std::uint32_t{0x41900000}));
case 0x46eb74:return double(std::bit_cast<float>(std::uint32_t{0x4299999a}));
case 0x46eb78:return double(std::bit_cast<float>(std::uint32_t{0x4199999a}));
case 0x46eb7c:return double(std::bit_cast<float>(std::uint32_t{0x44026667}));
default:throw FormatError("unmapped result source literal");}}
inline constexpr std::array<std::array<std::uint32_t,6>,4> box_uv{{
    {{0x43dc0000u,0x440f0000u,0x00000000u,0x42900000u,0x43b60000u,0x43da0000u}},
    {{0x43dc0000u,0x440f0000u,0x43da0000u,0x43fe0000u,0x44480000u,0x445a0000u}},
    {{0x00000000u,0x437c0000u,0x00000000u,0x42900000u,0x446d0000u,0x447f0000u}},
    {{0x00000000u,0x437c0000u,0x447f0000u,0x446d0000u,0x42900000u,0x00000000u}},
}};
}
