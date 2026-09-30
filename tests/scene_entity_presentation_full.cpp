#include "scene_entity_presentation_state.hpp"
#include <iostream>
#include <memory>

using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::presentation_test;
namespace {
void read(std::ifstream& in,void* data,std::size_t size){in.read(static_cast<char*>(data),static_cast<std::streamsize>(size));require(bool(in),"presentation truncated corpus");}
std::uint32_t number(std::ifstream& in){std::uint32_t value;read(in,&value,4);return value;}
void patch(std::ifstream& in,SourceImage& image,SourceImage* second=nullptr){
    const auto count=number(in);require(count<=image.bytes.size()/4,"presentation patch count");
    std::optional<std::uint32_t> previous;
    for(unsigned i=0;i<count;++i){const auto at=number(in),value=number(in);
        require(at%4==0&&at<=image.bytes.size()-4&&(!previous||at>*previous),"presentation patch bounds/order");
        std::memcpy(image.bytes.data()+at,&value,4);if(second)std::memcpy(second->bytes.data()+at,&value,4);previous=at;
    }
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected presentation corpus and retail root");std::ifstream in(argv[1],std::ios::binary);
    std::array<char,8> magic{};read(in,magic.data(),8);require(std::string_view(magic.data(),8)=="SGNSCE04","presentation corpus signature");
    const auto groups=number(in);require(groups>0&&groups<=16,"presentation group bounds");
    AssetStore assets(argv[2]);std::uint64_t calls{},continued{},compared{},excluded{},cars{},shadows{},strips{},color_checks{};
    for(unsigned group=0;group<groups;++group){
        require(number(in)==group,"presentation group index");const auto steps=number(in),regions=number(in),size=number(in);
        require(steps>0&&steps<=26&&regions==28&&size>0&&size<=32*1024*1024&&size%4==0,"presentation group extent");
        SourceImage actual;actual.regions.resize(regions);read(in,actual.regions.data(),regions*sizeof(Region));
        std::uint32_t offset{};std::set<std::pair<std::uint32_t,std::uint32_t>> owners;
        for(const auto& region:actual.regions){
            require(region.kind<=6&&owners.emplace(region.kind,region.index).second&&region.offset==offset&&region.length%4==0&&region.length<=size-offset,"presentation owner bounds/order");
            offset+=region.length;
        }
        require(offset==size&&actual.region(0).address==0x470000&&actual.region(0).length==0x1c8000,"presentation global region");
        actual.bytes.resize(size);read(in,actual.bytes.data(),size);SourceImage expected=actual;
        auto state=std::make_unique<sgn::retail::scene_test::State>(assets,argv[2],actual);
        for(unsigned step=0;step<steps;++step){
            patch(in,actual,&expected); //declared caller inputs apply to BOTH retained states
            const auto input_count=number(in);require(input_count<=size/4,"scene input provenance count");
            std::set<std::uint32_t> input_tags;
            for(unsigned i=0;i<input_count;++i){const auto at=number(in);require(at<size&&at%4==0&&input_tags.insert(at).second,"scene input provenance bounds/duplicate");}
            state->transfer(actual,true);
            state->input_provenance(actual,input_tags);
            const auto draws=prepare_scene_entity_presentation(state->scene_bindings());
            state->transfer(actual,false);
            patch(in,expected); //expected output never enters native state
            const auto count=number(in);require(count<=size/4,"presentation opaque count");
            std::set<std::uint32_t> unknown;
            for(unsigned i=0;i<count;++i){const auto at=number(in);require(at<size&&at%4==0&&unknown.insert(at).second,"presentation opaque bounds/duplicate");}
            for(unsigned i=0;i<2048;++i){
                const bool source_tag=unknown.contains(actual.region(3,3).offset+i*16+12);
                if(state->color_audit.source_undefined[i]!=source_tag){
                    std::cerr<<"scene color provenance mismatch group="<<group<<" step="<<step<<" attribute="<<i<<'\n';return 1;
                }
                ++color_checks;
            }
            const auto order_count=number(in);require(order_count>=6&&order_count<=66,"presentation call order extent");
            unsigned expected_cars{};
            for(unsigned i=0;i<order_count;++i){const auto entry=number(in),vehicle=number(in);
                if(i==0)require(entry==0x442500,"scene source parent order");
                else if(i==1)require(entry==0x418180,"scene source vehicle stage order");
                else if(i==2)require(entry==0x41d1c0&&vehicle==vehicle_base,"scene source strip order");
                if(i==order_count-3)require(entry==0x45e4f0&&vehicle==vehicle_base,"scene effect stage order");
                if(i==order_count-2)require(entry==0x416050&&vehicle==vehicle_base,"scene impact stage order");
                if(i==order_count-1)require(entry==0x449d60&&vehicle==vehicle_base,"scene terminal return order");
                if(entry==0x42b850)++expected_cars;
            }
            unsigned native_cars{};for(unsigned i=0;i<6;++i){native_cars+=draws.cars[i].has_value();shadows+=draws.shadows[i].has_value();}
            require(native_cars==expected_cars,"presentation active slot count");cars+=native_cars;strips+=draws.strip.has_value();
            for(unsigned at=0;at<size;at+=4){
                if(unknown.contains(at)){++excluded;continue;}
                std::uint32_t a,e;std::memcpy(&a,actual.bytes.data()+at,4);std::memcpy(&e,expected.bytes.data()+at,4);++compared;
                if(a!=e){
                    const auto region=std::find_if(actual.regions.begin(),actual.regions.end(),[&](const Region& r){return r.offset<=at&&at<r.offset+r.length;});
                    std::cerr<<"presentation comparison mismatch group="<<group<<" step="<<step<<" address="<<std::hex<<region->address+at-region->offset
                             <<" actual="<<a<<" expected="<<e<<std::dec<<'\n';return 1;
                }
            }
            continued+=step!=0;++calls;
        }
    }
    require(in.peek()==std::char_traits<char>::eof(),"presentation trailing corpus");
    std::cout<<"{\"passed\":true,\"calls\":"<<calls<<",\"retained_calls\":"<<continued<<",\"defined_words\":"<<compared
             <<",\"source_unwritten_words\":"<<excluded<<",\"cars\":"<<cars<<",\"shadows\":"<<shadows<<",\"strips\":"<<strips
             <<",\"color_provenance_checks\":"<<color_checks
             <<",\"mismatches\":0,\"allowed_ulp\":0}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
