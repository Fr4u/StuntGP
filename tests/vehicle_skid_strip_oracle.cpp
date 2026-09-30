#include "sgn/vehicle_skid_strip.hpp"
#include "vehicle_oracle_state.hpp"
#include <fstream>
#include <iostream>
#include <set>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr unsigned size=0xc900,vertices=0x7800,corners=0x9800,polygons=0xb800,indices=0xbc00,normals=0xc000,globals=0xc800,part=0xc840;
constexpr unsigned base=0x10000000;
void read(std::ifstream& in,void* data,std::size_t bytes){if(!in.read(static_cast<char*>(data),static_cast<std::streamsize>(bytes)))throw std::runtime_error("short skid corpus");}
unsigned word(std::ifstream& in){unsigned v;read(in,&v,4);return v;}
unsigned get(const Bytes& b,unsigned at){unsigned v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,unsigned at,unsigned v){std::memcpy(b.data()+at,&v,4);}
std::uint16_t short_word(const Bytes& b,unsigned at){std::uint16_t v;std::memcpy(&v,b.data()+at,2);return v;}
void short_put(Bytes& b,unsigned at,std::uint16_t v){std::memcpy(b.data()+at,&v,2);}
void geometry(Bytes& b,VehicleStripGeometry& g,bool load){
    auto block=[&](auto& value,unsigned at){if(load)std::memcpy(&value,b.data()+at,sizeof(value));else std::memcpy(b.data()+at,&value,sizeof(value));};
    block(g.vertices,vertices);block(g.corners,corners);block(g.polygons,polygons);block(g.corner_indices,indices);block(g.normals,normals);
    auto w=[&](unsigned& value,unsigned at){if(load)value=get(b,at);else put(b,at,value);};
    w(g.field_5712f0,globals);w(g.field_5712f4,globals+4);w(g.field_5712f8,globals+8);
    auto s=[&](std::uint16_t& value,unsigned at){if(load)value=short_word(b,at);else short_put(b,at,value);};
    s(g.polygon_count,part+0x24);s(g.corner_count,part+0x26);s(g.vertex_count,part+0x28);
}
void strips(Bytes& b,VehicleStripHeaders& headers,VehicleSkidRings& rings,bool load){
    static_assert(sizeof(VehicleSkidSegment)==52&&sizeof(VehicleStripHeader)==12);
    for(unsigned j=0;j<4;++j){auto& ring=rings.items[j];const unsigned at=0x3964+j*0xd10;
        if(load){
            std::memcpy(&headers.items[j],b.data()+at,12);std::memcpy(ring.segments.data(),b.data()+at+16,64*52);
            const auto pointer=get(b,at+12);if(pointer){
                if(pointer<base+corners||pointer>=base+corners+8192||(pointer-base-corners)%16)throw std::runtime_error("invalid strip corner binding");
                ring.last_corner=(pointer-base-corners)/16;
            }
        }else {std::memcpy(b.data()+at,&headers.items[j],12);std::memcpy(b.data()+at+16,ring.segments.data(),64*52);put(b,at+12,ring.last_corner?base+corners+*ring.last_corner*16:0);}
    }
}
std::set<unsigned> audit_offsets(const VehicleSkidWriteAudit& audit){std::set<unsigned> result;
    for(unsigned w=0;w<4;++w)for(unsigned bit=0;bit<192;++bit)if(audit.undefined_writes[w][bit])result.insert(0x3964+w*0xd10+16+(bit/3)*52+(bit%3)*16+12);return result;
}
}
int main(int argc,char** argv){try{
    if(argc!=2)throw std::runtime_error("expected skid corpus");std::ifstream in(argv[1],std::ios::binary);char magic[8];read(in,magic,8);
    if(std::string(magic,8)!="SGNSKD01")throw std::runtime_error("skid schema");const auto count=word(in);if(!count||count>100000)throw std::runtime_error("skid case count");
    std::uint64_t compared=0,retained=0;unsigned errors=0;Bytes previous;unsigned previous_step=0;
    for(unsigned n=0;n<count;++n){const auto op=word(in),argument=word(in);if(op>2||argument>(op==0?63u:3u))throw std::runtime_error("skid operation/argument");
        Bytes before(size),expected(size);read(in,before.data(),size);read(in,expected.data(),size);auto actual=before;
        const auto step=get(before,globals+20);if(step>15)throw std::runtime_error("skid continuation step");
        if(step){
            if(op!=2||previous.size()!=size||step!=previous_step+1)throw std::runtime_error("skid continuation sequence");
            auto next=previous;put(next,globals+20,step);
            for(unsigned w=0;w<2;++w){const auto at=0x201c+w*0x98;const auto x=std::bit_cast<float>(get(next,at));put(next,at,std::bit_cast<unsigned>(vehicle_f32(double(x)+(step%3?67:240))));}
            if(next!=before)throw std::runtime_error("skid continuation did not retain native state");actual=std::move(next);
        }
        const auto masked=word(in);if(masked>768)throw std::runtime_error("skid mask count");std::set<unsigned> exclusions;
        for(unsigned j=0;j<masked;++j){const auto at=word(in);if(!exclusions.insert(at).second)throw std::runtime_error("duplicate skid mask");}
        VehicleParameters parameters;VehicleState vehicle;oracle::transfer(actual,parameters,vehicle,true);
        VehiclePresentationState presentation;for(unsigned j=0;j<4;++j)std::memcpy(&presentation.wheel_centers[j],actual.data()+0x201c+j*0x98,16);
        VehicleStripHeaders headers;VehicleSkidRings rings;strips(actual,headers,rings,true);VehicleStripGeometry g;geometry(actual,g,true);
        VehicleRaceState race;ReplayBank replay;const auto handle=get(actual,0x7268);if(handle){if(handle!=base+0x10000)throw std::runtime_error("skid replay binding");race.recording=ReplayHandle{0};std::memcpy(replay.bytes().data(),actual.data()+globals+16,2);}
        VehicleSkidWriteAudit audit;
        if(op==0)emit_vehicle_skid_quad(rings.items[0],argument,g,vehicle.up);
        else if(op==1)update_vehicle_wheel_skid(argument,vehicle,presentation,headers,rings,g,&audit);
        else update_vehicle_skid(vehicle,race,presentation,replay,get(actual,globals+12),headers,rings,g,&audit);
        if(audit_offsets(audit)!=exclusions)throw std::runtime_error("skid undefined-lane source/native audit differs at case "+std::to_string(n));
        geometry(actual,g,false);strips(actual,headers,rings,false);
        // Diagnostic observation must not alter any native output.
        auto plain=before;VehicleStripHeaders plain_headers;VehicleSkidRings plain_rings;VehicleStripGeometry plain_geometry;
        strips(plain,plain_headers,plain_rings,true);geometry(plain,plain_geometry,true);
        if(op==0)emit_vehicle_skid_quad(plain_rings.items[0],argument,plain_geometry,vehicle.up);
        else if(op==1)update_vehicle_wheel_skid(argument,vehicle,presentation,plain_headers,plain_rings,plain_geometry);
        else update_vehicle_skid(vehicle,race,presentation,replay,get(plain,globals+12),plain_headers,plain_rings,plain_geometry);
        geometry(plain,plain_geometry,false);strips(plain,plain_headers,plain_rings,false);if(plain!=actual)throw std::runtime_error("skid audit changed native result");
        for(unsigned at=0;at<size;at+=4){const bool opaque=exclusions.contains(at);if(opaque)++retained;else++compared;const auto wanted=get(opaque?before:expected,at);
            if(get(actual,at)!=wanted&&errors++<16)std::cerr<<"case "<<n<<" op "<<op<<" offset "<<std::hex<<at<<" actual "<<get(actual,at)<<" expected "<<wanted<<std::dec<<'\n';}
        previous=std::move(actual);previous_step=step;
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing skid corpus");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"compared_words\":"<<compared<<",\"retained_words\":"<<retained<<",\"mismatches\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
