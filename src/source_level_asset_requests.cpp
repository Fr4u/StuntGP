#include "sgn/model_resources.hpp"
#include "sgn/source_script_search.hpp"
#include "sgn/source_request_search.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_platform.hpp"
#include "sgn/scene_resources.hpp"
#include "sgn/retail_world_view_prefix.hpp"
#include <bit>

namespace sgn::retail
{
void ModelResources::produce_source_level_assets_42c740(RaceSessionPlatform &platform, SourceBootSearchBoundary input)
{
    platform.require_owner_thread();
    if (&platform.source_scene_resources().models() != this)
        throw SourceRequestError(0x42c740, "Asset producer belongs to a different platform scene");
    auto &requests = source_loading_.requests;
    requests.check_thread(0x42c740);
    auto cursor = input; // 42C74E initializes the actual local slot from ECX.
    const auto token = [&](SourceRequestName destination) {
        const auto read = [&](std::uint32_t offset, std::uint32_t site) {
            auto source = cursor;
            source.offset = offset;
            return read_source_script_byte(platform, source, site);
        };
        const auto write = [&](std::uint32_t relative, std::uint8_t byte, std::uint32_t site) {
            if (!destination)
                throw SourceRequestError(site, "Reached null request-name destination");
            const auto at = std::uint64_t(destination.offset()) + relative;
            if (at >= requests.names_.size())
                throw SourceRequestError(site, "Reached write outside request-name owner");
            requests.names_[static_cast<std::size_t>(at)] = static_cast<char>(byte);
        };
        auto ecx = cursor.offset;
        for (;; ++ecx)
        {
            const auto c = read(ecx, 0x42c5eb);
            if (c != ' ' && c != '\r' && c != '\n')
                break;
        }
        std::uint32_t length = 0, eax = ecx;
        auto bl = read(ecx, 0x42c600);
        if (bl != ' ')
            for (;;)
            {
                if (bl == '\r')
                    break;
                bl = read(eax, 0x42c614);
                if (bl == '\n')
                    break;
                write(length, bl, 0x42c61b);
                bl = read(eax + 1, 0x42c61e);
                ++length;
                ++eax;
                if (bl == ' ')
                    break;
            }
        write(length, 0, 0x42c62d);
        cursor.offset = ecx + length + 1u; // 42C638, after terminator store; same local slot.
    };
    const auto record = [&](std::uint32_t index, std::uint32_t site) -> SourceRequestRecord & {
        if (index >= requests.records_.size())
            throw SourceRequestError(site, "Reached request-record write outside owner");
        return requests.records_[index];
    };
    if (find_source_script_key_42c580(platform, cursor, {{}, 0x4777f4, true}, &cursor) == 1)
    {
        token(requests.append_);            // 42C76B, direct bank-to-name-owner bytes.
        const auto name = requests.append_; // 42C770
        auto index = requests.count_;       // 42C77D BEFORE strlen.
        const auto extent = requests.name_extent(0x42c783);
        record(index, 0x42c787).name = name;
        record(index, 0x42c78f).role = 2;
        ++index;
        const auto next = name.advanced(extent); // 42C79B pointer value.
        requests.count_ = index;                 // 42C79F
        requests.append_ = next;                 // 42C7A5
        record(index, 0x42c7ab).name = {};       // role of sentinel is preserved.
        auto &sky = platform.state().texture_animation.sky;
        clear_sky_texture_motion(sky); // 42C7B2 -> full425880.
        const auto captured = requests.read_count_58c808_42c7b7();
        const auto haystack = requests.read_record_name_42c7c2(captured);
        if (find_source_request_462890(haystack, platform.source_application_literals(), 0x4777fc))
        {
            sky[0].step_u = std::bit_cast<float>(std::uint32_t{0});          // 42C7D6
            sky[0].step_v = std::bit_cast<float>(std::uint32_t{0});          // 42C7E0
            sky[1].step_u = std::bit_cast<float>(std::uint32_t{0x3a378034}); // 42C7EA
            sky[1].step_v = std::bit_cast<float>(std::uint32_t{0x3a378034}); // 42C7F4
        }
    }
    // 42C7FE reloads the SAME cursor, including advancement by the sky token.
    if (find_source_script_key_42c580(platform, cursor, {{}, 0x47780c, true}, &cursor) == 1)
    {
        token(requests.append_);            // 42C820.
        const auto name = requests.append_; // 42C825
        auto index = requests.count_;       // 42C832
        requests.track_name_ = name;        // 42C838 BEFORE strlen.
        const auto extent = requests.name_extent(0x42c83e);
        record(index, 0x42c842).name = name;
        record(index, 0x42c84a).role = 3;
        ++index;
        const auto next = name.advanced(extent); // 42C856
        requests.count_ = index;                 // 42C85A
        requests.append_ = next;                 // 42C860
        record(index, 0x42c866).name = {};
    } // 42C86D..42C872 RET; no model load, car producer or readiness publication.
}
} // namespace sgn::retail
