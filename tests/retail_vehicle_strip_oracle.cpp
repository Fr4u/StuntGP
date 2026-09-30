#include "sgn/retail_vehicle_strip.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
using namespace sgn;
using namespace sgn::retail;
namespace {
constexpr std::size_t polygons=0,indices=1024,vertices=2048,corners=10240,part=18432,descriptor=18496,
 normals=18600,globals=20648,resource=globals+64,current=resource+4,cars=current+4,car_size=0x7800,total=cars+6*car_size;
constexpr std::uint32_t token=0x10000000;
std::uint32_t get(const Bytes& b,std::size_t at){std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
void put(Bytes& b,std::size_t at,std::uint32_t v){std::memcpy(b.data()+at,&v,4);}
void put16(Bytes& b,std::size_t at,std::uint16_t v){std::memcpy(b.data()+at,&v,2);}
struct Reader {
 std::ifstream file;explicit Reader(const char* path):file(path,std::ios::binary){if(!file)throw std::runtime_error("cannot open strip corpus");}
 Bytes bytes(std::size_t n){if(n>2*total)throw std::runtime_error("excessive strip field");Bytes b(n);if(!file.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("truncated strip corpus");return b;}
 std::uint32_t word(){return get(bytes(4),0);}
};
void load_globals(const Bytes& b,VehicleStripGeometry& g){
 static_assert(sizeof(VehicleStripCorner)==16&&sizeof(VehicleStripPolygon)==8&&sizeof(VehicleVector)==16);
 std::memcpy(g.normals.data(),b.data()+normals,2048);
 g.texture_id=get(b,globals+36);g.field_5712f0=get(b,globals+24);g.field_5712f4=get(b,globals+28);g.field_5712f8=get(b,globals+32);
}
void serialize(Bytes& b,const VehicleStripGeometry& g){
 std::memcpy(b.data()+polygons,g.polygons.data(),1024);std::memcpy(b.data()+indices,g.corner_indices.data(),1024);
 std::memcpy(b.data()+vertices,g.vertices.data(),8192);std::memcpy(b.data()+corners,g.corners.data(),8192);
 std::memcpy(b.data()+normals,g.normals.data(),2048);
 // Freshly allocated source part/descriptor records contain zero in all
 // unused fields. Test-only addresses express the verified pointer graph.
 std::fill(b.begin()+part,b.begin()+normals,std::uint8_t{0});
 put16(b,part,g.part_flags);put(b,part+8,0x474820);
 put16(b,part+0x20,g.part_field20);put16(b,part+0x22,g.part_field22);
 put16(b,part+0x24,g.polygon_count);put16(b,part+0x26,g.corner_count);put16(b,part+0x28,g.vertex_count);
 put(b,part+0x2c,token+corners);put(b,part+0x30,token+vertices);put(b,part+0x34,token+polygons);
 put16(b,descriptor+2,1);put16(b,descriptor+0x20,g.model_field20);put16(b,descriptor+0x22,g.model_field22);
 put(b,descriptor+0x24,1);put(b,descriptor+0x28,0x570ad8);put(b,descriptor+0x2c,token+polygons);
 put(b,descriptor+0x30,token+corners);put(b,descriptor+0x34,token+vertices);put(b,descriptor+0x38,token+indices);
 put(b,descriptor+0x3c,0x474820);put(b,descriptor+0x40,0x5712d8);
 for(unsigned at=0x44;at<=0x54;at+=4)put(b,descriptor+at,token+part);
 put(b,descriptor+0x60,0x5712fc);put(b,globals,0x5712e0);
 for(unsigned i=0;i<3;++i)put(b,globals+8+i*4,g.texture_names[i]);
 put(b,globals+24,g.field_5712f0);put(b,globals+28,g.field_5712f4);put(b,globals+32,g.field_5712f8);put(b,globals+36,g.texture_id);
 for(auto [offset,destination]:std::array<std::pair<std::size_t,std::size_t>,6>{{{40,polygons},{44,indices},{48,vertices},{52,corners},{56,part},{60,descriptor}}})put(b,globals+offset,static_cast<std::uint32_t>(token+destination));
 put(b,resource,token+descriptor);
}
struct Binding final:VehicleStripBinding {VehicleStripGeometry* live{};unsigned calls{};void bind(VehicleStripGeometry& g) override{live=&g;++calls;}};
Bytes execute(Bytes b,unsigned op,unsigned& binds){
 auto target=get(b,current);if(target>=6)throw std::runtime_error("invalid current vehicle");
 VehicleStripGeometry geometry;load_globals(b,geometry);std::array<VehicleStripHeaders,6> headers;
 for(unsigned i=0;i<6;++i)for(unsigned j=0;j<4;++j){auto& h=headers[i].items[j];auto at=cars+i*car_size+0x3964+j*0xd10;h={get(b,at),get(b,at+4),get(b,at+8)};}
 Binding binding;
 if(op==0)initialize_strip_geometry(geometry,binding);else if(op==1)expand_strip_geometry(geometry,binding);
 else if(op==2)initialize_vehicle_strip(geometry,headers[target],binding);else throw std::runtime_error("unknown strip operation");
 if(binding.calls!=1||binding.live!=&geometry)throw std::runtime_error("generated geometry not bound to its live owner");
 binds+=binding.calls;serialize(b,*binding.live);
 for(unsigned i=0;i<6;++i)for(unsigned j=0;j<4;++j){const auto& h=headers[i].items[j];auto at=cars+i*car_size+0x3964+j*0xd10;
  put(b,at,h.field_00);put(b,at+4,h.field_04);put(b,at+8,h.field_08);}
 return b;
}
}
int main(int argc,char**argv){try{
 if(argc!=2)throw std::runtime_error("usage: retail_vehicle_strip_oracle corpus");Reader input(argv[1]);auto magic=input.bytes(8);
 if(std::string(magic.begin(),magic.end())!="SGNSTRP1")throw std::runtime_error("invalid strip corpus signature");
 auto count=input.word();if(!count||count>10000)throw std::runtime_error("invalid strip case count");std::uint64_t words=0;unsigned binds=0;
 for(unsigned i=0;i<count;++i){auto op=input.word();if(op>2)throw std::runtime_error("invalid strip operation");auto before=input.bytes(total),expected=input.bytes(total);
  auto actual=execute(std::move(before),op,binds);
  for(std::size_t at=0;at<total;at+=4){++words;if(get(actual,at)!=get(expected,at)){
   std::cerr<<"case "<<i<<" op "<<op<<" at 0x"<<std::hex<<at<<" actual "<<get(actual,at)<<" expected "<<get(expected,at)<<'\n';return 1;}}
 }
 if(input.file.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing strip data");
 std::cout<<"{\"passed\":true,\"cases\":"<<count<<",\"words\":"<<words<<",\"bindings\":"<<binds<<",\"mismatches\":0,\"ulp\":0}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
