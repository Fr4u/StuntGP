//Pinned PE float constants and HUD atlas bytes, source addresses retained.
#pragma once
#include <array>
#include <bit>
namespace sgn::retail::hud_source {
constexpr float scalar(unsigned address){switch(address){
case 0x46e1d8:return std::bit_cast<float>(0x3f000000u);
case 0x46e1ec:return std::bit_cast<float>(0x00000000u);
case 0x46e1f8:return std::bit_cast<float>(0x41000000u);
case 0x46e200:return std::bit_cast<float>(0x40000000u);
case 0x46e20c:return std::bit_cast<float>(0x40800000u);
case 0x46e220:return std::bit_cast<float>(0x41200000u);
case 0x46e230:return std::bit_cast<float>(0x3dcccccdu);
case 0x46e264:return std::bit_cast<float>(0x3f800000u);
case 0x46e294:return std::bit_cast<float>(0x3c23d70au);
case 0x46e2e0:return std::bit_cast<float>(0x43800000u);
case 0x46e2f0:return std::bit_cast<float>(0x42c80000u);
case 0x46e31c:return std::bit_cast<float>(0x3f4ccccdu);
case 0x46e378:return std::bit_cast<float>(0x43480000u);
case 0x46e3ec:return std::bit_cast<float>(0x43000000u);
case 0x46e460:return std::bit_cast<float>(0x40400000u);
case 0x46e46c:return std::bit_cast<float>(0x42a00000u);
case 0x46e49c:return std::bit_cast<float>(0x41800000u);
case 0x46e520:return std::bit_cast<float>(0x4b18967fu);
case 0x46e528:return std::bit_cast<float>(0x40cccccdu);
case 0x46e580:return std::bit_cast<float>(0x3fc00000u);
case 0x46e5c0:return std::bit_cast<float>(0x43200000u);
case 0x46e5fc:return std::bit_cast<float>(0x42000000u);
case 0x46e624:return std::bit_cast<float>(0x42c00000u);
case 0x46e6d0:return std::bit_cast<float>(0xc2c80000u);
case 0x46e6e4:return std::bit_cast<float>(0x3b800000u);
case 0x46e754:return std::bit_cast<float>(0x43f00000u);
case 0x46e75c:return std::bit_cast<float>(0x44800000u);
case 0x46e76c:return std::bit_cast<float>(0x42200000u);
case 0x46ead4:return std::bit_cast<float>(0x42820000u);
case 0x46ead8:return std::bit_cast<float>(0x41900000u);
case 0x46eaf4:return std::bit_cast<float>(0x42300000u);
case 0x46eaf8:return std::bit_cast<float>(0x41ce6667u);
case 0x46eafc:return std::bit_cast<float>(0x42e7999au);
case 0x46eb00:return std::bit_cast<float>(0x4250cccdu);
case 0x46eb04:return std::bit_cast<float>(0x410ccccdu);
case 0x46eb08:return std::bit_cast<float>(0x3fcccccdu);
case 0x46eb0c:return std::bit_cast<float>(0x4301999au);
case 0x46eb10:return std::bit_cast<float>(0x4179999au);
case 0x46eb14:return std::bit_cast<float>(0x42900000u);
case 0x46eb18:return std::bit_cast<float>(0x42500000u);
case 0x46eb1c:return std::bit_cast<float>(0x44340000u);
case 0x46eb20:return std::bit_cast<float>(0x42bb3333u);
case 0x46eb24:return std::bit_cast<float>(0x4019999au);
case 0x46eb28:return std::bit_cast<float>(0x40666667u);
case 0x46eb2c:return std::bit_cast<float>(0x46900000u);
case 0x46eb30:return std::bit_cast<float>(0x4249999au);
case 0x46eb34:return std::bit_cast<float>(0x42466667u);
case 0x46eb38:return std::bit_cast<float>(0x4239999au);
case 0x46eb3c:return std::bit_cast<float>(0x42533333u);
case 0x46eb40:return std::bit_cast<float>(0x42a4cccdu);
case 0x46eb44:return std::bit_cast<float>(0x42333333u);
case 0x46eb48:return std::bit_cast<float>(0x41600000u);
case 0x46eb4c:return std::bit_cast<float>(0x42bccccdu);
case 0x46eb50:return std::bit_cast<float>(0x43020000u);
case 0x46eb54:return std::bit_cast<float>(0x435c0000u);
case 0x46eb58:return std::bit_cast<float>(0x4289999au);
case 0x46eb5c:return std::bit_cast<float>(0x429a0000u);
case 0x46eb60:return std::bit_cast<float>(0x422c0000u);
case 0x46eb64:return std::bit_cast<float>(0x440f8000u);
case 0x46eb68:return std::bit_cast<float>(0x42840000u);
case 0x46eb6c:return std::bit_cast<float>(0x40aaaaabu);
case 0x46eb70:return std::bit_cast<float>(0x42acccccu);
case 0x46eb80:return std::bit_cast<float>(0x42cccccdu);
case 0x46eb84:return std::bit_cast<float>(0x4286cccdu);
case 0x46eb88:return std::bit_cast<float>(0x42180000u);
case 0x46eb8c:return std::bit_cast<float>(0x42f40000u);
case 0x46eb90:return std::bit_cast<float>(0x3f8f5c29u);
case 0x46eb94:return std::bit_cast<float>(0xc3000000u);
case 0x46eb98:return std::bit_cast<float>(0x400a3d70u);
default:throw FormatError("unknown HUD source constant");}}
inline constexpr std::array<std::uint32_t,84> digits={{ //004720D8
    0x0000000cu,0x40800000u,0x40800000u,0x43400000u,0x43800000u,0x423c0000u,0x427c0000u,0x0000000cu,
    0x40800000u,0x43820000u,0x43400000u,0x44000000u,0x423c0000u,0x427c0000u,0x0000000cu,0x40800000u,
    0x44010000u,0x43400000u,0x44400000u,0x423c0000u,0x427c0000u,0x0000000cu,0x40800000u,0x44410000u,
    0x43400000u,0x44800000u,0x423c0000u,0x427c0000u,0x0000000cu,0x43440000u,0x40800000u,0x43c00000u,
    0x43800000u,0x423c0000u,0x427c0000u,0x0000000cu,0x43440000u,0x43820000u,0x43c00000u,0x44000000u,
    0x423c0000u,0x427c0000u,0x0000000cu,0x43440000u,0x44010000u,0x43c00000u,0x44400000u,0x423c0000u,
    0x427c0000u,0x0000000cu,0x43440000u,0x44410000u,0x43c00000u,0x44800000u,0x423c0000u,0x427c0000u,
    0x0000000cu,0x43c20000u,0x40800000u,0x44100000u,0x43800000u,0x423c0000u,0x427c0000u,0x0000000cu,
    0x43c20000u,0x43820000u,0x44100000u,0x44000000u,0x423c0000u,0x427c0000u,0x0000000cu,0x43c20000u,
    0x44010000u,0x44100000u,0x44400000u,0x423c0000u,0x427c0000u,0x00006425u,0x3f800000u,0x00000000u,
    0x00000000u,0x00000000u,0x00000000u,0x3f800000u,
}};
inline constexpr std::array<std::uint32_t,54> gear={{ //004852C8
    0x00000000u,0x00000059u,0x00000000u,0x00000000u,0x00000026u,0x00000022u,0x00000000u,0x0000007cu,
    0x00000000u,0x00000000u,0x00000026u,0x00000021u,0x00000026u,0x00000059u,0x00000000u,0x00000000u,
    0x00000026u,0x00000022u,0x0000004cu,0x00000059u,0x00000000u,0x00000000u,0x00000026u,0x00000022u,
    0x00000072u,0x00000059u,0x00000000u,0x00000000u,0x00000026u,0x00000022u,0x00000026u,0x0000007cu,
    0x00000000u,0x00000000u,0x00000026u,0x00000021u,0x0000004cu,0x0000007cu,0x00000000u,0x00000000u,
    0x00000026u,0x00000021u,0x00000072u,0x0000007cu,0x00000000u,0x00000000u,0x00000026u,0x00000021u,
    0x000000d7u,0x000000e7u,0x00000000u,0x00000000u,0x00000027u,0x00000016u,
}};
inline constexpr std::array<std::uint32_t,72> place={{ //004853A0
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,0x00000047u,0x00000058u,0x00000049u,0x00000000u,
    0x00000000u,0x00000000u,0x00000047u,0x00000058u,0x00000092u,0x00000000u,0x00000000u,0x00000000u,
    0x00000047u,0x00000058u,0x00000000u,0x000000b9u,0x00000008u,0x0000000bu,0x0000002cu,0x00000046u,
    0x0000002cu,0x000000b9u,0x0000000cu,0x0000000bu,0x00000027u,0x00000046u,0x00000053u,0x000000b9u,
    0x00000007u,0x0000000bu,0x0000002du,0x00000046u,0x00000080u,0x000000b9u,0x0000000cu,0x0000000bu,
    0x00000026u,0x00000046u,0x000000a7u,0x000000b9u,0x00000006u,0x0000000bu,0x0000002eu,0x00000046u,
    0x000000e0u,0x00000000u,0x00000000u,0x00000000u,0x0000001du,0x0000001au,0x0000009bu,0x00000058u,
    0x00000038u,0x00000008u,0x0000002cu,0x00000029u,0x000000c9u,0x00000058u,0x00000038u,0x00000008u,
    0x0000002cu,0x00000029u,0x0000009bu,0x00000082u,0x00000038u,0x00000008u,0x0000002cu,0x00000029u,
}};
inline constexpr std::array<std::uint32_t,25> timer={{ //00485550
    0x3fd55555u,0x3dcccccdu,0x41200000u,0x43340000u,0x43650000u,0x3e2aaaabu,0x3f800000u,0x3f800000u,
    0x43340000u,0x43650000u,0x3c888889u,0x41200000u,0x3dcccccdu,0x43340000u,0x43720000u,0x3ada740eu,
    0x42c80000u,0x3c23d70au,0x42d80000u,0x43580000u,0x3991a2b4u,0x44160000u,0x3ada740du,0x43340000u,
    0x43720000u,
}};
inline constexpr std::array<std::uint32_t,8> positions={{ //004855F0
    0x00000000u,0xfffffff3u,0xffffffe2u,0xffffffd5u,0xffffffc4u,0xffffffb7u,0xffffffaau,0x00000000u,
}};
inline constexpr std::array<std::uint32_t,36> icons={{ //00485720
    0x00000000u,0x44200000u,0x42800000u,0x442f0000u,0x00000000u,0x44200000u,0x42800000u,0x442f0000u,
    0x42800000u,0x44200000u,0x43000000u,0x442f0000u,0x43000000u,0x44200000u,0x43400000u,0x442f0000u,
    0x43400000u,0x44200000u,0x43800000u,0x442f0000u,0x43800000u,0x44200000u,0x43a00000u,0x442f0000u,
    0x43a00000u,0x44200000u,0x43c00000u,0x442f0000u,0x43c00000u,0x44200000u,0x43e00000u,0x442f0000u,
    0x00000000u,0x00000000u,0x00000000u,0x00000000u,
}};
inline constexpr std::array<std::int8_t,16> animation={{0,13,26,39,39,26,13,0,0,11,22,33,33,22,11,0}};
inline constexpr std::array<std::uint32_t,10> star_x={{0u,13u,26u,39u,52u,66u,79u,92u,105u,118u}};
}
