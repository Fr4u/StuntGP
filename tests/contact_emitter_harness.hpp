#pragma once
#include "sgn/contact_particle_allocator.hpp"
#include "contact_takeoff_state.hpp"
#include <algorithm>
#include <fstream>
#include <iostream>

namespace sgn::retail::contact::emitter_oracle {
constexpr unsigned size=0xb400,bank_at=0x8000,parameters_at=0x7900;
using Record=std::vector<std::uint8_t>;
inline unsigned word(const Record& b,unsigned at){unsigned value;std::memcpy(&value,b.data()+at,4);return value;}
inline void word(Record& b,unsigned at,unsigned value){std::memcpy(b.data()+at,&value,4);}
template<class Apply>int run(int argc,char** argv,std::string_view schema,Apply apply)try{
    if(argc!=2)throw std::runtime_error("expected particle emitter corpus");std::ifstream file(argv[1],std::ios::binary);
    const auto read=[&](void* out,std::size_t n){file.read(static_cast<char*>(out),static_cast<std::streamsize>(n));if(!file)throw std::runtime_error("truncated particle emitter corpus");};
    const auto next=[&](){unsigned value;read(&value,4);return value;};char magic[8];read(magic,8);if(std::string_view(magic,8)!=schema)throw std::runtime_error("particle emitter schema");
    const auto scenarios=next(),steps=next();if(!scenarios||scenarios>4096||!steps||steps>64)throw std::runtime_error("particle emitter scenario limits");
    std::uint64_t exact=0,excluded=0;
    for(unsigned n=0;n<scenarios;++n){effects::EffectParticleBank bank;effects::EffectParticleFrameAudit audit;Record actual(size),previous_expected;std::vector<unsigned> previous_mask;
        for(unsigned step=0;step<steps;++step){Record input(size),expected(size);read(input.data(),size);read(expected.data(),size);const auto count=next();if(count>64*23)throw std::runtime_error("particle emitter mask limit");
            std::vector<unsigned> mask;for(unsigned j=0;j<count;++j)mask.push_back(next());
            if(step==0)actual=input;else {
                if(input!=previous_expected)throw std::runtime_error("particle emitter continuation input differs from prior source output");
                for(unsigned at=0;at<size;at+=4)if(!std::binary_search(previous_mask.begin(),previous_mask.end(),at)&&word(actual,at)!=word(input,at))throw std::runtime_error("particle emitter native continuation diverged");
            }
            const auto before=actual;apply(actual,bank,audit);
            auto native_mask=takeoff_oracle::excluded(bank,bank_at);
            for(unsigned j=0;j<64;++j)for(unsigned k=0;k<16;++k)if(audit.undefined[j][k])native_mask.push_back(bank_at+j*0xd0+0x18+k*4);
            std::sort(native_mask.begin(),native_mask.end());if(mask!=native_mask)throw std::runtime_error("particle emitter exact native mask differs");
            for(auto at:mask)if(word(actual,at)!=word(before,at))throw std::runtime_error("particle emitter opaque retention differs");
            for(unsigned at=0;at<size;at+=4){if(std::binary_search(mask.begin(),mask.end(),at)){++excluded;continue;}
                if(word(actual,at)!=word(expected,at))throw std::runtime_error("particle emitter mismatch scenario "+std::to_string(n)+" step "+std::to_string(step)+" offset "+std::to_string(at));++exact;}
            previous_expected=std::move(expected);previous_mask=mask;
        }
    }
    if(file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing particle emitter corpus");
    std::cout<<"{\"cases\":"<<scenarios*steps<<",\"exact_words\":"<<exact<<",\"errors\":0,\"excluded_words\":"<<excluded<<",\"allowed_ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
}
