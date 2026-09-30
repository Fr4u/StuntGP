#pragma once
#include "sgn/race_session_platform.hpp"
#include "sgn/source_allocation_bank.hpp"
#include <algorithm>

namespace warm_release_receiving {
// Native receiving stimulus in the actual retained bank. This does not
// replace a scene child: the actual outer scheduler must release these rows.
class AllocationWitness {
    sgn::retail::SourceAllocationBank& bank_;
    sgn::retail::SourceAllocationRef ordinary_,persistent_;
    sgn::retail::SourceAllocationWords before_;
    static void verify(bool value,const char* why){if(!value)throw std::runtime_error(why);}
public:
    explicit AllocationWitness(sgn::retail::RaceSessionPlatform& platform):bank_(platform.source_allocations()){
        verify(bank_.live_count()==0,"warm receiving expects no prior source allocations");
        if(!bank_.initialized())bank_.initialize_42eab0(); //explicit fixture prerequisite, not runtime fallback
        verify(bank_.source_words().active==1,"warm receiving requires an active source scope");
        ordinary_=bank_.allocate_42e8d0(17,0);persistent_=bank_.allocate_42e8d0(29,1);
        std::fill(bank_.bytes(persistent_).begin(),bank_.bytes(persistent_).end(),std::uint8_t{0x5b});
        before_=bank_.source_words();
    }
    void released() const{
        bool expired=false;try{(void)bank_.bytes(ordinary_);}catch(const sgn::retail::SourceAllocationError&){expired=true;}
        const auto after=bank_.source_words();
        verify(expired&&bank_.live_count()==1&&after.active==0&&after.ordinary_bytes==0,
            "actual warm42EAA0 did not release the actual ordinary allocation");
        verify(after.persistent_bytes==before_.persistent_bytes&&after.peak_bytes==before_.peak_bytes&&
            after.next_ordinal==before_.next_ordinal&&after.release_base==before_.release_base&&
            after.release_persistent==0,"warm42EAA0 changed non-release source words");
        verify(std::all_of(bank_.bytes(persistent_).begin(),bank_.bytes(persistent_).end(),[](auto b){return b==0x5b;}),
            "warm42EAA0 lost retained category1 payload");
    }
};
}
