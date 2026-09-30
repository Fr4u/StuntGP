#pragma once
#include "sgn/retail_frontend_state.hpp"
namespace sgn::retail
{
// Full43AE00. Borrows the caller's actual source roster/result owners and
// publishes them in return slot4 before43A720. No result reset or payload copy.
// Proven callers:44C8DB(61BAC8,61B9C0),44D62F(61C0A0,61C0B8).
void prepare_frontend_challenge_43ae00(FrontendState &, std::uint32_t roster_address, std::uint32_t result_address);
// Full43D690: return the captured value of live479668 after its three writes.
// The returned source address is borrowed from the same frontend bank.
std::uint32_t copy_frontend_initials_43d690(FrontendState &, std::uint32_t source_address);
} // namespace sgn::retail
