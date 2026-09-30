#pragma once
#include <cstddef>
#include <cstdint>
#include <span>

namespace sgn::retail
{
// Every view borrows the sole FrontendSourceStorage owner. Item tables share
// their boundary/default rows, so this is one catalog, not eleven copied lists.
struct CampaignConfigurationCatalog
{
    std::span<const std::byte, 0x57E4> items; // 5F3AF4, 75 rows of 300 bytes
    std::span<const std::byte, 0x660> rules;  // 5F943C, authored campaign bank
};
// Complete 436420, including its signed high-product discount arithmetic.
std::uint32_t campaign_item_price_436420(CampaignConfigurationCatalog, std::uint32_t sponsor,
                                         std::span<const std::byte, 16> selection,
                                         std::span<const std::byte, 300> item);
// Complete 4365D0 with the source caller's unused fourth stack argument = 0.
void apply_campaign_upgrade_4365d0(CampaignConfigurationCatalog, std::uint32_t sponsor, std::uint32_t kind,
                                   std::span<const std::byte, 0xEC> configuration,
                                   std::span<std::byte, 300> destination);
// Complete 436490: zero 0x218, copy configuration+base model, apply all eleven
// parts in source order. The caller supplies distinct input and output views.
void compose_campaign_configuration_436490(CampaignConfigurationCatalog, std::uint32_t sponsor,
                                           std::span<const std::byte, 0xEC> configuration,
                                           std::span<const std::byte, 300> model,
                                           std::span<std::byte, 0x218> destination);
std::uint32_t campaign_configuration_value_437de0(CampaignConfigurationCatalog, std::uint32_t sponsor,
                                                  std::uint32_t car, std::span<const std::byte, 0xEC> configuration);

// Full 43D5F0 for the 44BD00 call contract EDX=0. Return includes its terminating
// zero; negative signed values return pinned literal "0" without scratch writes.
std::span<const std::byte> format_campaign_value_43d5f0(std::uint32_t, std::span<std::byte, 0x80> numeric_scratch);
struct CampaignPresentationViews
{
    std::span<const std::byte, 0x160> live; // 5F92DC
    std::span<std::byte, 0x148> display;    // 5F9AA4..5F9BEB
    CampaignConfigurationCatalog catalog;
    std::span<std::byte, 4> initials_scratch;   // 47966C, source fourth byte = 0
    std::span<std::byte, 0x80> numeric_scratch; // 5FF328 reverse64 + forward64
};
// New read-only host preflight, shared by the presentation and full settlement
// adapters so invalid input is rejected before either publishes source writes.
void validate_campaign_presentation(CampaignPresentationViews);
// Full 44BD00, including original names, sort, configuration valuation and both
// format calls. It does not publish phase11 or call the results-page producer.
void build_campaign_presentation_44bd00(CampaignPresentationViews);
} // namespace sgn::retail
