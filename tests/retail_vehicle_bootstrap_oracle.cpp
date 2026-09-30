#include "retail_bootstrap_state.hpp"
#include "contact_solver_state.hpp"
#include "retail_vehicle_load_state.hpp"
#include "retail_vehicle_anchors_state.hpp"
#include "vehicle_restart_state.hpp"
#include "vehicle_tick_state.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>

namespace {
using namespace sgn;using namespace sgn::retail;
using namespace sgn::retail::bootstrap_test;
struct Reader{
    std::ifstream in;explicit Reader(const char* path):in(path,std::ios::binary){}
    void read(void* out,std::size_t size){if(!in.read(static_cast<char*>(out),static_cast<std::streamsize>(size)))throw std::runtime_error("truncated bootstrap corpus");}
    std::uint32_t word(){std::uint32_t value;read(&value,4);return value;}
};
struct StripBinding final:VehicleStripBinding{VehicleStripGeometry* live{};unsigned calls{};void bind(VehicleStripGeometry& g)override{live=&g;++calls;}};
bool opaque(unsigned slot,std::size_t at,const VehicleGridPayloadAudit& audit){
    if(slot>=6)return false;
    if(audit.orientation[slot]&&(at==0x1bd8||at==0x1be8||at==0x1bf8||at==0x72c4||at==0x72d4||at==0x72e4))return true;
    return audit.position[slot]&&(at==0x1bb8||at==0x1bc8||at==0x727c||at==0x728c);
}
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("bootstrap oracle needs corpus and retail root");
    Reader reader(argv[1]);std::array<char,8> magic{};reader.read(magic.data(),8);
    if(std::string_view(magic.data(),8)!="SGNBOOT1")throw std::runtime_error("bootstrap corpus signature");
    const auto count=reader.word();if(count==0||count>512)throw std::runtime_error("bootstrap case count");
    AssetStore assets(argv[2]);std::uint64_t words=0,opaque_words=0,mismatches=0;
    for(unsigned n=0;n<count;++n){
        const auto config=std::bit_cast<std::int32_t>(reader.word()),control=std::bit_cast<std::int32_t>(reader.word()),selector=std::bit_cast<std::int32_t>(reader.word());
        const auto active=reader.word(),prototype_model=reader.word(),spline_mode=reader.word();
        if(active>=64||spline_mode>2)throw std::runtime_error("bootstrap resource mask or spline mode");
        std::array<Record,8> before{},expected{};for(auto& b:before)reader.read(b.data(),b.size());for(auto& b:expected)reader.read(b.data(),b.size());
        std::array<VehicleInitializationExtras,6> cold_extras{};
        for(auto& extra:cold_extras)extra.field_28b0=0xdeadbeefu;
        initialize_cold_vehicle_extras(cold_extras);
        for(unsigned i=0;i<6;++i)if(cold_extras[i].field_28b0!=get(before[i],0x28b0))
            throw std::runtime_error("cold28B0 initializer differs from executed40A2F0");
        ModelResources models(assets,argv[2]);std::array<LevelCarSlot,6> cars{};
        for(unsigned i=0;i<6;++i){const auto name="car"+std::to_string(i+1);
            const auto model=models.load(name),shadow=models.load(name+"sh");cars[i].model=i==0?prototype_model:i+1;
            if(active&(1u<<i)){cars[i].source_word00=model;cars[i].source_word04=shadow;}}
        auto storage=std::make_unique<VehicleStorage>();for(unsigned i=0;i<8;++i)transfer(before[i],*storage,i,true,models);
        auto views=storage->reset_views();contact::player::Controls controls{};VehicleResetGlobals globals{};
        VehicleResetParameters rp{};rp.selector=selector;
        rp.vehicle_names={"CAR0","CAR1","CAR2","CAR3","CAR4","CAR5"};rp.special_end_text="Recovered";
        VehicleResetWorld reset{views,5,controls,globals,rp};
        SplineSet splines(read_file(std::filesystem::path(argv[2])/(spline_mode==1?"meshdata/car1.PMD":"meshdata/track1.PMD")));VehicleGridState grid;
        if(spline_mode==2)grid.start_spline=splines.find(1);
        VehicleStripGeometry strip{};StripBinding binding;VehicleGridPayloadAudit audit;
        const bool placed=initialize_vehicle_bootstrap({*storage,assets,models,reset,splines,grid,strip,binding},
            {cars,config,control,{100,200,300,17},{0,0,1,19}},&audit);
        if(placed!=(spline_mode!=1)||reset.current!=0||grid.start_spline.has_value()!=(spline_mode!=1)||binding.calls!=1||binding.live!=&strip||strip.field_5712f8!=1)
            throw std::runtime_error("bootstrap world publication");
        for(unsigned i=0;i<8;++i){Record actual{};transfer(actual,*storage,i,false,models);
            for(std::size_t at=0;at<0x7724;at+=4){if(opaque(i,at,audit)){++opaque_words;continue;}++words;
                if(get(actual,at)!=get(expected[i],at)){
                    if(mismatches<80)std::cerr<<"case="<<n<<" slot="<<i<<" at="<<std::hex<<at<<" native="<<get(actual,at)<<" source="<<get(expected[i],at)<<std::dec<<'\n';++mismatches;
                }
            }
        }
    }
    if(reader.in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing bootstrap corpus");
    std::cout<<"{\"passed\":"<<(mismatches?"false":"true")<<",\"cases\":"<<count<<",\"words\":"<<words<<",\"opaque_words\":"<<opaque_words<<",\"mismatches\":"<<mismatches<<"}\n";
    return mismatches?1:0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}}
