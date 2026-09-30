#include "sgn/source_car_selection.hpp"
#include "sgn/source_application_literals.hpp"
#include "sgn/model_resources.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"
#include <cstring>
#include <optional>
#include <type_traits>

namespace sgn::retail
{
namespace
{
using Word = std::uint32_t;
static_assert(std::is_trivially_copyable_v<SelectionRecord> && sizeof(SelectionRecord) == 0x150);
static_assert(std::is_trivially_copyable_v<LevelCarSlot> && sizeof(LevelCarSlot) == 0x64);
// Both arrays contain source scalar words/bytes only, no host pointer encoding.
// The full source-owned region is accessible, including a name crossing its
// nominal member into following represented words. Bounds fail only on access.
struct Region
{
    unsigned char *bytes;
    std::size_t size;
    Word read(Word offset, Word width, Word pc) const
    {
        if (offset > size || size - offset < width)
            throw SourceRequestError(pc, "Reached source read outside selection/car owner");
        Word value = 0;
        std::memcpy(&value, bytes + offset, width);
        return value;
    }
    void write(Word offset, Word width, Word value, Word pc) const
    {
        if (offset > size || size - offset < width)
            throw SourceRequestError(pc, "Reached source store outside car owner");
        std::memcpy(bytes + offset, &value, width);
    }
};
Word extent(Region source, Word offset, Word pc)
{
    Word length = 0;
    while (source.read(offset + length, 1, pc) != 0)
        ++length;
    return length + 1u;
}
void copy_words(Region source, Word from, Region destination, Word to, Word size, Word dword_pc, Word byte_pc)
{
    Word offset = 0;
    for (Word count = size >> 2; count != 0; --count)
    {
        const auto value = source.read(from + offset, 4, dword_pc);
        destination.write(to + offset, 4, value, dword_pc);
        offset += 4;
    }
    for (Word count = size & 3u; count != 0; --count)
    {
        const auto value = source.read(from + offset, 1, byte_pc);
        destination.write(to + offset, 1, value, byte_pc);
        ++offset;
    }
}
} // namespace
// Full caller body conditional on a genuine full444DC9 binding. The source
// clear is deliberately not replaced by the old represented typed={} reset.
void ModelResources::produce_source_car_requests_444db0(RaceSessionPlatform &platform, SourceVehicleClear444dc9 &clear)
{
    platform.require_owner_thread();
    if (&platform.source_scene_resources().models() != this)
        throw SourceRequestError(0x444db0, "Car producer belongs to another platform scene");
    clear.clear_444dc9(platform); // 444DC9: before selection, request reads or writes.
    auto &s = platform.state();
    auto &requests = source_loading_.requests;
    requests.check_thread(0x444df3);
    Region selection{reinterpret_cast<unsigned char *>(s.selection.data()), sizeof(s.selection)};
    Region cars{reinterpret_cast<unsigned char *>(s.cars.data()), sizeof(s.cars)};
    Word selected = 0;
    for (const auto &record : s.selection)
        if (record.kind == 2)
            ++selected;             // 444DE5..F1
    auto append = requests.append_; // 444DF3 capture EBX once, not each iteration.
    Word count = requests.count_;   // 444E01 capture EDX once.
    Word role = 4;
    std::optional<Word> previous;
    Word copied = 0;
    const auto app = platform.source_application_literals();
    const auto record = [&](Word index, Word pc) -> SourceRequestRecord & {
        if (index >= requests.records_.size())
            throw SourceRequestError(pc, "Reached request-record store outside owner");
        return requests.records_[index];
    };
    const auto app_extent = [&](Word source, Word pc) {
        Word length = 0;
        while (app.read_model_catalog_byte_444db0(source + length, pc) != 0)
            ++length;
        return length + 1u;
    };
    const auto copy_app = [&](Word source, Word bytes, Word dword_pc, Word byte_pc) {
        Word offset = 0;
        const auto write = [&](Word value, Word width, Word pc) {
            if (!append)
                throw SourceRequestError(pc, "Reached null request append store");
            const auto at = std::uint64_t(append.offset()) + offset;
            if (at > requests.names_.size() || requests.names_.size() - at < width)
                throw SourceRequestError(pc, "Reached request-name store outside owner");
            std::memcpy(requests.names_.data() + at, &value, width);
        };
        for (Word remaining = bytes >> 2; remaining != 0; --remaining)
        {
            const auto value = app.read_model_catalog_word_444db0(source + offset, dword_pc);
            write(value, 4, dword_pc);
            offset += 4;
        }
        for (Word remaining = bytes & 3u; remaining != 0; --remaining)
        {
            const auto value = app.read_model_catalog_byte_444db0(source + offset, byte_pc);
            write(value, 1, byte_pc);
            ++offset;
        }
    };
    const auto name_extent = [&](Word pc) {
        Word size = 0;
        for (;; ++size)
        {
            if (!append)
                throw SourceRequestError(pc, "Reached null request strlen");
            const auto at = std::uint64_t(append.offset()) + size;
            if (at >= requests.names_.size())
                throw SourceRequestError(pc, "Reached request strlen outside owner");
            if (requests.names_[static_cast<std::size_t>(at)] == 0)
                return size + 1u;
        }
    };
    for (Word slot = 0; slot < 6; ++slot)
    {
        auto &from = s.selection[slot];
        auto &to = s.cars[slot];
        if (from.kind != 2)
            continue;
        copy_words(selection, slot * 0x150u + 0x10u, cars, slot * 0x64u + 0x24u,
                   extent(selection, slot * 0x150u + 0x10u, 0x444e22), 0x444e38, 0x444e42);
        if (from.tyre != 0)
            to.settings.tyre = from.tyre;
        if (from.config != 0)
            to.settings.config = from.config;
        to.model = from.model;
        if (from.livery != 8)
            to.settings.livery = from.livery;
        if (from.control == 0)
            from.ai_skill = 1; // writes actual selection source.
        if (from.ai_skill != 0)
            to.settings.ai_skill = from.ai_skill;
        if (from.ai_skill != 1000)
            to.settings.ai_mode = from.ai_mode;
        // 444EA2 reads the live budget for EACH selected car after above stores.
        if (app.read_model_memory_budget_444ea2() <= 0x400000u && selected >= 6u)
        {
            Word model;
            if (previous && from.control != 0 && copied < 1u)
            {
                ++copied;
                copy_words(cars, *previous * 0x64u, cars, slot * 0x64u, 0x64, 0x444ef2, 0x444ef2);
                if (from.ai_skill != 0)
                    to.settings.ai_skill = from.ai_skill;
                if (from.ai_skill != 1000)
                    to.settings.ai_mode = from.ai_mode;
                if (from.tyre != 0)
                    to.settings.tyre = from.tyre;
                copy_words(selection, slot * 0x150u + 0x10u, cars, slot * 0x64u + 0x24u,
                           extent(selection, slot * 0x150u + 0x10u, 0x444f29), 0x444f3f, 0x444f46);
                model = app.read_model_catalog_word_444db0(0x482c08u + s.selection[*previous].model * 8u, 0x444f4e);
            }
            else
                model = app.read_model_catalog_word_444db0(0x482c08u + from.model * 8u, 0x444f5a);
            copy_app(model, app_extent(model, 0x444f66), 0x444f75, 0x444f7e);
            record(count, 0x444f83).name = append;
            record(count, 0x444f8a).role = role;
            record(count + 1u, 0x444f96).name = {}; // before first destination strlen.
            const auto first_size = name_extent(0x444f9d);
            const auto shadow = app.read_model_catalog_word_444db0(0x482c44, 0x444f9f);
            ++count;
            append = append.advanced(first_size);
            const auto second_size = app_extent(shadow, 0x444fb0);
            record(count, 0x444fb6).name = append; // BEFORE copying shadow bytes.
            copy_app(shadow, second_size, 0x444fc6, 0x444fcf);
            record(count, 0x444fd7).role = role + 1u;
            const auto written = name_extent(0x444fe1);
            ++count;
            requests.count_ = count; // 444FE7
            record(count, 0x444fed).name = {};
            append = append.advanced(written);
            requests.append_ = append; // 444FFD
            if (!previous && from.control != 0)
                previous = slot;
        }
        else
        {
            const auto table_offset = from.model * 8u; // saved source[EBP-20]
            const auto model = app.read_model_catalog_word_444db0(0x482c08u + table_offset, 0x445034);
            copy_app(model, app_extent(model, 0x44503c), 0x44504b, 0x445054);
            record(count, 0x445059).name = append;
            record(count, 0x445060).role = role;
            record(count + 1u, 0x44506c).name = {};
            const auto first_size = name_extent(0x445073);
            const auto shadow = app.read_model_catalog_word_444db0(0x482c0cu + table_offset, 0x44507b);
            ++count;
            append = append.advanced(first_size);
            requests.append_ = append; // 44508B BEFORE second strlen.
            requests.count_ = count;   // 445091
            copy_app(shadow, app_extent(shadow, 0x445097), 0x4450a6, 0x4450af);
            record(count, 0x4450b1).name = append;
            record(count, 0x4450be).role = role + 1u;
            const auto written = name_extent(0x4450c8);
            ++count;
            requests.count_ = count; // 4450CE
            append = append.advanced(written);
            record(count, 0x4450d8).name = {};
            requests.append_ = append; // 4450DF
        }
        role += 2u; // 4450E5..EB only after a selected car completes both requests.
    }
}
} // namespace sgn::retail
