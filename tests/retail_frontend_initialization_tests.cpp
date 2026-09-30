#include "sgn/retail_frontend_initialization.hpp"
#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_launch.hpp"
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using namespace sgn::retail;
namespace {
std::uint64_t checks = 0, compared = 0;
void require(bool value, const char* reason) {
    ++checks;
    if (!value) throw std::runtime_error(reason);
}
struct Region {
    std::uint32_t address;
    std::span<std::byte> actual;
    std::vector<std::byte> expected;
};
template<class T> void add(std::vector<Region>& regions, std::uint32_t address, T& object) {
    static_assert(std::is_trivially_copyable_v<T>);
    auto bytes = std::as_writable_bytes(std::span{&object, 1});
    regions.push_back({address, bytes, {bytes.begin(), bytes.end()}});
}
template<class T,std::size_t N> void add(std::vector<Region>&,std::uint32_t,std::span<T,N>)=delete;
template<class T> concept ObjectCapturable=requires(std::vector<Region>& r,T& value){add(r,0u,value);};
static_assert(!ObjectCapturable<std::span<std::uint8_t,92>> && ObjectCapturable<std::array<std::uint8_t,92>>);
void add_payload(std::vector<Region>& r,std::uint32_t address,std::span<std::uint8_t> payload) {
    const auto b=std::as_writable_bytes(payload);r.push_back({address,b,{b.begin(),b.end()}});
}
std::vector<Region> capture(FrontendSourceStorage& f, FrontendLaunchState& w) {
    std::vector<Region> r;
    add(r, 0x5da2e8, f.menu_words_5da2e8);
    add(r, 0x5da360, f.frontend_clip);
    add(r, 0x5da370, f.menu_counter_5da370); add(r, 0x5df194, f.menu_words_5df194);
    add(r, 0x5df560, f.persistence.bytes); add(r, 0x5e167c, f.cold_suffix);
    add(r, 0x625938, f.read_scratch.bytes); add(r, 0x5fd148, f.csv_token_scratch);
    add(r, 0x47966c, f.initials_scratch); add(r, 0x5ff328, f.numeric_scratch);
    add(r, 0x5fd258, f.page_pool); add_payload(r, 0x5ff3e8, f.page_navigation());
    add(r, 0x5ff4ac, f.menu_music_word()); add(r, 0x61b9c8, f.challenge_text);
    add(r, 0x5ff8c0, f.feedback_gate);
    add(r, 0x61bdcc, f.campaign_text); add(r, 0x5ff8c4, f.language_scratch);
    add(r, 0x5ffcc8, f.language_arena); add(r, 0x605cc8, f.language_text_handles);
    add(r, 0x606838, f.language_names); add(r, 0x606860, f.language_descriptors);
    add(r, 0x47b1a0, f.language_remaining); add(r, 0x6087a0, f.language_input_identity);
    add(r, 0x6087a4, f.language_empty_sentinel); add(r, 0x60d61c, f.scene.menu_initialized);
    add(r, 0x5df528, f.race_return_selector); add(r, 0x5fd248, f.menu_state);
    add(r, 0x5fd250, f.initializing); add(r, 0x5fd254, f.initialization_count);
    add(r, 0x6087a8, f.lookup_a); add(r, 0x60883c, f.lookup_b); add(r, 0x608cf0, f.lookup_c);
    add(r, 0x608d88, f.frontend_descriptors);
    add(r, 0x61bbe0, f.campaign_launch); add(r, 0x61bed0, f.campaign_result);
    //Separate legacy regions remain92+4 bytes; gaps are now owned and checked.
    add_payload(r,0x5ff444,f.source_bytes(0x5ff444,104));
    add_payload(r,0x5ff4b0,f.source_bytes(0x5ff4b0,8));
    add(r, 0x60d744, w.setting_60d744); add(r, 0x60d748, w.track_index);
    add(r, 0x60df6c, w.prepared); add(r, 0x60df90, w.catalog_count);
    for (std::uint32_t i = 0; i < 4; ++i) add(r, 0x62d598 + i * 0x1dc, w.input_bindings[i]);
    return r;
}
void seed(FrontendSourceStorage& f, FrontendLaunchState& w, std::uint32_t number) {
    auto random = number ^ 0xa194b317u;
    for (auto& region : capture(f, w)) for (auto& b : region.actual) {
        random ^= random << 13; random ^= random >> 17; random ^= random << 5;
        b = static_cast<std::byte>(random & 255u);
    }
    f.bind_campaign_return();
}
Region& region_at(std::vector<Region>& regions, std::uint32_t address) {
    for (auto& r : regions) if (address >= r.address && std::size_t(address - r.address) < r.actual.size()) return r;
    throw std::runtime_error("test address has no tracked owner");
}
void expected_word(std::vector<Region>& regions, std::uint32_t address, std::uint32_t value) {
    auto& r = region_at(regions, address);
    const auto offset = address - r.address;
    require(offset + 4 <= r.expected.size(), "expected word crosses region");
    for (unsigned i = 0; i < 4; ++i) r.expected[offset + i] = static_cast<std::byte>((value >> (8 * i)) & 255u);
}
void expected_clear(std::vector<Region>& regions, std::uint32_t address) {
    auto& r = region_at(regions, address);
    std::fill(r.expected.begin(), r.expected.end(), std::byte{0});
}
// Composition check only; child arithmetic is tested independently below
// and the original-execution harness remains pending Main execution.
void expected_descriptor_composition(std::vector<Region>& regions) {
    std::array<std::uint8_t,frontend_descriptor_bytes> child{};
    initialize_frontend_descriptors_441340(child);
    auto& target=region_at(regions,0x608d88);
    std::transform(child.begin(),child.end(),target.expected.begin(),[](auto value){return static_cast<std::byte>(value);});
    // Independently pinned4411C0 literal fills and actual437E78 input words.
    for(unsigned i=0;i<37;++i)expected_word(regions,0x6087a8+i*4,0xffffffffu);
    for(unsigned i=0;i<301;++i)expected_word(regions,0x60883c+i*4,0xffffffffu);
    for(unsigned i=0;i<37;++i)expected_word(regions,0x608cf0+i*4,0x10000u);
    expected_word(regions,0x5da360,0);expected_word(regions,0x5da364,0);
    expected_word(regions,0x5da368,640);expected_word(regions,0x5da36c,480);
}
void compare(const std::vector<Region>& regions) {
    for (const auto& r : regions) {
        compared += r.expected.size();
        if (!std::equal(r.actual.begin(), r.actual.end(), r.expected.begin()))
            throw std::runtime_error("state mismatch at source region " + std::to_string(r.address));
        ++checks;
    }
}
// Independently authored end-state specification: every persistence byte is
// zero except these five nonzero words. This is not an original execution oracle.
void expected_defaults(std::vector<Region>& regions, std::uint32_t catalog) {
    expected_clear(regions, 0x5df560);
    expected_word(regions, 0x5df5d8, 1); expected_word(regions, 0x5df5dc, 80);
    expected_word(regions, 0x5df5e0, 100); expected_word(regions, 0x5df5e4, 100);
    expected_word(regions, 0x5df5f4, 2);
    expected_word(regions, 0x5f26a8, catalog); expected_word(regions, 0x5e37a0, 1);
}
void binding_checks(FrontendSourceStorage& f, PersistenceView before, bool cleared) {
    const auto after = f.persistence_view();
    require(before.live.data() == after.live.data() && after.live.data() == f.persistence.bytes.data(), "live bank alias");
    require(before.saved_body.data() == after.saved_body.data(), "saved body retained alias");
    require(before.scratch.data() == after.scratch.data(), "scratch retained alias");
    require(after.saved_body.data() == f.source_bytes(0x5e167c, persistence_body_size).data(), "saved body physical owner");
    require(f.source_bytes(0x5fd258, 0x20d0).data() == f.page_pool.data(), "page pool physical owner");
    for (auto address : {0x5f26a8u, 0x5e37a0u}) {
        std::uint32_t value{};
        auto& typed = f.source_word(address);
        const auto bytes = f.source_bytes(address, 4);
        std::memcpy(&value, bytes.data(), 4);
        require(value == typed && bytes.data() == reinterpret_cast<std::uint8_t*>(&typed), "suffix word and byte alias");
    }
    if (cleared) {
        require(f.race_return_bindings[5].roster.data() == nullptr && f.race_return_bindings[5].roster.empty(), "source null roster after439E30");
        require(f.race_return_bindings[5].result.data() == nullptr && f.race_return_bindings[5].result.empty(), "source null result after439E30");
    } else {
        require(f.race_return_bindings[5].roster.data() == reinterpret_cast<std::uint8_t*>(f.campaign_launch.data()), "campaign roster retained alias");
        require(f.race_return_bindings[5].result.data() == reinterpret_cast<std::uint8_t*>(f.campaign_result.data()), "campaign result retained alias");
    }
}
unsigned authored() {
    constexpr std::array counts{0u, 1u, 17u, 0x7fffffffu, 0x80000000u, 0xfffffffdu, 0xfffffffeu, 0xffffffffu};
    constexpr std::array catalogs{0u, 1u, 0x5096c0u, 0xffffffffu};
    unsigned cases = 0;
    for (const auto count : counts) for (const auto catalog : catalogs) for (const bool prefix : {false, true}) {
        auto frontend = std::make_unique<FrontendSourceStorage>(); auto& f = *frontend;
        FrontendLaunchState w;
        seed(f, w, cases + 1);
        f.initialization_count = count;
        w.catalog_count = catalog;
        // Re-enter on the same owners. A second call must re-read the current
        // world word, clear changed saves, and increment again even after stop.
        for (unsigned pass = 0; pass < 2; ++pass) {
            w.catalog_count = catalog ^ (pass ? 0x97b4012du : 0u);
            f.persistence.bytes.back() = 0xed;
            f.source_word(0x5e3794) = 0x617593bdu;
            f.bind_campaign_return(); //source launch can republish after a prior clear
            auto expected = capture(f, w);
            const auto view = f.persistence_view();
            const auto bindings = f.race_return_bindings;
            if (prefix) {
                expected_clear(expected, 0x5e167c);
                expected_clear(expected, 0x5da2e8);
                expected_clear(expected,0x5ff3e8);expected_clear(expected,0x5ff444);
                expected_word(expected,0x5ff4ac,0);expected_clear(expected,0x5ff4b0);
                expected_word(expected,0x5ff3e8,1);expected_word(expected,0x5ff3ec,0x10000);
                expected_word(expected,0x5ff444,1);
                expected_descriptor_composition(expected);
                expected_clear(expected, 0x5df194); expected_word(expected, 0x5da370, 0);
                expected_word(expected, 0x5fd248, 0); expected_word(expected, 0x5fd250, 1);
                expected_word(expected, 0x5fd254, count + pass + 1);
                expected_word(expected, 0x5df528, 0);
                expected_word(expected, 0x5ff8c0, 1);
                expected_word(expected, 0x5ff2d8, 16); expected_word(expected, 0x5ff2dc, 0);
                for (unsigned i = 0; i < 16; ++i) expected_word(expected, 0x5ff2e0 + i * 4, 0x5fd258 + i * 0x208);
            }
            expected_defaults(expected, w.catalog_count);
            if (prefix) {
                const auto stop = initialize_frontend_prefix_43d0b0(f, w);
                require(stop == FrontendInitializationStop::before_440ae0, "missing child boundary retained");
                require(frontend_initialization_parent_callsite == 0x43d100 && frontend_initialization_stop_callsite == 0x43d10f, "outer source boundary callsites");
                require(f.initializing == 1, "prefix must not report complete initialization");
            } else initialize_frontend_defaults_43c060(f, w);
            compare(expected);
            // Nonzero defaults surviving a cold clear detect reversed
            // clear/default ordering; all bytes and unrelated fields checked.
            require(persistence_word(view.live, 0x80) == 100, "defaults must follow cold clear");
            binding_checks(f, view, prefix);
            for (std::size_t i = 0; i < bindings.size(); ++i) {
                if (prefix && i != 0 && i != 2) {
                    require(f.race_return_bindings[i].roster.data() == nullptr && f.race_return_bindings[i].roster.empty(), "cleared source roster view");
                    require(f.race_return_bindings[i].result.data() == nullptr && f.race_return_bindings[i].result.empty(), "cleared source result view");
                } else {
                    require(bindings[i].roster.data() == f.race_return_bindings[i].roster.data() &&
                        bindings[i].roster.size() == f.race_return_bindings[i].roster.size(), "retained roster view");
                    require(bindings[i].result.data() == f.race_return_bindings[i].result.data() &&
                        bindings[i].result.size() == f.race_return_bindings[i].result.size(), "retained result view");
                }
            }
            ++cases;
        }
    }
    return cases;
}
unsigned authored_return_clear() {
    unsigned calls = 0;
    for (unsigned pattern = 0; pattern < 64; ++pattern) {
        auto frontend = std::make_unique<FrontendSourceStorage>(); auto& f = *frontend;
        FrontendLaunchState w; seed(f, w, pattern + 0x439e30u);
        std::array<std::array<std::uint8_t, 64>, 8> rosters{}, results{};
        for (unsigned i = 0; i < 8; ++i) {
            rosters[i].fill(static_cast<std::uint8_t>(pattern + i));
            results[i].fill(static_cast<std::uint8_t>(pattern ^ (i + 0x93u)));
        }
        const auto old_rosters = rosters, old_results = results;
        for (unsigned pass = 0; pass < 2; ++pass) {
            // Include null, normal, interior and nonnull zero-length views.
            // In particular .empty() alone does not mean a null source pointer.
            for (unsigned i = 0; i < 8; ++i) {
                switch ((pattern + i + pass) % 4) {
                case 0: f.race_return_bindings[i] = {}; break;
                case 1: f.race_return_bindings[i] = {rosters[i], results[i]}; break;
                case 2: f.race_return_bindings[i] = {{rosters[i].data(), 0}, {results[i].data(), 0}}; break;
                case 3: f.race_return_bindings[i] = {{rosters[i].data() + 3, 17}, {results[i].data() + 5, 29}}; break;
                }
            }
            f.race_return_selector = 0xfffffff0u + pattern;
            auto expected = capture(f, w); expected_word(expected, 0x5df528, 0);
            const auto bindings = f.race_return_bindings;
            clear_frontend_race_return_439e30(f);
            compare(expected);
            require(f.race_return_selector == 0, "first cleared source word");
            for (unsigned i = 0; i < 8; ++i) {
                const auto& b = f.race_return_bindings[i];
                if (i == 0 || i == 2) {
                    require(b.roster.data() == bindings[i].roster.data() && b.roster.size() == bindings[i].roster.size(), "host-only roster slot preserved");
                    require(b.result.data() == bindings[i].result.data() && b.result.size() == bindings[i].result.size(), "host-only result slot preserved");
                } else {
                    require(b.roster.data() == nullptr && b.roster.size() == 0, "source roster word must be null");
                    require(b.result.data() == nullptr && b.result.size() == 0, "source result word must be null");
                }
            }
            require(rosters == old_rosters && results == old_results, "439E30 must not clear pointed-to data");
            compared += sizeof(rosters) + sizeof(results);
            ++calls;
        }
    }
    return calls;
}
unsigned authored_feedback_gate() {
    unsigned calls = 0;
    for (const auto value : {0u, 1u, 2u, 0x7fffffffu, 0x80000000u, 0x80000001u, 0xfffffffeu, 0xffffffffu}) {
        for (unsigned op = 0; op < 3; ++op) {
            auto frontend = std::make_unique<FrontendSourceStorage>(); auto& f = *frontend;
            FrontendLaunchState w; seed(f, w, value ^ (op + 0x4408e0u));
            auto& word = f.source_word(0x5ff8c0);
            const auto view = f.source_bytes(0x5ff8c0, 4);
            require(&word == &f.feedback_gate && view.data() == reinterpret_cast<std::uint8_t*>(&word), "gate single typed/byte owner");
            word = value;
            auto expected = capture(f, w);
            const auto wanted = op == 0 ? 1u : static_cast<std::uint32_t>((std::uint64_t{value} + (op == 1 ? 1u : 0xffffffffu)) & 0xffffffffu);
            expected_word(expected, 0x5ff8c0, wanted);
            if (op == 0) initialize_frontend_feedback_gate_4408e0(f);
            else if (op == 1) increment_frontend_feedback_gate_440900(f);
            else decrement_frontend_feedback_gate_440910(f);
            compare(expected);
            require(word == wanted && persistence_word(view, 0) == wanted, "gate aliases observe exact modulo32 bits");
            require(f.source_bytes(0x5ff8c1, 3).data() == view.data() + 1, "gate partial byte view");
            bool rejected = false;
            try { (void)f.source_bytes(0x5ff8c0, 8); } catch (const sgn::FormatError&) { rejected = true; }
            require(rejected, "gate view must not cross into language scratch");
            rejected = false;
            try { (void)f.source_word(0x5ff8c1); } catch (const sgn::FormatError&) { rejected = true; }
            require(rejected, "gate misaligned word rejection");
            ++calls;
        }
    }
    return calls;
}
unsigned authored_draw_lists() {
    unsigned calls = 0;
    for (unsigned pattern = 0; pattern < 32; ++pattern) for (const bool parent : {false, true}) {
        auto frontend = std::make_unique<FrontendSourceStorage>(); auto& f = *frontend;
        FrontendLaunchState w; seed(f, w, pattern + 0x437e80u);
        auto& counter = f.source_word(0x5da370);
        require(&counter == &f.menu_counter_5da370, "draw counter typed owner");
        require(f.source_bytes(0x5da370, 4).data() == reinterpret_cast<std::uint8_t*>(&counter), "draw counter byte owner");
        constexpr std::array counter_bits{0u, 1u, 79u, 80u, 0x7fffffffu, 0x80000000u, 0xfffffffeu, 0xffffffffu};
        counter = counter_bits[pattern % counter_bits.size()];
        const auto all = f.source_bytes(0x5df194, 640);
        const auto first = f.source_bytes(0x5df194, 320), second = f.source_bytes(0x5df2d4, 320);
        require(all.data() == reinterpret_cast<std::uint8_t*>(f.menu_words_5df194.data()), "draw words actual bank");
        require(first.data() == all.data() && second.data() == all.data() + 320, "adjacent draw halves share one bank");
        require(f.source_bytes(0x5df2d0, 8).data() == all.data() + 316, "legal span across same-owner midpoint");
        for (unsigned i = 0; i < 160; ++i) {
            auto& word = f.source_word(0x5df194 + i * 4);
            require(&word == &f.menu_words_5df194[i], "draw word exact typed alias");
            word = pattern == 0 ? 0u : pattern == 1 ? 0xffffffffu : (0x835aa17du ^ (pattern * 167u + i * 0x9183u));
            require(persistence_word(all, i * 4) == word, "draw bytes reflect typed writes");
        }
        for (auto [address, size] : std::array<std::pair<unsigned, unsigned>, 5>{{
                 {0x5da36c, 8}, {0x5da370, 8}, {0x5df190, 8}, {0x5df410, 8}, {0x5df414, 1}}}) {
            bool rejected = false;
            try { (void)f.source_bytes(address, size); } catch (const sgn::FormatError&) { rejected = true; }
            require(rejected, "draw byte view outside actual owner");
        }
        for (const auto address : {0x5da374u, 0x5df190u, 0x5df414u, 0x5df195u}) {
            bool rejected = false;
            try { (void)f.source_word(address); } catch (const sgn::FormatError&) { rejected = true; }
            require(rejected, "draw typed view gap/end/alignment rejection");
        }
        auto expected = capture(f, w);
        // Every tracked source interval is disjoint; neither new range aliases
        // a current frontend bank or one of the captured world-owned words.
        for (std::size_t i = 0; i < expected.size(); ++i) for (std::size_t j = i + 1; j < expected.size(); ++j) {
            const auto& a = expected[i]; const auto& b = expected[j];
            require(std::uint64_t(a.address) + a.actual.size() <= b.address ||
                    std::uint64_t(b.address) + b.actual.size() <= a.address, "source owner interval overlap");
        }
        expected_word(expected, 0x5da370, 0); expected_clear(expected, 0x5df194);
        const auto bindings = f.race_return_bindings; const auto persistence = f.persistence_view();
        if (parent) {
            expected_descriptor_composition(expected);
            initialize_frontend_drawing_437e60(f);
            require(frontend_initialization_after_drawing_callsite == 0x43d105, "drawing returns before436EB0");
        } else clear_frontend_draw_lists_437e80(f);
        compare(expected); binding_checks(f, persistence, false);
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            require(bindings[i].roster.data() == f.race_return_bindings[i].roster.data() &&
                    bindings[i].roster.size() == f.race_return_bindings[i].roster.size(), "draw clear retains source roster view");
            require(bindings[i].result.data() == f.race_return_bindings[i].result.data() &&
                    bindings[i].result.size() == f.race_return_bindings[i].result.size(), "draw clear retains source result view");
        }
        require(counter == 0 && std::all_of(all.begin(), all.end(), [](auto b) { return b == 0; }), "retained aliases see full clear");
        ++calls;
    }
    return calls;
}
unsigned authored_word_clear_436eb0() {
    unsigned calls=0;
    for(unsigned pattern=0;pattern<72;++pattern) {
        auto frontend=std::make_unique<FrontendSourceStorage>();auto& f=*frontend;
        FrontendLaunchState w;seed(f,w,pattern+0x436eb0);
        const auto all=f.source_bytes(0x5da2e8,120);
        require(all.data()==reinterpret_cast<std::uint8_t*>(f.menu_words_5da2e8.data()),"436EB0 sole byte owner");
        require(f.source_bytes(0x5da2eb,113).data()==all.data()+3,"436EB0 unaligned subview");
        for(unsigned i=0;i<30;++i) {
            auto& word=f.source_word(0x5da2e8+i*4);
            require(&word==&f.menu_words_5da2e8[i],"436EB0 typed alias");
            const auto value=pattern==0?0u:pattern==1?0xffffffffu:pattern==2?0xaaaaaaaau:pattern==3?0x55555555u:
                pattern<36?(1u<<(pattern-4)):pattern<68?~(1u<<(pattern-36)):(0x35afcd91u^(pattern*1597u+i*9127u));
            word=value;require(persistence_word(all,i*4)==value,"436EB0 byte view sees typed writes");
        }
        for(auto [address,size]:std::array<std::pair<unsigned,unsigned>,5>{{
            {0x5da2e4,8},{0x5da2e7,1},{0x5da2e8,121},{0x5da35c,8},{0x5da358,12}}}) {
            bool rejected=false;try{(void)f.source_bytes(address,size);}catch(const sgn::FormatError&){rejected=true;}
            require(rejected,"436EB0 gap or crossing into separately owned clip rejected");
        }
        for(auto address:{0x5da2e4u,0x5da2e9u,0x5da35du}) {
            bool rejected=false;try{(void)f.source_word(address);}catch(const sgn::FormatError&){rejected=true;}
            require(rejected,"436EB0 typed gap/alignment rejected");
        }
        const auto view=f.persistence_view();const auto bindings=f.race_return_bindings;
        for(unsigned pass=0;pass<2;++pass) {
            if(pass) {f.menu_words_5da2e8.front()=0xffffffffu;f.menu_words_5da2e8.back()=0x80000000u;}
            auto expected=capture(f,w);expected_clear(expected,0x5da2e8);
            clear_frontend_words_436eb0(f);compare(expected);binding_checks(f,view,false);
            for(unsigned i=0;i<30;++i)require(persistence_word(all,i*4)==0,"all30 words cleared, including both boundaries");
            for(std::size_t i=0;i<bindings.size();++i) {
                require(bindings[i].roster.data()==f.race_return_bindings[i].roster.data()&&bindings[i].roster.size()==f.race_return_bindings[i].roster.size(),"436EB0 retains roster borrow");
                require(bindings[i].result.data()==f.race_return_bindings[i].result.data()&&bindings[i].result.size()==f.race_return_bindings[i].result.size(),"436EB0 retains result borrow");
            }
            ++calls;
        }
    }
    return calls;
}
unsigned authored_navigation_initialization() {
    unsigned calls=0;
    for(unsigned pattern=0;pattern<72;++pattern)for(bool full:{false,true}) {
        auto frontend=std::make_unique<FrontendSourceStorage>();auto& f=*frontend;
        FrontendLaunchState w;seed(f,w,pattern+0x43dda0);
        for(unsigned i=0;i<52;++i)f.navigation_words[i]=pattern==0?0u:pattern==1?0xffffffffu:
            pattern<34?(1u<<(pattern-2)):pattern<66?~(1u<<(pattern-34)):(0x349acfd1u^(pattern*43+i*53087u));
        const auto prefix=f.page_navigation();auto& music=f.menu_music_word();
        const auto bindings=f.race_return_bindings;const auto persistence=f.persistence_view();
        for(unsigned pass=0;pass<2;++pass){
            if(pass){f.navigation_words.front()=0xa918b423;f.navigation_words.back()=0x80acd3ef;music=0x921bcafe;f.navigation_words[34]=0xffffffff;}
            auto expected=capture(f,w);
            if(full){
                expected_clear(expected,0x5ff3e8);expected_clear(expected,0x5ff444);expected_word(expected,0x5ff4ac,0);expected_clear(expected,0x5ff4b0);
                expected_word(expected,0x5ff3e8,1);expected_word(expected,0x5ff3ec,0x10000);expected_word(expected,0x5ff444,1);
                initialize_frontend_navigation_43dda0(f);
            }else{
                for(unsigned address=0x5ff470;address<=0x5ff49c;address+=4)expected_word(expected,address,0);
                clear_frontend_navigation_tail_43dee0(f);
            }
            compare(expected);binding_checks(f,persistence,false);
            require(f.page_navigation().data()==prefix.data()&&&music==&f.navigation_words[49],"navigation borrows survive repeat initialization");
            for(std::size_t i=0;i<bindings.size();++i){
                require(f.race_return_bindings[i].roster.data()==bindings[i].roster.data()&&f.race_return_bindings[i].roster.size()==bindings[i].roster.size(),"navigation retains roster borrows");
                require(f.race_return_bindings[i].result.data()==bindings[i].result.data()&&f.race_return_bindings[i].result.size()==bindings[i].result.size(),"navigation retains result borrows");
            }
            ++calls;
        }
    }
    return calls;
}
unsigned immutable_cold(const char* path) {
    std::ifstream in(path, std::ios::binary);
    require(bool(in), "immutable frontend corpus missing");
    const auto read = [&](void* value, std::size_t size) {
        if (!in.read(static_cast<char*>(value), static_cast<std::streamsize>(size))) throw std::runtime_error("truncated frontend corpus");
    };
    const auto word = [&] { std::uint32_t value; read(&value, 4); return value; };
    std::array<char, 8> schema{}; read(schema.data(), schema.size());
    require(std::string(schema.data(), schema.size()) == "SGNFS001", "immutable frontend schema");
    const auto cases = word(); require(cases == 24, "immutable frontend case count");
    for (std::uint32_t i = 0; i < cases; ++i) {
        auto frontend = std::make_unique<FrontendSourceStorage>(); auto& f = *frontend;
        FrontendLaunchState w; seed(f, w, i + 1);
        std::uint32_t audio_pause{};
        auto tracked = capture(f, w); add(tracked, 0x4d5b20, audio_pause);
        const auto op = word(), regions = word();
        require(op == i % 3 && regions == 22, "immutable frontend operation/region schema");
        for (std::uint32_t j = 0; j < regions; ++j) {
            const auto address = word(), size = word(); auto& r = region_at(tracked, address);
            require(r.address == address && r.actual.size() == size, "immutable frontend owner layout");
            read(r.actual.data(), size); read(r.expected.data(), size);
        }
        if (op == 0) clear_frontend_cold_storage(f);
        else if (op == 1) initialize_frontend_lookup_tables(f);
        else return_to_frontend(f);
        compare(tracked);
    }
    require(in.peek() == std::char_traits<char>::eof(), "immutable frontend trailing bytes");
    return cases;
}
}
int main(int argc, char** argv) try {
    static_assert(!std::is_copy_constructible_v<FrontendSourceStorage> && !std::is_move_constructible_v<FrontendSourceStorage>);
    if (argc > 2) return 2;
    const auto cases = authored();
    const auto return_clear = authored_return_clear();
    const auto feedback_gate = authored_feedback_gate();
    const auto draw_lists = authored_draw_lists();
    const auto word_clear = authored_word_clear_436eb0();
    const auto navigation_calls = authored_navigation_initialization();
    const auto oracle = argc == 2 ? immutable_cold(argv[1]) : 0;
    std::cout << "{\"passed\":true,\"authored_calls\":" << cases << ",\"authored_return_clear_calls\":" << return_clear
              << ",\"authored_feedback_gate_calls\":" << feedback_gate << ",\"authored_draw_list_calls\":" << draw_lists << ",\"existing_oracle_cases\":" << oracle
              << ",\"authored_word_clear_calls\":" << word_clear << ",\"authored_navigation_calls\":" << navigation_calls << ",\"checks\":" << checks << ",\"compared_bytes\":" << compared
              << ",\"full_prefix_original_oracle\":false,\"parent_callsite\":\"43d100\",\"stop_callsite\":\"43d10f\",\"missing_child\":\"440ae0\"}\n";
    return 0;
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
