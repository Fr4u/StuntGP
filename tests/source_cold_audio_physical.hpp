#pragma once
#include "sgn/race_session.hpp"
#include <array>
#include <stdexcept>

namespace sgn::test::cold_audio {
using namespace sgn::retail;
inline void require(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
using PhysicalImage=std::array<std::uint8_t,1904>;
// Receiving serializer of the source record, independent of production's
// scattered byte-target binder. Shared only by CPU and Main-only witnesses.
inline PhysicalImage physical_image(const RaceSessionState& s){
    PhysicalImage image{};std::size_t cursor=0;
    const auto byte=[&](std::uint8_t v){image.at(cursor++)=v;};
    const auto word=[&](std::uint32_t v){for(unsigned i=0;i<4;++i)byte(static_cast<std::uint8_t>(v>>(i*8)));};
    for(std::size_t i=0;i<4;++i){
        const auto& r=s.source_input.acquisition.physical[i];const auto& x=s.source_input.physical[i];
        const auto& e=s.menu_input.edges[i];const auto& b=r.extra_edges;
        word(s.menu_input.raw_words[i].word_0);word(s.menu_input.raw_words[i].word_4);
        word(r.word_8);word(r.word_0c);word(x.activity_kind);word(x.profile_index);
        word(s.frontend_launch.input_bindings[i].vehicle);word(s.frontend_launch.input_bindings[i].kind);
        word(x.feedback_mode);word(r.word_24);word(r.word_28);
        for(auto v:r.words_from_2c)word(v);
        for(auto v:x.bytes_168_187)byte(v);for(auto v:x.bytes_188_1a7)byte(v);
        byte(b.current_1a8);byte(b.current_1a9);byte(e.up);byte(e.down);byte(e.activate);byte(b.current_1ad);byte(e.back);
        byte(b.previous_1af);byte(b.previous_1b0);byte(e.up_previous);byte(e.down_previous);byte(e.activate_previous);byte(b.previous_1b4);byte(e.back_previous);
        for(auto v:x.bytes_1b6_1b7)byte(v);for(auto v:s.source_input.force_feedback.physical_words[i])word(v);
        for(auto v:x.bytes_1d8_1db)byte(v);
        require(cursor==(i+1)*476,"physical receiving record extent");
    }
    return image;
}
inline PhysicalImage reset_image(){
    PhysicalImage image{};
    for(std::size_t i=0;i<4;++i){image[i*476+24]=255;image[i*476+25]=255;}
    return image;
}
inline void seed_physical(RaceSessionState& s){
    for(std::size_t i=0;i<4;++i){
        const auto word=0x90807060u+static_cast<unsigned>(i)*0x01010101u;
        const auto byte=static_cast<std::uint8_t>(0xb0u+i);
        auto& r=s.source_input.acquisition.physical[i];auto& x=s.source_input.physical[i];
        s.menu_input.raw_words[i]={word,word};r.word_8=word;r.word_0c=word;r.word_24=word;r.word_28=word;
        r.words_from_2c.fill(word);r.extra_edges={byte,byte,byte,byte,byte,byte};
        x.activity_kind=word;x.profile_index=word;x.feedback_mode=word;
        x.bytes_168_187.fill(byte);x.bytes_188_1a7.fill(byte);x.bytes_1b6_1b7.fill(byte);x.bytes_1d8_1db.fill(byte);
        s.menu_input.edges[i]={byte,byte,byte,byte,byte,byte,byte,byte};
        s.frontend_launch.input_bindings[i]={word,word};s.source_input.force_feedback.physical_words[i].fill(word);
    }
}
}
