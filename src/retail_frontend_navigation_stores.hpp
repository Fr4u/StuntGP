#pragma once
#include <cstdint>

namespace sgn::retail::detail {
//Logical source store order. The production sink writes the sole typed bank.
//A CPU qualification sink records the same operations without native services.
template<class Store> constexpr void frontend_navigation_tail_stores_43dee0(Store&& store) noexcept(noexcept(store(std::uint32_t{},std::uint32_t{}))) {
    store(0x5ff470u,0u); //0043dee0
    store(0x5ff474u,0u); //0043deea
    store(0x5ff478u,0u); //0043def4
    store(0x5ff47cu,0u); //0043defe
    store(0x5ff480u,0u); //0043df08
    store(0x5ff484u,0u); //0043df12
    store(0x5ff488u,0u); //0043df1c
    store(0x5ff48cu,0u); //0043df26
    store(0x5ff490u,0u); //0043df30
    store(0x5ff494u,0u); //0043df3a
    store(0x5ff498u,0u); //0043df44
    store(0x5ff49cu,0u); //0043df4e
}
template<class Store> constexpr void frontend_navigation_stores_43dda0(Store&& store) noexcept(noexcept(store(std::uint32_t{},std::uint32_t{}))) {
    for(std::uint32_t address=0x5ff3e8;address<0x5ff4b8;address+=4)store(address,0u); //43DDAD: REP STOSD,52 DWORDs
    store(0x5ff3ecu,65536u); //0043ddb4
    store(0x5ff3e8u,1u); //0043ddbe
    store(0x5ff3f0u,0u); //0043ddc4
    store(0x5ff3f4u,0u); //0043ddc9
    store(0x5ff3f8u,0u); //0043ddce
    store(0x5ff3fcu,0u); //0043ddd3
    store(0x5ff440u,0u); //0043ddd8
    store(0x5ff444u,1u); //0043dddd
    store(0x5ff448u,0u); //0043dde3
    store(0x5ff44cu,0u); //0043dde8
    store(0x5ff450u,0u); //0043dded
    store(0x5ff454u,0u); //0043ddf2
    store(0x5ff458u,0u); //0043ddf7
    store(0x5ff45cu,0u); //0043ddfc
    store(0x5ff460u,0u); //0043de01
    store(0x5ff464u,0u); //0043de06
    store(0x5ff468u,0u); //0043de0b
    store(0x5ff46cu,0u); //0043de10
    store(0x5ff4a0u,0u); //0043de15
    store(0x5ff4a4u,0u); //0043de1a
    store(0x5ff4a8u,0u); //0043de1f
    store(0x5ff4acu,0u); //0043de24
    store(0x5ff4b4u,0u); //0043de29
    frontend_navigation_tail_stores_43dee0(store); //43DE2F: tail jump
}
}
