#pragma once
#include <cstdint>
namespace sgn::retail::detail {
//Logical single source store shared by the actual owner setter and CPU sink.
//Pinned43DD10..43DD39:16 instructions,42 bytes. Code SHA256:
//5966641ea46bf69b6ee9f9edc211417ecd141595435b717ff2c0ba69d06f943b.
template<class Store>constexpr void frontend_packed_word_store_43dd10(std::uint32_t ecx,Store&& store)
    noexcept(noexcept(store(std::uint32_t{},std::uint32_t{},std::uint32_t{}))) {
    const auto value=((ecx&0xffu)<<16u)|(ecx&0xff00u)|((ecx>>16u)&0xffu);
    store(0x43dd31u,0x5ff4b8u,value);
}
}
