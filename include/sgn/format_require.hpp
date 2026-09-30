#pragma once
// Shared validation helper (code organisation only, no new behaviour).
// format_check::require(ok, why) is exactly the per-file helper
//     void require(bool ok, const char* why) { if (!ok) throw FormatError(why); }
// that many translation units define in their anonymous namespace. A file adopts it
// with `using format_check::require;` in that same anonymous namespace, so every call
// site keeps its spelling. Only adopt it where the local body is identical in meaning
// (throws sgn::FormatError with the given message and nothing else).
#include "sgn/assets.hpp"

namespace sgn::format_check
{
inline void require(bool ok, const char *why)
{
    if (!ok)
        throw FormatError(why);
}
} // namespace sgn::format_check
