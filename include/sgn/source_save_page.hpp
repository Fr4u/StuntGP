#pragma once
#include "sgn/source_main_page_navigation.hpp"
#include "sgn/retail_persistence.hpp"
#include "sgn/source_application_literals.hpp"
namespace sgn::retail
{
// Required synchronous leaf bindings over the SAME App/CRT owner. No FILE table here.
struct SourceSavePageServices
{
    virtual ~SourceSavePageServices() = default;
    virtual SourceApplicationLiteralRef application_literals() const = 0;
    virtual std::uint32_t changed_cards_440170() = 0;
    virtual void select_card_4402f0(std::uint32_t) = 0;
    virtual std::uint32_t card_capacity_440200(std::uint32_t &free_bytes) = 0;
    virtual std::uint32_t setup_status_440320() = 0;
    virtual std::uint32_t replay_status_4405c0(std::uint32_t) = 0;
    virtual std::uint32_t replay_blocks_4405b0(std::uint32_t) = 0;
    // Full457B36..457B78: actual440660 +43D550 +46280C into selected row+70.
    virtual void replay_row_457b36(FrontendState &, std::uint32_t page, std::uint32_t record) = 0;
    virtual PersistenceServices &persistence() = 0;
    virtual std::uint32_t format_card_440180() = 0;
    virtual std::uint32_t replay_save_458e10(FrontendState &) = 0;
    virtual std::uint32_t replay_load_458e30(FrontendState &) = 0;
    virtual void sleep_source_458de9(std::uint32_t milliseconds) = 0;
    virtual void draw_cards_4581b0(FrontendState &, std::uint32_t page) = 0;
    virtual void draw_progress_458730(FrontendState &, std::uint32_t page) = 0;
    virtual void draw_confirmation_459070(FrontendState &, std::uint32_t page, std::uint32_t text) = 0;
    // Actual4580FC..458151 formatting/background paths, only modes4/5.
    virtual void replay_background_4580fc(FrontendState &, std::uint32_t mode) = 0;
};
std::uint32_t create_frontend_save_page_43e000(FrontendState &, std::uint32_t key, SourceNavigationPageServices &,
                                               SourceMainPageServices &, SourceSavePageServices &);
void destroy_frontend_save_page_457ea0(FrontendState &, std::uint32_t page);
std::uint32_t dispatch_frontend_save_page_457ec0(FrontendState &, std::uint32_t page, std::uint32_t event,
                                                 SourcePageFactoryServices &, SourceNavigationPageServices &,
                                                 CampaignScoreMusic &, SourceMainPageServices &,
                                                 SourceSavePageServices &);
} // namespace sgn::retail
