#include "source_pmd_presentation_fixture.hpp"
using namespace sgn;using namespace sgn::presentation_test;
namespace {
void retail40(const std::filesystem::path& root){
    const std::array<std::array<unsigned,3>,8> gates{{{0,0,0},{0,1,1},{1,1,1},{1,0,1},{1,2,1},{2,1,0},{0xffffffffu,1,2},{0,2,2}}};
    unsigned calls{},shadows{};std::uint64_t intact{};
    for(unsigned id=1;id<=20;++id){auto name="car"+std::to_string(id);Fixture f(root,name);ModelResources legacy(f.assets,root);const auto handle=*legacy.resolve(legacy.load(name));auto& old=legacy.at(handle);auto m=f.view();
        for(unsigned raw=0;raw<4;++raw){m.current_detail_level().set(raw);old.current_detail_level()=raw;const auto lod=m.source_lod_index(raw),old_lod=old.source_lod_index(raw);
            for(auto g:gates){VehiclePresentationEffects e{g[0],g[1],g[2],{2.25f,-0.0f,17},{-3.5f,41,.125f}};
                for(unsigned i:{11u,12u,13u,14u,15u,16u,19u,20u}){m.part(lod,i).flags().set(0xbeef);old.source_part(old_lod,i).flags=0xbeef;}
                VehiclePresentationState a{91,92,93,94,95},b=a;const auto before=snapshot(f.bank,m.allocation());const auto allowed=footprint(m,lod,e);
                prepare_vehicle_presentation(a,e,f.models,f.handle);prepare_vehicle_presentation(b,e,legacy,handle);check(fields(a)==fields(b),"retail presentation state differs");
                for(unsigned i:{11u,12u,13u,14u,15u,16u,19u,20u})check(m.part(lod,i).flags().value()==old.source_part(old_lod,i).flags,"retail presentation flags differ");
                for(unsigned i:{19u,20u}){const auto value=m.part(lod,i).transform().value();check(std::memcmp(&value.scale,&old.source_part(old_lod,i).transform.scale,12)==0,"retail presentation scale differs");}
                intact+=unchanged(before,snapshot(f.bank,m.allocation()),allowed);++calls;
            }
        }
        Fixture shadow(root,name+"sh");VehiclePresentationState state{};rejects<FormatError>([&]{prepare_vehicle_presentation(state,{},shadow.models,shadow.handle);});check(state.field_3134==1&&state.field_33c8==0,"shadow missing source prefix");++shadows;
    }
    check(calls==640&&shadows==20,"presentation actual model coverage");std::cout<<"{\"car_calls\":"<<calls<<",\"shadow_rejections\":"<<shadows<<",\"unchanged_pmd_bytes\":"<<intact<<"}\n";
}
void pointer_order(const std::filesystem::path& root){
    Fixture f(root);auto m=f.view();VehiclePresentationState s{0,92,93,94,95};VehiclePresentationEffects e{};
    m.current_detail_level().set(4);rejects<FormatError>([&]{prepare_vehicle_presentation(s,e,f.models,f.handle);});check(s.field_3134==1&&s.field_3928==95,"3134 must precede invalid LOD guard");m.current_detail_level().set(0);
    auto first=m.part(0,19),second=m.part(0,20);const auto a=first.transform().location().offset;
    auto pointer=[&](SourcePmdPartView p,unsigned value){m.bytes().field<unsigned>(p.location().offset+8).set(value);};pointer(first,0xffffffffu);pointer(second,0xffffffffu);
    prepare_vehicle_presentation(s,e,f.models,f.handle);e={0,1,1,{2,3,4},{5,6,7}};prepare_vehicle_presentation(s,e,f.models,f.handle);check(first.flags().value()==0&&second.flags().value()==0,"deactivation dereferenced transform");
    e={1,0,1,{2,3,4},{5,6,7}};second.flags().set(0x77);prepare_vehicle_presentation(s,e,f.models,f.handle);check(second.flags().value()==0x77,"second activation escaped first availability guard");
    e.field_328c=1;s.field_33c8=92;s.field_3508=93;rejects<FormatError>([&]{prepare_vehicle_presentation(s,e,f.models,f.handle);});check(first.flags().value()==0&&s.field_3928==94&&s.field_33c8==92&&s.field_3508==93,"first pointer guard or footer order");
    pointer(first,a);second.flags().set(0);rejects<FormatError>([&]{prepare_vehicle_presentation(s,e,f.models,f.handle);});
    check(first.flags().value()==1&&second.flags().value()==1&&first.transform().value().scale.x==2&&s.field_33c8==92,"second pointer guard lost preceding writes");
}
void alias_current(const std::filesystem::path& root){
    Fixture f(root);auto m=f.view();VehiclePresentationEffects e{1,1,1,{2,3,4},{7,8,9}};VehiclePresentationState s{};
    m.descriptor().field<unsigned>(0x24).set(0); //413DA0 has no logical-count predicate
    m.current_detail_level().set(1);const auto first=m.part(1,19),second=m.part(1,20);m.bytes().field<unsigned>(second.location().offset+8).set(first.transform().location().offset);
    const auto before=first.transform().value();prepare_vehicle_presentation(s,e,f.models,f.handle);const auto value=first.transform().value();
    check(value.scale.x==7&&value.scale.y==8&&value.scale.z==9&&std::bit_cast<unsigned>(value.scale.w)==std::bit_cast<unsigned>(before.scale.w),"shared transform write order or W retention");
    check(m.part_count()==0,"presentation repaired logical count");m.part(0,11).flags().set(0);m.current_detail_level().set(0);prepare_vehicle_presentation(s,e,f.models,f.handle);check(m.part(0,11).flags().value()==1,"current LOD cached");
    m.descriptor().field<unsigned>(0x54).set(m.descriptor().field<unsigned>(0x4c).value());m.current_detail_level().set(3);m.part(1,11).flags().set(0);prepare_vehicle_presentation(s,e,f.models,f.handle);check(m.part(1,11).flags().value()==1,"rawslot alias lost");
}
struct SparseFiles final:SourcePmdFileSource {
    SourcePmdFileSystem files;explicit SparseFiles(const std::filesystem::path& root):files(root){}
    unsigned size(std::string_view name)override{return files.size(name);}
    SourcePmdFileRead read(std::string_view name,std::span<std::uint8_t> b)override{const auto r=files.read(name,b);unsigned offset;std::memcpy(&offset,b.data()+0x4c,4);const unsigned count=13;std::memcpy(b.data()+0x148+offset+0x24,&count,4);return r;}
};
void sparse_prefix(const std::filesystem::path& root){
    SourceAllocationBank bank;bank.initialize_42eab0();SparseFiles files(root);SourcePmdLoadingState loading;auto owner=load_source_pmd_42efb0(bank,loading,files,"car1");auto m=owner.view();
    m.part(0,11).flags().set(0);m.part(0,12).flags().set(0);VehiclePresentationState s{0,2,3,4,5};
    rejects<FormatError>([&]{prepare_vehicle_presentation(s,{},m);});check(s.field_3134==1&&s.field_3928==5&&s.field_33c8==2,"sparse guard scalar prefix");
    check(m.part(0,11).flags().value()==1&&m.part(0,12).flags().value()==1,"six ordered flag stores were preflighted as a batch");
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root),other(root);VehiclePresentationState s{};f.bank.release_42eaa0();rejects<SourceAllocationError>([&]{prepare_vehicle_presentation(s,{},f.models,f.handle);});
    rejects<std::out_of_range>([&]{prepare_vehicle_presentation(s,{},other.models,std::size_t(-1));});
    prepare_vehicle_presentation(s,{},other.models,other.handle);bool caught=false;
    std::thread worker([&]{try{VehiclePresentationState t{};prepare_vehicle_presentation(t,{},other.models,other.handle);}catch(const SourceAllocationError&){caught=true;}});worker.join();check(caught,"foreign thread presentation accepted");
}
}
int main(int argc,char** argv)try{check(argc==3,"expected presentation contract and retail root");std::string_view mode=argv[1];
    if(mode=="retail40")retail40(argv[2]);else if(mode=="pointer_order")pointer_order(argv[2]);else if(mode=="alias_current")alias_current(argv[2]);else if(mode=="sparse_prefix")sparse_prefix(argv[2]);else if(mode=="lifetime")lifetime(argv[2]);else throw std::runtime_error("unknown presentation contract");
    std::cout<<"passed "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
