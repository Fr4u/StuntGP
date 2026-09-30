#pragma once
#include <cstdint>

namespace sgn::retail
{
// Native identity of one of the six original recording buffers. It is not an
// EXE pointer; ReplayBank validates the slot before any memory access.
struct ReplayHandle
{
    std::uint32_t slot{};
    friend bool operator==(ReplayHandle, ReplayHandle) = default;
};
} // namespace sgn::retail
