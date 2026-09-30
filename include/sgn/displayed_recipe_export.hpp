#pragma once
// SGNDRCP v1 retained when transforms are absent. v2 appends three tagged
// transforms per snapshot and an optional tagged transform per device event.
// Tags:0 unknown,1 sixteen literal DWORDs,2 unspecified431420 stack.
#include "sgn/polygon_submission.hpp"
#include <filesystem>
#include <iosfwd>

namespace sgn::diagnostics
{
// NEW, opt-in CPU diagnostics. Serializes existing immutable presentation data;
// it neither draws nor identifies source producers. See displayed-recipe-v1.md.
// Float fields are exported as their binary32 bits, never decimal conversions.
void write_displayed_recipe(std::ostream &, const ImmutablePolygonSubmission &);
// Explicit caller-owned output path; must not already exist. Parent must exist.
// A failed write throws; the footer lets the reader reject a partial file.
void export_displayed_recipe(const std::filesystem::path &, const ImmutablePolygonSubmission &);
} // namespace sgn::diagnostics
