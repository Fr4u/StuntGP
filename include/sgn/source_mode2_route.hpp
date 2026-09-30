#pragma once
#include "sgn/source_single_page_navigation.hpp"
namespace sgn::retail
{
struct SourceMode2RouteServices
{
    virtual ~SourceMode2RouteServices() = default;
    virtual std::uint32_t car_available_437230(FrontendState &, std::uint32_t car, std::uint32_t mode) = 0;
    virtual std::uint32_t track_available_4371c0(FrontendState &, std::uint32_t track, std::uint32_t mode) = 0;
    virtual void rank_car_437cc0(FrontendState &, std::uint32_t car) = 0;
    // Original 441170(31) result used as live format, one variadic integer1.
    // Output is the actual caller local64, not a replacement initials string.
    // Returns the initialized prefix including NUL; fewer than3 bytes is an undefined source read.
    virtual std::uint32_t format_name_46280c(FrontendState &, FrontendLanguageTextIdentity format,
                                             std::uint32_t argument, std::span<std::uint8_t, 64> output) = 0;
    virtual void prepare_race_43a780(FrontendState &, std::uint32_t source_address) = 0;
    virtual void finish_race_43d500(FrontendState &, std::uint32_t ecx) = 0;
};
void advance_mode2_44cd70(FrontendState &, SourcePageFactoryServices &, SourceNavigationPageServices &,
                          CampaignScoreMusic &, SourceMode2RouteServices &);
} // namespace sgn::retail
