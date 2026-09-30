#pragma once
#include "sgn/track_part_animation.hpp"
#include "sgn/model_resources.hpp"
#include <bit>
#include <cmath>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>
namespace sgn::track_phase_test {
using namespace retail;
using Bank=std::array<TrackPartAnimation,32>;
static_assert(sizeof(Bank)==0x900);
inline void check(bool value,const char* text){if(!value)throw FormatError(text);}
template<class E,class F>void rejects(F fn){bool caught=false;try{fn();}catch(const E&){caught=true;}check(caught,"missing typed track phase rejection");}
inline bool same(const Bank& a,const Bank& b){return std::memcmp(&a,&b,sizeof(a))==0;}
inline Bytes snapshot(SourceAllocationBank& bank,SourceAllocationRef ref){const auto bytes=bank.bytes(ref);return {bytes.begin(),bytes.end()};}
struct Fixture {
    SourceAllocationBank bank;AssetStore assets;ModelResources models;std::size_t handle;
    explicit Fixture(const std::filesystem::path& root,std::string name="track1"):assets(root),models(assets,root){bank.initialize_42eab0();handle=*models.resolve(models.load_source(bank,name));}
    SourcePmdModelView view(){return models.source_at(handle);}
    void count(std::uint32_t value){view().descriptor().field<std::uint32_t>(0x24).set(value);}
};
inline Bank state(){Bank b{};for(unsigned i=0;i<32;++i){auto& p=b[i];p.kind=i%18;
    p.angles={-1.0f,359.9f,720.0f,std::bit_cast<float>(0x7fc00000u+i)};
    p.angular_velocity={-120,120,60,std::bit_cast<float>(0xffc01000u+i)};
    p.parameters={1,2,3,std::bit_cast<float>(0x7fc02000u+i)};
    p.parameter_34=0x11223300u+i;p.phase_u=0xffffffffu-i;p.phase_v=0xabcdef00u+i;p.retained_40=0x87654300u+i;p.parameter_44=0xdeadbe00u+i;
}return b;}
struct Event {unsigned at,index,value;bool operator==(const Event&)const=default;};
struct Events {
    std::vector<Event> values;std::function<void(Event)> action;std::exception_ptr error;
    TrackPhaseAudit audit{this,[](void* context,unsigned at,unsigned index,unsigned value)noexcept{
        auto& self=*static_cast<Events*>(context);try{const Event e{at,index,value};self.values.push_back(e);if(self.action)self.action(e);}catch(...){self.error=std::current_exception();}
    }};
    void checked(){if(error)std::rethrow_exception(error);}
};
inline void read(std::ifstream& f,void* out,unsigned count){if(!f.read(static_cast<char*>(out),count))throw FormatError("truncated canonical track phase corpus");}
inline unsigned word(std::ifstream& f){unsigned result{};read(f,&result,4);return result;}
struct Record {std::string name;unsigned op,count,steps;Bank before,expected;};
inline std::vector<Record> corpus(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);char sig[8];read(f,sig,8);
    check(std::string_view(sig,8)=="SGNTAP01","canonical track phase schema");const auto count=word(f);check(count>0&&count<=10000,"canonical track phase corpus count");
    std::vector<Record> result;result.reserve(count);for(unsigned i=0;i<count;++i){const auto length=word(f);check(length>0&&length<=64,"canonical track phase name length");
        Record r;r.name.resize(length);read(f,r.name.data(),length);check(r.name.find_first_of("/\\:\0",0,4)==std::string::npos&&r.name.find("..")==std::string::npos,"canonical track phase invalid name");
        check(r.name.size()>4&&r.name.substr(r.name.size()-4)==".PMD","canonical track phase PMD extension");
        r.op=word(f);r.count=word(f);r.steps=word(f);check(r.op<=2,"canonical track phase opcode");check(r.steps>0&&r.steps<=64,"canonical track phase duration");
        if(r.op!=1)check(r.steps==1,"canonical track phase loader duration");
        read(f,&r.before,sizeof(Bank));read(f,&r.expected,sizeof(Bank));result.push_back(std::move(r));}
    check(f.peek()==std::char_traits<char>::eof(),"trailing canonical track phase corpus");return result;
}
inline void finite(const Bank& bank,unsigned count){if(count==0xffffffffu)return;
    for(unsigned i=1;i<count&&i<bank.size();++i){const auto& p=bank[i];if(p.kind!=4&&p.kind!=16)continue;
        for(float value:{p.angles.x,p.angles.y,p.angles.z,p.angular_velocity.x,p.angular_velocity.y,p.angular_velocity.z})check(std::isfinite(value),"canonical track phase outside finite domain");}
}
}
