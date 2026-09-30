#include "sgn/source_frontend_initialization.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/source_frontend_csv.hpp"
#include "sgn/source_frontend_csv_models.hpp"
#include "sgn/source_frontend_csv_lower.hpp"
#include "sgn/source_frontend_csv_window.hpp"
#include "sgn/retail_campaign_data.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/race_session.hpp"
#include <array>
namespace sgn::retail
{
namespace
{
using W = std::uint32_t;
std::string_view string(std::span<const std::uint8_t> bytes)
{
    return {reinterpret_cast<const char *>(bytes.data()), bytes.size() - 1};
}
} // namespace
void SourceFrontendDataStartupImpl::initialize_data_434b50(FrontendState &f, SourceBootFilePlatform &files)
{
    f.shared_platform()->require_owner_thread();
    auto &memory = f.resource_memory();
    W cursor = 0;
    const auto app = f.shared_platform()->source_application_literals();
    const auto load = [&](W name) { return memory.load_43c7f0(files, string(app.frontend_csv_literal(name))); };
    auto resource = load(0x478b70);
    {
        SourceFrontendCsv csv(f, memory.complete_input(resource, 0x434b6f).bytes, cursor);
        csv.next_line_43c5c0();
        W car = 1;
        for (W destination = 0x5f2798; destination < 0x5f3af4; destination += 0xec, ++car)
            (void)parse_frontend_csv_435e30(csv, car, destination);
    }
    memory.release_462f9e(resource, 0x434ba5);
    cursor = 0;
    resource = load(0x478b84);
    {
        SourceFrontendCsv csv(f, memory.complete_input(resource, 0x434bc1).bytes, cursor);
        csv.next_line_43c5c0();
        struct Group
        {
            W name, destination;
            std::int32_t count;
        };
        constexpr std::array<Group, 12> groups{{{0x478b98, 0x5f3c20, 21},
                                                {0x478b9c, 0x5f54bc, 5},
                                                {0x478ba4, 0x5f5a98, 5},
                                                {0x478bac, 0x5f6074, 5},
                                                {0x478bb4, 0x5f6650, 5},
                                                {0x478bbc, 0x5f6c2c, 5},
                                                {0x478bc8, 0x5f7208, 5},
                                                {0x478bd0, 0x5f77e4, 3},
                                                {0x478bd8, 0x5f7b68, 5},
                                                {0x478be0, 0x5f8144, 5},
                                                {0x478bec, 0x5f8720, 5},
                                                {0x478bf0, 0x5f8cfc, 5}}};
        for (const auto &g : groups)
            (void)parse_frontend_csv_435fa0(csv, g.name, g.destination, g.count);
    }
    memory.release_462f9e(resource, 0x434ccf);
    cursor = 0;
    resource = load(0x478bf8);
    {
        const auto input = memory.complete_input(resource, 0x434ceb);
        auto rules = std::as_writable_bytes(f.source.source_bytes(0x5f943c, campaign_data_size));
        auto numeric = std::as_writable_bytes(std::span{f.source.csv_token_scratch});
        const CampaignDataViews views{std::span<std::byte, campaign_data_size>{rules.data(), campaign_data_size},
                                      numeric};
        (void)parse_campaign_data_435b60(views, string(input.bytes), cursor);
    }
    memory.release_462f9e(resource, 0x434cf1);
    struct Remaining
    {
        W name, parse_site, free_site;
        W (*parse)(SourceFrontendCsv &);
    };
    constexpr std::array<Remaining, 8> remainder{{{0x478c0c, 0x434d0d, 0x434d13, parse_frontend_csv_435980},
                                                  {0x478c20, 0x434d2f, 0x434d35, parse_frontend_csv_435860},
                                                  {0x478c34, 0x434d51, 0x434d57, parse_frontend_csv_435730},
                                                  {0x478c44, 0x434d73, 0x434d79, parse_frontend_csv_4356f0},
                                                  {0x478c58, 0x434d95, 0x434d9b, parse_frontend_csv_435010},
                                                  {0x478c68, 0x434db7, 0x434dbd, parse_frontend_csv_434f60},
                                                  {0x478c7c, 0x434dd9, 0x434ddf, parse_frontend_csv_434e90},
                                                  {0x478c90, 0x434dfb, 0x434e01, parse_frontend_csv_434e10}}};
    for (const auto &call : remainder)
    {
        cursor = 0;
        resource = load(call.name);
        SourceFrontendCsv csv(f, memory.complete_input(resource, call.parse_site).bytes, cursor);
        (void)call.parse(csv);
        memory.release_462f9e(resource, call.free_site);
    }
}
} // namespace sgn::retail
