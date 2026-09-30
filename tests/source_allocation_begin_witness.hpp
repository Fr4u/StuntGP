#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <algorithm>
#include <array>
#include <vector>

namespace allocation_begin_receiving {
using Words=std::array<std::uint32_t,7>;
inline Words words(const sgn::retail::SourceAllocationBank& bank){
    const auto& w=bank.source_words();
    return {w.active,w.ordinary_bytes,w.persistent_bytes,w.peak_bytes,w.next_ordinal,w.release_base,w.release_persistent};
}
inline bool same(const sgn::retail::SourceAllocationRecord& a,const sgn::retail::SourceAllocationRecord& b){
    return a.aligned==b.aligned&&a.raw==b.raw&&a.reserved_bytes==b.reserved_bytes&&
        a.requested_bytes==b.requested_bytes&&a.category==b.category&&a.ordinal==b.ordinal;
}
// Read-only observer around ONE real42EA80 call. It never calls a source
// child, alters a record, creates a bank or retries a stopped transition.
// Main may construct it at actual Warm begin entry and verify at42E6E0.
class BeginWitness final {
    sgn::retail::RaceSessionPlatform& platform_;
    const sgn::retail::SourceAllocationBank* identity_;
    Words before_;
    std::array<sgn::retail::SourceAllocationRecord,sgn::retail::SourceAllocationBank::capacity> rows_;
    std::array<std::vector<std::uint8_t>,sgn::retail::SourceAllocationBank::capacity> payloads_;
public:
    explicit BeginWitness(sgn::retail::RaceSessionPlatform& p):
        platform_(p),identity_(&p.source_allocations()),before_(words(*identity_)){
        if(!identity_->initialized())throw std::runtime_error("begin witness requires explicit prior boot");
        std::ranges::copy(identity_->source_records(),rows_.begin());
        for(std::size_t i=0;i<rows_.size();++i)if(rows_[i].raw!=0){
            const auto* data=reinterpret_cast<const std::uint8_t*>(rows_[i].aligned);
            payloads_[i].assign(data,data+rows_[i].requested_bytes);
        }
    }
    unsigned verify() const{
        unsigned checks=0;
        const auto check=[&](bool ok,const char* reason){++checks;if(!ok)throw std::runtime_error(reason);};
        const auto& bank=platform_.source_allocations();
        check(&bank==identity_,"42EA80 replaced the retained bank");
        auto expected=before_;expected[0]=1;expected[4]=0;expected[5]=0;
        const auto after=bank.source_records();
        std::size_t live=0;
        for(std::size_t i=0;i<rows_.size();++i){
            const auto& prior=rows_[i];
            // Independent closed window membership; does not reproduce the
            // implementation's reverse traversal. uint32 subtraction wraps.
            const bool released=before_[0]==1&&prior.aligned!=0&&
                static_cast<std::uint32_t>(prior.ordinal-before_[5])<384u&&
                (prior.category!=1||before_[6]!=0);
            if(released){
                check(same(after[i],{}),"42EA80 did not clear all six released row fields");
                expected[prior.category==1?2u:1u]-=prior.reserved_bytes;
            }else{
                check(same(after[i],prior),"42EA80 altered a retained or empty row");
                if(prior.raw!=0){
                    ++live;
                    const auto* data=reinterpret_cast<const std::uint8_t*>(after[i].aligned);
                    check(std::equal(payloads_[i].begin(),payloads_[i].end(),data),"42EA80 changed retained payload bytes");
                }
            }
        }
        check(words(bank)==expected,"42EA80 wrote incorrect source accounting/control words");
        check(bank.live_count()==live,"42EA80 live ownership count differs");
        return checks;
    }
};
}
