#pragma once
#include "sgn/retail_replay.hpp"
#include <cstring>

namespace sgn::retail::fixture {
// Existing SGNRP001/SGNPB001/SGNSEL01/TimeTrial corpora encode ALL six +10
// words as modular offsets, including inactive headers. Confined to their
// invocation boundary; production replay bytes never pass through this codec.
inline void legacy_replay_cursor_words(ReplayBank& bank,bool to_source){
    for(std::uint32_t slot=0;slot<ReplayBank::slots;++slot){
        const auto at=ReplayBank::base({slot})+0x10u;
        std::uint32_t word;std::memcpy(&word,bank.bytes().data()+at,4);
        word=to_source?word+ReplayBank::source_base:word-ReplayBank::source_base;
        std::memcpy(bank.bytes().data()+at,&word,4);
    }
}
}
