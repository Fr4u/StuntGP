#pragma once
#include <cstdint>
namespace sgn::retail
{
class SourceFrontendCsv;
std::uint32_t parse_frontend_csv_435e30(SourceFrontendCsv &, std::uint32_t car, std::uint32_t destination);
std::uint32_t parse_frontend_csv_435fa0(SourceFrontendCsv &, std::uint32_t prefix_source_address,
                                        std::uint32_t destination, std::int32_t count);
std::uint32_t parse_frontend_csv_435980(SourceFrontendCsv &);
std::uint32_t parse_frontend_csv_435860(SourceFrontendCsv &);
} // namespace sgn::retail
