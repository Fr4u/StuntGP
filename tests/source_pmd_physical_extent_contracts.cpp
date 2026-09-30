#include "source_pmd_brake_lights_fixture.hpp"
#include "sgn/contact_engine_particles.hpp"
#include "world_template_fixture.hpp"
#include <algorithm>
#include <cstring>
#include <limits>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace sgn::brake_test;
std::uint32_t word(const Bytes& bytes,std::size_t at){std::uint32_t out;check(at+4<=bytes.size(),"test raw word bounds");std::memcpy(&out,bytes.data()+at,4);return out;}
struct SparseFiles final:SourcePmdFileSource {
    SourcePmdFileSystem files;bool zero_geometry{},overlap{};
    explicit SparseFiles(const std::filesystem::path& root):files(root){}
    std::uint32_t size(std::string_view name)override{return files.size(name);}
    SourcePmdFileRead read(std::string_view name,std::span<std::uint8_t> bytes)override{
        auto result=files.read(name,bytes);auto get=[&](unsigned at){std::uint32_t v;std::memcpy(&v,bytes.data()+at,4);return v;};
        auto set=[&](unsigned at,std::uint32_t v){std::memcpy(bytes.data()+at,&v,4);};const auto d=0x148+get(0x4c);set(d+0x24,1);
        if(overlap)set(d+0x4c,get(d+0x48));
        if(zero_geometry){const auto p=0x148+get(0x48);set(p+0x24,0);set(p+0x28,0);}
        return result; //authored destination fault only, retail input untouched
    }
};
void execute(std::string_view mode,const std::filesystem::path& root){
    if(mode=="logical_count"){
        Fixture f(root);auto m=f.view();auto first=m.part(0,17);const auto physical=first.location();
        check(first.attribute_count().value()==0&&first.vertices().size()==0&&(first.flags_02().value()&4),"real zero geometry anchor missing");(void)first.transform().value();
        for(const std::uint32_t count:{17u,0u,0xffffffffu}){m.descriptor().field<std::uint32_t>(0x24).set(count);
            check(m.part_count()==count&&m.part(0,17).location()==physical,"logical count replaced physical extent");(void)m.part(0,24).flags_02().value();(void)m.part(0,74).flags_02().value();}
        rejects<FormatError>([&]{(void)m.part(0,75);});rejects<FormatError>([&]{(void)m.part(0,0xffffffffu);});
    }else if(mode=="bounds_forgery"){
        Fixture f(root);auto m=f.view();auto d=m.descriptor();const auto start=m.part(0,0).location().offset;
        const auto end=start+75*64;d.field<std::uint32_t>(0x24).set(0xffffffffu);
        for(auto at:{start+1,0u,end,static_cast<unsigned>(m.bytes().size()-32)}){
            d.field<std::uint32_t>(0x48).set(at);rejects<FormatError>([&]{(void)m.part(0,0);});}
        m.bytes().field<std::uint32_t>(0x48).set(0);m.bytes().field<std::uint32_t>(0xdc).set(m.bytes().size());
        d.field<std::uint32_t>(0x48).set(end);rejects<FormatError>([&]{(void)m.part(0,0);});
        d.field<std::uint32_t>(0x48).set(start+74*64);check(m.part(0,0).location().offset==start+74*64,"relocated alias rejected");
        rejects<FormatError>([&]{(void)m.part(0,1);});
        d.field<std::uint32_t>(0x58).set(0);d.field<std::uint32_t>(0x48).set(start+25*64);
        check(m.part(0,0).location().offset==start+25*64,"later maxLOD erased relocation history");
    }else if(mode=="sparse_ranges"){
        for(bool zero:{false,true}){
            SourceAllocationBank bank;bank.initialize_42eab0();SparseFiles files(root);files.zero_geometry=zero;SourcePmdLoadingState state;
            auto owner=load_source_pmd_42efb0(bank,state,files,"car1");auto m=owner.view();const auto first=m.part(0,0).location();
            m.descriptor().field<std::uint32_t>(0x24).set(25);
            for(unsigned index:{0u,25u,50u})check(m.part(0,index).location().offset==first.offset+index*64,"sparse ready record missing");
            for(unsigned index:{1u,17u,24u,26u,49u,51u,74u})rejects<FormatError>([&]{(void)m.part(0,index);});
            if(zero)check(m.part(0,0).attribute_count().value()==0&&m.part(0,0).vertices().size()==0,"zero geometry changed extent");
        }
        SourceAllocationBank bank;bank.initialize_42eab0();SparseFiles files(root);files.overlap=true;SourcePmdLoadingState state;
        rejects<FormatError>([&]{(void)load_source_pmd_42efb0(bank,state,files,"car1");});
    }else if(mode=="all_files"){
        unsigned accepted{},rejected{},records{};std::uint64_t untouched{};
        for(const auto& entry:std::filesystem::directory_iterator(root/"meshdata")){
            if(entry.path().extension()!=".PMD")continue;
            const auto raw=read_file(entry.path());const bool valid=word(raw,0xdc)%64==0;
            if(!valid){check(std::memcmp(raw.data(),"PMD V1.82",10)==0,"unexpected rejected PMD version");bool caught=false;
                try{Fixture f(root,entry.path().stem().string());}catch(const SourceAllocationError& e){check(e.source_site==0x42f038,"wrong original version rejection site");caught=true;}
                check(caught,"V1.82 PMD accepted");++rejected;continue;}
            check(std::memcmp(raw.data(),"PMD V1.83",10)==0,"unexpected admitted PMD version");
            Fixture f(root,entry.path().stem().string());auto m=f.view();const auto count=m.part_count(),last=m.maximum_detail_level();
            m.descriptor().field<std::uint32_t>(0x24).set(0);const auto before=snapshot(f.bank,m.allocation());
            for(unsigned lod=0;lod<=last;++lod)for(unsigned index=0;index<count;++index){(void)m.part(lod,index).flags_02().value();++records;}
            check(before==snapshot(f.bank,m.allocation()),"physical extent reads changed PMD");untouched+=before.size();++accepted;
        }
        check(accepted==78&&rejected==10&&records==1556,"actual PMD admitted extent inventory changed");
        std::cout<<"{\"accepted\":"<<accepted<<",\"rejected_version_and_stride\":"<<rejected<<",\"ready_records\":"<<records<<",\"untouched_bytes\":"<<untouched<<"}\n";
    }else if(mode=="aliases_lifetime"){
        Fixture f(root);auto m=f.view();auto part=m.part(0,17);m.descriptor().field<std::uint32_t>(0x24).set(17);
        m.descriptor().field<std::uint32_t>(0x54).set(m.descriptor().field<std::uint32_t>(0x48).value());
        check(m.part(m.source_lod_index(3),17).location()==part.location(),"raw LOD alias changed identity");
        check(m.part(1,17).transform().location()==part.transform().location(),"real LOD transform alias lost");
        Fixture independent(root);check(independent.view().part(0,17).location()!=part.location(),"independent bank identities merged");
        bool foreign_rejected=false;std::thread other([&]{try{(void)part.flags_02().value();}catch(const SourceAllocationError&){foreign_rejected=true;}});other.join();check(foreign_rejected,"foreign thread access accepted");
        const auto cached=part.flags_02();(void)f.bank.release_42e980(m.allocation());rejects<SourceAllocationError>([&]{(void)cached.value();});
        (void)f.models.load_source(f.bank,"car1");rejects<SourceAllocationError>([&]{(void)cached.value();});
        SourcePmdField<std::uint16_t> expired=[&]{Fixture owner(root);return owner.view().part(0,17).flags_02();}();
        rejects<SourceAllocationError>([&]{(void)expired.value();});
    }else if(mode=="flags_gate"){
        using namespace contact::effects;Fixture f(root);auto m=f.view();auto p17=m.part(0,17),p18=m.part(0,18);
        const auto ptr17=p17.transform().location().offset,ptr18=p18.transform().location().offset;
        auto setptr=[&](SourcePmdPartView part,std::uint32_t value){m.bytes().field<std::uint32_t>(part.location().offset+8).set(value);};
        VehicleParameters params{};params.rpm_max=6000;VehicleState car{};car.clutch=.5f;WorldGravity gravity{};gravity.inverse_factor=1;
        auto call=[&](EffectStaging& staging){VehicleParticleState state{};state.field_7628=1000;VehicleRandom random{123};EffectParticleBank bank{};
            return emit_engine_particles(params,car,state,gravity,m,staging,random,bank,fixture::cold_world_template);};
        m.descriptor().field<std::uint32_t>(0x24).set(17);p17.flags().set(4);p17.flags_02().set(0);setptr(p17,0xffffffffu);
        EffectStaging a{};check(call(a)==std::array<std::optional<unsigned>,2>{},"flags00 incorrectly enabled first emitter");
        p17.flags_02().set(4);rejects<FormatError>([&]{(void)call(a);});
        p17.flags_02().set(0);p18.flags_02().set(4);setptr(p18,ptr18);m.descriptor().field<std::uint32_t>(0x24).set(19);
        EffectStaging second{};const auto emitted=call(second);check(!emitted[0]&&emitted[1]&&second.initialized==0,"source second-only path initialized or skipped");
        p18.flags_02().set(0);setptr(p18,0xffffffffu);EffectStaging neither{};check(call(neither)==std::array<std::optional<unsigned>,2>{},"disabled second touched invalid transform");
        p17.flags().set(0);p17.flags_02().set(4);setptr(p17,ptr17);m.descriptor().field<std::uint32_t>(0x24).set(17);
        EffectStaging valid{};const auto single=call(valid);check(single[0]&&!single[1]&&valid.initialized==1&&m.part_count()==17,"physical zero geometry first anchor not emitted");
    }else throw std::runtime_error("unknown physical extent contract");
}
}
int main(int argc,char** argv)try{if(argc!=3)throw std::runtime_error("expected physical extent contract and retail root");execute(argv[1],argv[2]);std::cout<<"passed "<<argv[1]<<'\n';return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
