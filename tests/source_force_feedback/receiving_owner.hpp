#pragma once
#include "sgn/source_input_dispatch.hpp"

namespace receiving {
// Explicit test values written into the canonical mutable profile owner.
// They are NOT recovered defaults and are never additional selector storage.
inline sgn::retail::MenuDeviceSelectorsView fixture_selectors(sgn::retail::SourceInputStorage& owner) {
    auto* bytes=reinterpret_cast<std::uint8_t*>(owner.profiles[2].data());
    bytes[10]=0; bytes[11]=1; owner.profiles[2][3]=1;
    return sgn::retail::source_menu_selectors(owner.profiles);
}
}
