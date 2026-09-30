#include "vehicle_ai_automatic_state.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <memory>

using namespace sgn;
using namespace sgn::retail;
namespace {
constexpr std::size_t car_size=0x7800,global_at=6*car_size,total_size=global_at+0x80;
Bytes file(const std::filesystem::path& p){std::ifstream f(p,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open input");
    const auto size=f.tellg();if(size<0||size>1024ll*1024*1024)throw std::runtime_error("excessive input");
    Bytes out(static_cast<std::size_t>(size));f.seekg(0);f.read(reinterpret_cast<char*>(out.data()),size);if(!f)throw std::runtime_error("short input");return out;}
struct Reader{std::span<const std::uint8_t> b;std::size_t at{};
    auto bytes(std::size_t n){if(at>b.size()||n>b.size()-at)throw std::runtime_error("truncated corpus");auto out=b.subspan(at,n);at+=n;return out;}
    std::uint32_t word(){auto v=bytes(4);std::uint32_t x;std::memcpy(&x,v.data(),4);return x;}};
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t x;std::memcpy(&x,b.data()+at,4);return x;}
float real(const Bytes& b,std::size_t at){return std::bit_cast<float>(get(b,at));}
void put(Bytes& b,std::size_t at,std::uint32_t x){std::memcpy(b.data()+at,&x,4);}
void put(Bytes& b,std::size_t at,float x){put(b,at,std::bit_cast<std::uint32_t>(x));}
unsigned required_exclusions(const Bytes& b,unsigned op,unsigned self){
    if(op==1)return 2;
    if(op==2)return 1;
    if(op!=5)return 0;
    // Independent raw-image predicate from 40800B..408112. Initialization,
    // the ramp decrement and subsequent automatic ticks do not change these
    // five guards. Only a reached 408167 call can write target W at 406D27.
    const auto at=self*car_size;
    return get(b,at+0x1ba8)!=0 && get(b,at+0x7648)!=1 && get(b,at+0x24e8)!=1 &&
           (get(b,at+0x24e0)==1 || get(b,at+0x767c)==1) && get(b,global_at+0x3c)!=3 ? 1u:0u;
}
void ownership(){VehicleParameters p{};VehicleState s{};VehicleRaceState r{};AiAutomaticMemory a{};
    std::array<Bytes,3> b{Bytes(car_size,0xa5),Bytes(car_size,0xa5),Bytes(car_size,0xa5)};
    oracle::transfer(b[0],p,s,false);race_oracle::transfer(b[1],r,false);automatic_oracle::transfer(b[2],a,false);
    for(std::size_t at=0;at<car_size;++at){unsigned n=0;for(auto& v:b)n+=v[at]!=0xa5;if(n>1)throw std::runtime_error("overlapping AI/physical/race ownership");}}
void execute(Bytes& b,SplineSet& set,unsigned op,unsigned self,unsigned ticks){
    auto p=std::make_unique<std::array<VehicleParameters,6>>();auto s=std::make_unique<std::array<VehicleState,6>>();
    std::array<VehicleRaceState,6> r{};std::array<AiAutomaticMemory,6>a{};
    auto transfer=[&](bool read){for(unsigned j=0;j<6;++j){std::span image(b.data()+j*car_size,car_size);
        oracle::transfer(image,(*p)[j],(*s)[j],read);race_oracle::transfer(image,r[j],read);automatic_oracle::transfer(image,a[j],read);}};
    transfer(true);
    std::array<AiAutomaticVehicle,6> cars{{{(*p)[0],(*s)[0],r[0],a[0]},{(*p)[1],(*s)[1],r[1],a[1]},
        {(*p)[2],(*s)[2],r[2],a[2]},{(*p)[3],(*s)[3],r[3],a[3]},{(*p)[4],(*s)[4],r[4],a[4]},{(*p)[5],(*s)[5],r[5],a[5]}}};
    AiAutomaticGlobals g;g.initialized=get(b,global_at);
    std::array<SplineView,3> routes;
    for(unsigned j=0;j<3;++j){auto view=set.find(get(b,global_at+4+j*4));if(!view)throw std::runtime_error("invalid fixture route");routes[j]=*view;g.route_distances[j]=real(b,global_at+0x10+j*4);}
    g.routes=routes;g.target={real(b,global_at+0x20),real(b,global_at+0x24),real(b,global_at+0x28)};
    g.target_speed=real(b,global_at+0x30);g.response_scale=real(b,global_at+0x34);g.braking_allowed=get(b,global_at+0x38);
    AiAutomaticEnvironment e{{real(b,global_at+0x40),real(b,global_at+0x44),real(b,global_at+0x48)},real(b,global_at+0x4c),real(b,global_at+0x50),get(b,global_at+0x3c)};
    for(unsigned n=0;n<ticks;++n){
        if(op==0)initialize_ai_automatic(DecodedTrackSplines{set,{}},cars,g);
        else if(op==1){auto v=sample_ai_route(g,real(b,global_at+0x54),real(b,global_at+0x58));put(b,global_at+0x60,v.x);put(b,global_at+0x64,v.y);put(b,global_at+0x68,v.z);}
        else if(op==2)choose_ai_route_target(cars[self],g);
        else if(op==3)choose_ai_route_speed(cars[self],g,e);
        else if(op==4)apply_ai_pedals(cars[self],g);
        else if(op==5)ai_automatic_tick(DecodedTrackSplines{set,{}},cars,self,g,e);
        else throw std::runtime_error("unknown operation");
    }
    transfer(false);put(b,global_at,g.initialized);
    if(!g.routes)throw std::runtime_error("missing route result");
    for(unsigned j=0;j<3;++j){put(b,global_at+4+j*4,(*g.routes)[j].descriptor.id);put(b,global_at+0x10+j*4,g.route_distances[j]);}
    put(b,global_at+0x20,g.target.x);put(b,global_at+0x24,g.target.y);put(b,global_at+0x28,g.target.z);
    put(b,global_at+0x30,g.target_speed);put(b,global_at+0x34,g.response_scale);put(b,global_at+0x38,g.braking_allowed);
}
}
int main(int argc,char**argv){try{
    if(argc!=3)throw std::runtime_error("usage: vehicle_ai_automatic_oracle corpus model_directory");
    ownership();auto data=file(argv[1]);Reader input{data};auto magic=input.bytes(8);
    if(std::string(magic.begin(),magic.end())!="SGNAUTO1")throw std::runtime_error("invalid corpus signature");
    auto count=input.word();if(!count||count>10000)throw std::runtime_error("empty/excessive corpus");
    std::map<std::string,SplineSet> models;std::uint64_t words=0;
    for(unsigned c=0;c<count;++c){auto op=input.word(),self=input.word(),ticks=input.word(),len=input.word(),excluded=input.word();
        if(op>5||self>=6||ticks<1||ticks>600||len<1||len>200||excluded>3)throw std::runtime_error("invalid case metadata");
        if(((excluded&1)&&op!=2&&op!=5)||((excluded&2)&&op!=1))throw std::runtime_error("undefined-lane mask outside source-writing operation");
        auto bytes=input.bytes(len);std::string name(bytes.begin(),bytes.end());if(name.find_first_of("/\\:")!=std::string::npos||name.find("..")!=std::string::npos)throw std::runtime_error("invalid model name");
        if(!models.contains(name))models.emplace(name,SplineSet(file(std::filesystem::path(argv[2])/name)));
        auto before=input.bytes(total_size);auto expected=input.bytes(total_size);Bytes actual(before.begin(),before.end());
        if(excluded!=required_exclusions(actual,op,self))throw std::runtime_error("undefined-lane mask differs from source guard predicate");
        execute(actual,models.at(name),op,self,ticks);
        for(std::size_t at=0;at<total_size;at+=4){
            if((at==global_at+0x2c&&(excluded&1))||(at==global_at+0x6c&&(excluded&2)))continue;
            std::uint32_t want;std::memcpy(&want,expected.data()+at,4);++words;
            if(get(actual,at)!=want){std::cerr<<"case "<<c<<" model "<<name<<" op "<<op<<" self "<<self<<" offset 0x"<<std::hex<<at<<" actual "<<get(actual,at)<<" original "<<want<<'\n';return 1;}
        }
    }
    if(input.at!=data.size())throw std::runtime_error("trailing corpus bytes");
    std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"words\":"<<words<<",\"mismatches\":0,\"ulp\":0,\"single_state_ownership\":true}\n";return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
