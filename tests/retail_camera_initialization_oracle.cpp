#include "retail_camera_lifecycle_state.hpp"
#include "sgn/retail_camera_initialization.hpp"
#include "sgn/retail_camera_vehicle.hpp"
#include "sgn/vehicle.hpp"
#include <fstream>
#include <iostream>
#include <algorithm>
namespace {
using namespace sgn::retail;
using namespace sgn::retail::camera_test;
constexpr std::size_t cs=0x900;
void read(std::ifstream& in,void* p,std::size_t n) {in.read(static_cast<char*>(p),static_cast<std::streamsize>(n));if(!in)throw sgn::FormatError("truncated initialization corpus");}
std::uint32_t number(std::ifstream& in) {std::uint32_t value;read(in,&value,4);return value;}
Bytes blob(std::ifstream& in) {const auto n=number(in);if(n>1000000)throw sgn::FormatError("invalid corpus blob");Bytes result(n);read(in,result.data(),n);return result;}
std::string text(std::ifstream& in) {const auto b=blob(in);return {b.begin(),b.end()};}
void state(Bytes& c,contact::player::Controller& controller,CameraLifecycle& life,CameraPresentationState& presentation,
           std::optional<SplineView> bound,bool reading) {
    Bytes small(c.begin(),c.begin()+camera_size),dummy(vehicle_size);CameraVehicle ignored;
    if(reading)std::copy_n(c.begin()+0x800,0xd0,small.begin()+0x200);
    transfer(small,dummy,controller.camera,ignored,reading);lifecycle(small,life,reading);
    if(!reading) {std::copy_n(small.begin(),0x1e8,c.begin());std::copy_n(small.begin()+0x200,0xd0,c.begin()+0x800);}
    const auto integer=[&](std::uint32_t& x,std::size_t at) {if(reading)x=word(c,at);else put(c,at,x);};
    const auto signed_integer=[&](std::int32_t& x,std::size_t at) {if(reading)x=std::bit_cast<std::int32_t>(word(c,at));else put(c,at,std::bit_cast<std::uint32_t>(x));};
    const auto vector=[&](CameraVector& v,std::size_t at) {
        float* p[]{&v.x,&v.y,&v.z,&v.w};for(unsigned i=0;i<4;++i)if(reading)*p[i]=real(c,at+i*4);else put(c,at+i*4,std::bit_cast<std::uint32_t>(*p[i]));
    };
    signed_integer(controller.active,0);integer(controller.vehicle,0xac);
    integer(controller.respawn_transition.state,0x300);integer(controller.respawn_transition.phase,0x308);
    if(reading)presentation.spline=word(c,0x1a8)?bound:std::nullopt;else put(c,0x1a8,presentation.spline?1:0);
    vector(presentation.up_axis,0x8c);vector(presentation.forward_axis,0x9c);
    for(unsigned i=0;i<4;++i)vector(presentation.planes[i],0x118+i*16);
    for(unsigned i=0;i<8;++i) {
        auto& n=controller.notices[i];const auto at=0x4ac+i*0x54;
        signed_integer(n.remaining,at);signed_integer(n.initial,at+4);integer(n.tick,at+8);
        if(reading)std::memcpy(n.text.data(),c.data()+at+12,64);else std::memcpy(c.data()+at+12,n.text.data(),64);
        integer(n.source_vehicle,at+0x4c);signed_integer(n.audience,at+0x50);
    }
}
std::vector<std::size_t> offsets() {
    std::vector<std::size_t> result;
    for(auto [start,end]:{std::pair{0,0x48},std::pair{0x54,0x58},std::pair{0x68,0x114},
                          std::pair{0x118,0x158},std::pair{0x198,0x1e8},std::pair{0x300,0x304},
                          std::pair{0x308,0x30c},std::pair{0x4ac,0x74c}})
        for(auto at=start;at<end;at+=4)result.push_back(static_cast<std::size_t>(at));
    return result;
}
struct Reset {
    std::array<VehicleState,6> physical;CameraEffectTargets targets;CameraModeReset actual;
    std::uint32_t hash{0x811c9dc5u},count{};
    CameraModeReset adapter() {return {this,actual.active,[](void* context,std::uint32_t slot) noexcept {
        auto& self=*static_cast<Reset*>(context);self.hash=(self.hash^slot)*16777619u;++self.count;self.actual.reset(self.actual.context,slot);
    }};}
};
void contracts() {
    CameraBank bank;bank.count=7;CameraSessionState session{19,.2f,.3f};const auto previous=bank;
    Bytes bad(328);std::memcpy(bad.data(),"PMD V1.83",9);put(bad,0xe8,33);
    bool rejected=false;try{camera_prepare_resources(bank,session,1,std::nullopt,bad);}catch(const sgn::FormatError&){rejected=true;}
    if(!rejected||std::memcmp(&bank,&previous,sizeof(bank))||session.authored_cycle_index!=19)throw sgn::FormatError("invalid fallback mutated bank");
    contact::player::Controls controls;controls.current=7;controls.controllers[0].camera.drift.x=42;
    rejected=false;try{camera_begin_outer_frame(controls);}catch(const sgn::FormatError&){rejected=true;}
    if(!rejected||controls.controllers[0].camera.drift.x!=42)throw sgn::FormatError("invalid current controller mutated primary");
}
}
int main(int argc,char** argv) try {
    contracts();if(argc!=3)throw sgn::FormatError("expected retail root and corpus");sgn::AssetStore assets(argv[1]);
    std::ifstream in(argv[2],std::ios::binary);char magic[8];read(in,magic,8);if(std::memcmp(magic,"SGNINIT1",8))throw sgn::FormatError("wrong initialization corpus");
    const auto count=number(in);if(!count||count>10000)throw sgn::FormatError("invalid initialization cases");
    const auto represented=offsets();std::uint64_t words=0,mismatches=0;
    for(std::uint32_t n=0;n<count;++n) {
        const auto kind=number(in),scene=number(in),split=number(in),lookup=number(in);
        const auto pmd_name=text(in),cam_name=text(in);auto override_pmd=blob(in);const auto has_text=number(in);const auto raw_text=text(in);
        auto pmd=override_pmd.empty()?sgn::read_file(std::filesystem::path(argv[1])/"meshdata"/pmd_name):override_pmd;
        SplineSet full(pmd);Bytes empty_bytes(328);std::memcpy(empty_bytes.data(),"PMD V1.83",9);SplineSet empty(std::move(empty_bytes));
        const auto& splines=lookup?full:empty;
        CameraBank bank,expected_bank;bank.count=number(in);read(in,bank.records.data(),sizeof(bank.records));
        expected_bank.count=number(in);read(in,expected_bank.records.data(),sizeof(expected_bank.records));
        std::array<std::uint32_t,7> globals,expected_globals;read(in,globals.data(),sizeof(globals));read(in,expected_globals.data(),sizeof(expected_globals));
        std::array<Bytes,4> c,expected_c;std::array<Bytes,6> v,expected_v;
        for(auto* group:{&c,&expected_c})for(auto& b:*group){b.resize(cs);read(in,b.data(),b.size());}
        for(auto* group:{&v,&expected_v})for(auto& b:*group){b.resize(vehicle_size);read(in,b.data(),b.size());}
        contact::player::Controls controls;controls.current=globals[0];std::array<CameraLifecycle,4> lives;
        std::array<CameraPresentationState,4> presentations;std::array<CameraVehicle,6> vehicles;Reset reset;
        for(unsigned i=0;i<4;++i)state(c[i],controls.controllers[i],lives[i],presentations[i],full.find(1),true);
        for(unsigned i=0;i<6;++i) {
            Bytes dummy(camera_size);CameraState ignored;transfer(dummy,v[i],ignored,vehicles[i],true);
            reset.targets.vehicles[i]=&reset.physical[i];reset.targets.active[i]=vehicles[i].active;
            for(unsigned k=0;k<8;++k)reset.physical[i].camera_effects.values[k]=word(v[i],0x2800+k*4);
        }
        reset.actual=camera_vehicle_reset_adapter(reset.targets);
        CameraSessionState session{globals[2],std::bit_cast<float>(globals[3]),std::bit_cast<float>(globals[4])};
        CameraMatrix shared_model_identity{}; //isolated fixture owner; old corpus does not compare61C468
        CameraInitializationBindings bindings{controls,lives,presentations,bank,session,vehicles,globals[1],reset.adapter(),shared_model_identity};
        if(kind==0)camera_initialize_level(bindings,{assets,cam_name,pmd,splines},scene,split);
        else if(kind==1)camera_begin_outer_frame(controls);
        else if(kind==2)camera_prepare_view(controls,lives,presentations,session,splines,{bool(split&1),bool(split&2)},reset.adapter(),shared_model_identity);
        else if(kind==3)camera_prepare_resources(bank,session,scene,has_text?std::optional<std::string_view>(raw_text):std::nullopt,pmd);
        else throw sgn::FormatError("unknown initialization operation");
        globals={controls.current,globals[1],session.authored_cycle_index,std::bit_cast<std::uint32_t>(session.path_speed),std::bit_cast<std::uint32_t>(session.path_parameter),reset.hash,reset.count};
        for(unsigned i=0;i<4;++i)state(c[i],controls.controllers[i],lives[i],presentations[i],full.find(1),false);
        for(unsigned i=0;i<6;++i) {
            Bytes dummy(camera_size);CameraState ignored;transfer(dummy,v[i],ignored,vehicles[i],false);
            for(unsigned k=0;k<8;++k)put(v[i],0x2800+k*4,reset.physical[i].camera_effects.values[k]);
        }
        auto compare=[&](std::uint32_t value,std::uint32_t expected,const std::string& what,std::size_t at) {
            ++words;if(value==expected)return;if(mismatches<20)std::cerr<<"case="<<n<<" kind="<<kind<<" "<<what<<"+"<<std::hex<<at<<" actual="<<value<<" expected="<<expected<<std::dec<<'\n';++mismatches;
        };
        for(unsigned i=0;i<4;++i) {
            for(auto at:represented)compare(word(c[i],at),word(expected_c[i],at),"controller"+std::to_string(i),at);
            if(kind==2 && i==controls.current)for(std::size_t at=0x800;at<0x8d0;at+=4) {
                // 848/84c are protocol padding between depth values and basis,
                // not CameraState fields or original matrix words.
                if(at==0x848||at==0x84c)continue;
                compare(word(c[i],at),word(expected_c[i],at),"view-cache",at);
            }
        }
        for(unsigned i=0;i<6;++i)for(std::size_t at=0;at<vehicle_size;at+=4)compare(word(v[i],at),word(expected_v[i],at),"vehicle"+std::to_string(i),at);
        for(unsigned i=0;i<7;++i)compare(globals[i],expected_globals[i],"global",i);
        compare(bank.count,expected_bank.count,"bank-count",0);
        for(std::size_t at=0;at<sizeof(bank.records);at+=4) {
            std::uint32_t a,b;std::memcpy(&a,reinterpret_cast<const std::uint8_t*>(bank.records.data())+at,4);
            std::memcpy(&b,reinterpret_cast<const std::uint8_t*>(expected_bank.records.data())+at,4);compare(a,b,"bank",at);
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw sgn::FormatError("trailing initialization corpus");
    std::cout<<"cases="<<count<<" words="<<words<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
