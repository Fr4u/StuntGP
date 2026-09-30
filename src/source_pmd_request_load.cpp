#include "sgn/scene_resources.hpp"
#include <array>

namespace sgn::retail
{
void SceneResources::load_source_requests_42f090(SourceAllocationBank &bank, std::uint32_t scene_mode,
                                                 std::span<const SelectionRecord, 6> selection,
                                                 std::span<LevelCarSlot, 6> cars, SourcePmdObserver *observer,
                                                 SourceResourceRegistryObserver *registry_observer)
{
    // The existing loose MeshData route; every request still makes its own
    // 42EFB0 allocation/read. No filename cache or legacy decoded model is used.
    require_owner_thread();
    SourcePmdFileSystem files(models_.root_);
    load_source_requests_42f090(bank, files, scene_mode, selection, cars, observer, registry_observer);
}

void SceneResources::load_source_requests_42f090(SourceAllocationBank &bank, SourcePmdFileSource &files,
                                                 std::uint32_t scene_mode,
                                                 std::span<const SelectionRecord, 6> selection,
                                                 std::span<LevelCarSlot, 6> cars, SourcePmdObserver *observer,
                                                 SourceResourceRegistryObserver *registry_observer)
{
    require_owner_thread();
    auto &loading = models_.source_loading_;
    auto &requests = loading.requests;
    requests.check_thread(0x42f090);
    // 42F090..42F0A9. Scene2 clears only the first name pointer and count;
    // name bytes, first role, remaining records and append pointer survive.
    if (scene_mode == 2)
    {
        requests.records_[0].name = {};
        requests.count_ = 0;
    }
    auto name = requests.records_[0].name;   // EDX, captured before the REP stores
    loading.opaque_cursor_58c80c = 0x58c810; // 42F0BD, source address value only
    for (auto &word : loading.opaque_words_58c810)
        word = 0;                            // 42F0C9, DF=0,1024 DWORDs
    loading.opaque_cursor_58c538 = 0x58c600; // 42F0CB; this does NOT clear58C600

    std::uint32_t index = 0;
    while (name)
    {
        // Host filename transport is a synchronous value copy for the existing
        // file API, not a retained replacement for the live request-name owner.
        const auto id = models_.load_source(bank, files, name.text_snapshot(), observer); // 42F0DE ->42EFB0
        // Source reads the role AFTER the load; count58C808 never controls this walk.
        const auto role = requests.records_[index].role; // 42F0E3
        if (role >= records_.size())
            throw SourceRequestError(0x42f0f8, "request role outside the admitted scene registry");
        const auto handle = *models_.resolve(id);
        const auto file = loading.file; // EDX at42F0EC, before descriptor publication
        auto &row = records_[role];
        row.descriptor = ScenePmdBinding{handle}; // 42F0F8: +0, geometry and name-table descriptor
        if (registry_observer)
            registry_observer->published({0x42f0f8, 0x636418 + role * 16, role, 0}, *this);
        const auto file_size = loading.file_size; // ECX at42F0FE
        row.file = file;                          // 42F104: +8; retains allocation identity after source release
        if (registry_observer)
            registry_observer->published({0x42f104, 0x636420 + role * 16, role, 8}, *this);
        row.file_size = file_size; // 42F10A: +0C
        if (registry_observer)
            registry_observer->published({0x42f10a, 0x636424 + role * 16, role, 12}, *this);
        // No +4 store: the same world's SessionWorldMaterials still owns flags.
        ++index;
        if (index >= requests.records_.size())
            throw SourceRequestError(0x42f110, "request list has no in-region null sentinel");
        name = requests.records_[index].name; // 42F110: read next live pointer, not a prebuilt batch
    }

    // Literal DWORD tables at477C4C/477C64. Compact index advances only for
    // kind2; physical selection and car strides remain150/64 x86 bytes.
    constexpr std::array<std::uint32_t, 6> body_roles{4, 6, 8, 10, 12, 14};
    constexpr std::array<std::uint32_t, 6> auxiliary_roles{5, 7, 9, 11, 13, 15};
    auto descriptor_identity = [&](std::uint32_t role, std::uint32_t site) {
        const auto &value = records_[role].descriptor;
        if (std::holds_alternative<std::monostate>(value))
            return std::uint32_t{0};
        if (const auto *pmd = std::get_if<ScenePmdBinding>(&value))
            return models_.identity(pmd->handle);
        // Never turn a non-PMD pointer into a null model or a guessed token.
        throw SourceRequestError(site, "vehicle slot descriptor is not a PMD identity");
    };
    unsigned compact = 0;
    for (unsigned physical = 0; physical < 6; ++physical)
    {
        if (selection[physical].kind != 2)
            continue;                                                                      // 42F122/42F125
        cars[physical].source_word00 = descriptor_identity(body_roles[compact], 0x42f130); // 42F136
        const auto auxiliary = auxiliary_roles[compact];                                   // 42F139
        ++compact;                                                                         // 42F142
        cars[physical].source_word04 = descriptor_identity(auxiliary, 0x42f145);           // 42F14B
    }
    // 42F15E returns. No descriptor LOD, texture name, livery, tyre selection,
    // bank lifetime, material flags or future42C510 upload has been changed.
}
} // namespace sgn::retail
