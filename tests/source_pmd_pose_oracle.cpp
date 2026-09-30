#include "source_pmd_pose_fixture.hpp"
#include <fstream>
#include <iostream>
#include <memory>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::retail::prepare_full_test;
std::uint64_t calls{},continued{},words{},opaque{},retained{},alias_checks{},immutable_bytes{};
template<class T>void read(std::ifstream& in,T& value){in.read(reinterpret_cast<char*>(&value),sizeof(value));require(bool(in),"truncated prepare corpus");}
unsigned number(std::ifstream& in){unsigned value;read(in,value);return value;}
Bytes blob(std::ifstream& in){const auto n=number(in);require(n>0&&n<0x100000,"prepare blob size");Bytes value(n);in.read(reinterpret_cast<char*>(value.data()),n);require(bool(in),"truncated prepare blob");return value;}
template<class A,class B>void compare(const A& actual,const B& expected,const char* label){
    static_assert(sizeof(A)==sizeof(B)&&sizeof(A)%4==0);const auto a=std::bit_cast<std::array<unsigned,sizeof(A)/4>>(actual),e=std::bit_cast<std::array<unsigned,sizeof(B)/4>>(expected);
    for(unsigned i=0;i<a.size();++i){++words;if(a[i]!=e[i]){std::cerr<<label<<" call="<<calls<<" offset="<<i*4<<" actual="<<std::hex<<a[i]<<" expected="<<e[i]<<std::dec<<'\n';throw FormatError("prepare source comparison mismatch");}}
}
}
int main(int argc,char** argv)try{
    require(argc==3,"expected prepare corpus and retail root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);
    require(std::string_view(magic.data(),8)=="SGNPRP01","prepare corpus signature");const auto groups=number(in);require(groups>0&&groups<=4,"prepare group count");
    AssetStore assets(argv[2]);std::set<unsigned> distinct;
    for(unsigned group=0;group<groups;++group){require(number(in)==6,"prepare physical model count");
        SourceAllocationBank bank;bank.initialize_42eab0();ModelResources resources(assets,argv[2]);std::array<SourcePmdModelView,6> models{};std::array<std::size_t,6> handles{};std::vector<CanonicalModelLayout> layouts;
        std::array<Car,6> cars;std::array<Mask,6> taint;Mask basis_taint;std::array<unsigned,6> invocations{};
        for(unsigned slot=0;slot<6;++slot){const auto id=number(in),base=number(in);require(id>=1&&id<=20,"prepare car identity");distinct.insert(id);
            const auto name="car"+std::to_string(id);auto authored=blob(in),live=blob(in);
            require(authored==read_file(std::filesystem::path(argv[2])/"meshdata"/(name+".PMD")),"prepare authored PMD mismatch");
            const auto token=resources.load_source(bank,name);handles[slot]=*resources.resolve(token);models[slot]=resources.source_at(handles[slot]);layouts.emplace_back(authored,base);
            layouts.back().load_image(models[slot],authored,live);
            read(in,cars[slot]);require(cars[slot][0x26b0/4]==base+layouts.back().descriptor,"prepare source model binding mismatch");
        }
        VehicleMatrix basis;read(in,basis);const auto count=number(in);require(count>0&&count<=1024,"prepare call count");
        for(unsigned step=0;step<count;++step){const auto slot=number(in),lod=number(in),patch_count=number(in);
            require(slot<6&&lod<4&&patch_count<=128,"prepare invocation bounds");auto& car=cars[slot];auto& model=models[slot];const auto& layout=layouts[slot];
            std::set<unsigned> patched;for(unsigned j=0;j<patch_count;++j){const auto at=number(in),value=number(in);
                require(stimulus_word(at)&&patched.insert(at).second,"prepare undeclared or repeated input patch");car[at/4]=value;}
            model.current_detail_level().set(lod);VehicleOwners vehicle;vehicle.transfer(car,true);
            const Table before=layout.table(model);Mask mask=basis_taint;mask.insert(taint[slot].begin(),taint[slot].end());
            const auto expected_w=expected_lanes(layout,lod,vehicle,before,basis,mask);const auto immutable=layout.immutable(bank,model);
            prepare_vehicle_model(vehicle.physics,vehicle.inputs,vehicle.wheels,vehicle.presentation,vehicle.anchors,vehicle.segments,resources,handles[slot],basis,vehicle.effect);
            require(immutable==layout.immutable(bank,model),"prepare immutable PMD state mismatch");immutable_bytes+=immutable.size();alias_checks+=layout.verify_aliases(model);
            vehicle.transfer(car,false);Car expected_car;read(in,expected_car);compare(car,expected_car,"vehicle");
            VehicleMatrix expected_basis;Table expected_table;read(in,expected_basis);read(in,expected_table);
            const auto given_count=number(in);require(given_count<=128,"prepare mask extent");Mask given;
            for(unsigned j=0;j<given_count;++j){const auto at=number(in);require(at<516&&given.insert(at).second,"prepare duplicate or out of bounds mask");}
            require(given==mask,"prepare exact source W mask mismatch");
            const Table table=layout.table(model);
            const auto a=std::bit_cast<std::array<unsigned,500>>(table),e=std::bit_cast<std::array<unsigned,500>>(expected_table);
            const auto ab=std::bit_cast<std::array<unsigned,16>>(basis),eb=std::bit_cast<std::array<unsigned,16>>(expected_basis);
            for(unsigned at=0;at<516;++at){const auto value=at<16?ab[at]:a[at-16],wanted=at<16?eb[at]:e[at-16];
                if(mask.contains(at)){++opaque;continue;}++words;if(value!=wanted){std::cerr<<"model/basis call="<<calls<<" lane="<<at<<" actual="<<std::hex<<value<<" expected="<<wanted<<std::dec<<'\n';throw FormatError("prepare source comparison mismatch");}}
            for(const auto& [at,wanted]:expected_w){++retained;require((at<16?ab[at]:a[at-16])==wanted,"prepare native fourth-lane retention mismatch");}
            basis_taint.clear();taint[slot].clear();for(auto at:mask)if(at<16)basis_taint.insert(at);else taint[slot].insert(at);
            continued+=invocations[slot]++!=0;++calls;
        }
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing prepare corpus data");
    std::cout<<"{\"passed\":true,\"models\":"<<distinct.size()<<",\"calls\":"<<calls<<",\"continued_model_calls\":"<<continued
        <<",\"defined_words\":"<<words<<",\"source_undefined_words\":"<<opaque<<",\"native_fourth_lane_checks\":"<<retained
        <<",\"authored_alias_checks\":"<<alias_checks<<",\"immutable_native_bytes_checked\":"<<immutable_bytes<<",\"mismatches\":0,\"allowed_ulp\":0}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}
