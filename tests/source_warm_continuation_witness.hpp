#pragma once
#include "source_warm_allocation_witness.hpp"
#include "source_allocation_begin_witness.hpp"
#include "source_scene_reset_witness.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_frontend_page_pool.hpp"
#include <cstring>
#include <optional>
#include <vector>
#include <iostream>

namespace warm_release_receiving {
// Main-only receiving stimulus in the existing frontend owner. The loose
// retail WAD is opaque resource payload here: no catalog parser/startup claim.
class RetainedFrontendWitness final {
    sgn::retail::FrontendState& frontend_;
    const sgn::retail::SourceFrontendResourceMemory* identity_;
    sgn::retail::SourceFrontendResourceRef first_,second_;
    sgn::retail::SourceFrontendResourceInfo first_info_,second_info_;
    std::vector<std::uint8_t> first_bytes_,second_bytes_,source_bytes_;
    std::array<char,128> base_path_{};
    std::array<std::uint32_t,2> page_handles_{};
    std::array<std::vector<std::byte>,2> pages_;
    bool attached_;
    static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
    auto pool(){return std::as_writable_bytes(std::span(frontend_.source.page_pool));}
    static bool same(const sgn::retail::SourceFrontendResourceInfo& a,const sgn::retail::SourceFrontendResourceInfo& b){
        return a.identity==b.identity&&a.payload_bytes==b.payload_bytes&&a.allocation_bytes==b.allocation_bytes&&
            a.native_address==b.native_address&&a.initialized_prefix==b.initialized_prefix&&
            a.terminator_written==b.terminator_written&&a.loader_returned==b.loader_returned&&a.read_return==b.read_return;
    }
public:
    explicit RetainedFrontendWitness(sgn::retail::FrontendState& f):frontend_(f),identity_(&f.resource_memory()),
        attached_(f.shared_platform()->session_attachment_claimed()){
        auto& memory=f.resource_memory();check(memory.live_allocations()==0,"warm frontend fixture expects fresh retained owner");
        // Explicit receiving path prerequisite in the existing boot owner.
        // SourceBootConfigState retains the PE D:\Game\ spelling until boot configuration.
        auto& base=f.shared_platform()->state().boot_config.base_path;
        constexpr char pe_root[]="D:\\Game\\";
        check(std::equal(std::begin(pe_root),std::end(pe_root),base.begin()),"warm frontend fixture expects the retained PE boot root");
        base[0]='.';base[1]=0;base_path_=base;
        sgn::retail::NativeSourceBootFilePlatform files(f.shared_platform()->retail_root());
        first_=memory.load_43c7f0(files,"wads\\config.wad");check(bool(first_),"retail retained resource missing");
        memory.publish_440b26(first_);
        second_=memory.load_43c7f0(files,"wads\\config.wad");check(bool(second_)&&second_!=first_,"retained resource replacement identity");
        memory.publish_440b26(second_);check(memory.live_allocations()==2,"replacement unexpectedly freed predecessor");
        first_info_=memory.inspect(first_);second_info_=memory.inspect(second_);
        const auto a=memory.initialized_prefix(first_),b=memory.initialized_prefix(second_);
        first_bytes_.assign(a.begin(),a.end());second_bytes_.assign(b.begin(),b.end());
        check(!first_bytes_.empty()&&first_bytes_==second_bytes_,"retail resource reads differ");
        sgn::retail::initialize_frontend_page_pool_43d430(pool());
        for(std::size_t i=0;i<page_handles_.size();++i){
            const auto size=i==0?32u:511u;page_handles_[i]=sgn::retail::allocate_frontend_page_object_43d470(pool(),size);
            auto bytes=sgn::retail::frontend_page_object_bytes(pool(),page_handles_[i],size);
            std::fill(bytes.begin(),bytes.end(),std::byte{static_cast<unsigned char>(0x51u+i)});pages_[i].assign(bytes.begin(),bytes.end());
        }
        const auto* raw=reinterpret_cast<const std::uint8_t*>(&f.source);source_bytes_.assign(raw,raw+sizeof(f.source));
    }
    void verify(){
        check(frontend_.shared_platform()->state().boot_config.base_path==base_path_,"warm reset changed explicit receiving root spelling");
        auto& memory=frontend_.resource_memory();check(&memory==identity_&&memory.live_allocations()==2,"warm reset replaced retained resource owner");
        check(memory.current_reference()==second_&&same(memory.inspect(first_),first_info_)&&same(memory.inspect(second_),second_info_),
            "warm reset changed resource identity, count or allocation address");
        const auto a=memory.initialized_prefix(first_),b=memory.initialized_prefix(second_);
        check(std::equal(a.begin(),a.end(),first_bytes_.begin(),first_bytes_.end())&&
            std::equal(b.begin(),b.end(),second_bytes_.begin(),second_bytes_.end()),"warm reset changed retained resource payload");
        check(std::memcmp(source_bytes_.data(),&frontend_.source,source_bytes_.size())==0,"warm reset changed frontend raw/page/catalog owner");
        for(std::size_t i=0;i<page_handles_.size();++i){const auto bytes=sgn::retail::frontend_page_object_bytes(pool(),page_handles_[i],pages_[i].size());
            check(std::equal(bytes.begin(),bytes.end(),pages_[i].begin(),pages_[i].end()),"warm reset invalidated live frontend page object");}
        check(frontend_.shared_platform()->session_attachment_claimed()==attached_,"warm reset changed Session attachment reservation");
    }
};

// This observer sits on the existing external counter provider. It never
// replaces a game child. At reset read1 the actual42EAA0 and445FA0 prefix have
// returned. Clock/rail functions do not access the bank; the snapshot therefore
// also describes bank entry at42EA80. No exact call-site breakpoint is claimed.
class ResetContinuationWitness final {
    sgn::retail::RaceSessionPlatform& platform_;
    const AllocationWitness& released_;
    RetainedFrontendWitness retained_;
    bool expected_;
    unsigned calls_{};
    std::uint32_t increment_;
    std::optional<source_scene_reset_receiving::SceneResetWitness> prefix_;
    std::optional<allocation_begin_receiving::BeginWitness> begin_;
    static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
public:
    ResetContinuationWitness(sgn::retail::RaceSession& s,sgn::retail::FrontendState& f,const AllocationWitness& released,bool expected):
        platform_(*s.shared_platform()),released_(released),retained_(f),expected_(expected),increment_(s.state().tick_clock.increment){
        if(expected_)prefix_.emplace(s.state(),true);
    }
    std::uint64_t counter(unsigned ordinal,std::uint64_t ordinary){
        check(ordinal==++calls_,"warm continuation skipped or repeated counter request");
        if(ordinal==1)return ordinary;
        check(expected_&&(ordinal==2||ordinal==3),"unreached clock reset consumed counter");
        auto& s=platform_.state();(void)prefix_->verify();released_.released();retained_.verify();
        check(s.tick_clock.increment==increment_&&s.tick_clock.observed==static_cast<std::uint32_t>(ordinary),"reset changed increment or observed too early");
        check(s.tick_clock.scheduled==(ordinal==2?static_cast<std::uint32_t>(ordinary):117u),"reset did not store first sample before next read");
        if(ordinal==2)begin_.emplace(platform_);
        return ordinal==2?0x1234567800000075ull:0xabcdef010000007bull;
    }
    void finished(){
        retained_.verify();check(calls_==(expected_?3u:1u),"wrong actual warm counter count");
        if(expected_){
            check(begin_.has_value(),"actual reset counter boundary was not observed");const auto checks=begin_->verify();
            const auto& clock=platform_.state().tick_clock;
            check(clock.scheduled==117u&&clock.observed==123u&&clock.increment==increment_,"actual warm clock reset results");
            std::cout<<"warm_continuation actual_reset_reads=2 scheduled=117 observed=123 bank_checks="<<checks
                <<" retained_resources=2 retained_pages=2 next_child=42E6E0\n";
        }else released_.released();
    }
};
}
