#pragma once
#include "sgn/source_menu_update.hpp"
#include "sgn/source_menu_navigation.hpp"
#include "sgn/retail_persistence.hpp"
namespace sgn::retail
{
struct SourceBootFilePlatform;
// Borrowed startup dependency; concrete implementation below owns no data.
struct SourceFrontendDataStartup
{
    virtual ~SourceFrontendDataStartup() = default;
    virtual void initialize_data_434b50(FrontendState &, SourceBootFilePlatform &) = 0;
};
struct SourceFrontendDataStartupImpl final : SourceFrontendDataStartup
{
    void initialize_data_434b50(FrontendState &, SourceBootFilePlatform &) override;
};
void initialize_source_unlocks_43c370(FrontendState &);
void initialize_source_records_43c130(FrontendState &);
// Full parent composition: invokes prefix exactly once, then440AE0 and tail.
// All referents borrowed; constructing the caller context must be inert.
void initialize_source_frontend_43d0b0(FrontendState &, SourceBootFilePlatform &, SourceMenuUpdateChildren &,
                                       SourceNavigationPageServices &, PersistenceServices &,
                                       SourceFrontendDataStartup &);
// Tail starts43D114: ONLY for a caller that has already executed the prefix AND440AE0.
// Does not re-run either, and is not a retry of a partially completed tail.
void continue_source_frontend_43d114(FrontendState &, SourceBootFilePlatform &, SourceMenuUpdateChildren &,
                                     SourceNavigationPageServices &, PersistenceServices &,
                                     SourceFrontendDataStartup &);
} // namespace sgn::retail
