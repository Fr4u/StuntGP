#pragma once
#include "sgn/source_car_script.hpp"
namespace sgn::retail
{
// Concrete binding of both required CRT children to the existing platform/App.
// No private locale, key cache, source-string snapshot or success default.
class SourceCarKeyFormatterImpl final : public SourceCarKeyFormatter
{
  public:
    std::uint32_t classify_number_4631f4(RaceSessionPlatform &, std::uint8_t character, std::uint32_t mask) override;
    void format_car_key_46280c(RaceSessionPlatform &, std::uint32_t format_address, std::uint32_t car_number,
                               SourceCarKeyCall &) override;
};
} // namespace sgn::retail
