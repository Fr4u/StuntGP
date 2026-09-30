#include "sgn/source_time_trial_save.hpp"
#include "sgn/source_replay_selected.hpp"
#include "sgn/source_selection_data.hpp"
#include "sgn/source_title_page.hpp"
#include "sgn/race_session.hpp"

namespace sgn::retail
{
void advance_time_trial_save_phase11_44dc2a(FrontendState &f, SourcePageFactoryServices &factory,
                                            SourceNavigationPageServices &pages, CampaignScoreMusic &music)
{
    f.shared_platform()->require_owner_thread();
    const auto gate = f.source.source_word(0x61c1c0); // 44DC2A: captured before phase store
    f.source.source_word(0x5e3794) = 12;              // 44DC2F, including both unsuccessful paths
    if (gate == 0)
        return; // 44DC39/3B ->44DCD1, no replay/capacity reads

    const auto track = f.source.source_word(0x5e39d4);      // 44DC41
    const auto capacity = source_track_value_437da0(track); // 44DC47, retained EBX
    const auto selected = f.source.source_word(0x61c1f8);   // 44DC4E, actual result+8
    if (compress_selected_native_replay_44dc5b(f.source, f.world().replays, selected, capacity) == 0)
        return; // 44DC60/62: preserves any codec writes, no copy or Save27

    // 44DC78: exactly7500 forward DWORD transfers, including the retained
    // tail beyond the compressed payload. Neither output is a new buffer.
    // Each source word is captured before its destination word is written.
    for (std::uint32_t offset = 0; offset < 30000; offset += 4)
    {
        const auto value = f.source.source_word(0x5eb178u + offset);
        f.source.source_word(0x5e3c48u + offset) = value;
    }
    const auto fresh_track = f.source.source_word(0x5e39d4);          // 44DC7A after full copy
    f.source.source_word(0x5e3c44) = fresh_track;                     // 44DC81
    f.source.source_word(0x5e3c1c) = 0;                               // 44DC86
    f.source.source_word(0x5e3c20) = 1;                               // 44DC8C
    f.source.source_word(0x5e3c28) = 5;                               // 44DC96, replay-save operation
    f.source.source_word(0x5e3c24) = 0;                               // 44DCA0
    f.source.source_word(0x5e3c2c) = fresh_track;                     // 44DCA6
    f.source.source_word(0x5e3c30) = 0x5e3c48;                        // 44DCAB, existing frontend source identity
    f.source.source_word(0x5e3c34) = capacity;                        // 44DCB5, original capacity, not codec return
    (void)push_frontend_page_43dfa0(f, 0, 27, factory, pages, music); // 44DCBB
}
} // namespace sgn::retail
