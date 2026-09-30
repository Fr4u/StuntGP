#include "sgn/model_anchor_adapter.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <memory>

namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
template<class F>void rejects(F&& action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}check(caught,"expected PMD admission/lifetime rejection");}
template<class F>void rejects_at(F&& action,std::uint32_t site){
    bool caught=false;try{action();}catch(const SourceAllocationError& error){check(error.source_site==site,"wrong PMD source boundary");caught=true;}
    check(caught,"expected typed PMD source boundary");
}
bool live(const SourceAllocationBank& bank,const SourceAllocationRef& allocation){
    try{(void)bank.bytes(allocation);return true;}catch(const SourceAllocationError&){return false;}
}
std::uint32_t word(std::span<const std::uint8_t> bytes,std::size_t at){
    check(at<=bytes.size()&&bytes.size()-at>=4,"receiving raw word extent");std::uint32_t out;std::memcpy(&out,bytes.data()+at,4);return out;
}
struct Files final:SourcePmdFileSource {
    SourcePmdFileSystem actual;const SourceAllocationBank& bank;
    unsigned sizes{},reads{};bool corrupt_version{},short_read{};
    Files(const std::filesystem::path& root,const SourceAllocationBank& b):actual(root),bank(b){}
    std::uint32_t size(std::string_view name) override{++sizes;return actual.size(name);}
    SourcePmdFileRead read(std::string_view name,std::span<std::uint8_t> destination) override{
        ++reads;const auto rows=bank.source_records();const auto address=reinterpret_cast<std::uintptr_t>(destination.data());
        check(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.raw&&row.aligned==address&&row.requested_bytes==destination.size()&&row.category==0;}),
            "actual PMD file destination is not canonical bank memory");
        const auto result=actual.read(name,short_read?destination.first(destination.size()-1):destination);
        if(corrupt_version)destination[0]='X'; //fault only in actual destination, never retail file mutation
        return result;
    }
};
struct Audit final:SourcePmdObserver {
    const SourceAllocationBank& expected;std::vector<SourcePmdEvent> events;std::uint32_t abort_site{};
    explicit Audit(const SourceAllocationBank& bank):expected(bank){}
    void observe(const SourcePmdEvent& event,const SourceAllocationBank& bank,const SourcePmdLoadingState& state) override{
        check(&bank==&expected,"PMD observer received another bank");events.push_back(event);
        if(event.kind==SourcePmdEventKind::allocation&&event.allocation){
            const auto bytes=bank.bytes(event.allocation);check(bytes.size()==event.requested_bytes&&reinterpret_cast<std::uintptr_t>(bytes.data())%32==0,"PMD bank extent/alignment");
            check(std::all_of(bytes.begin(),bytes.end(),[](auto byte){return byte==0;}),"PMD actual allocation not zero-filled");
        }
        if(event.kind==SourcePmdEventKind::globals)check(state.file==event.allocation&&state.file_size==event.requested_bytes&&state.version==0,"PMD global publication order");
        if(event.kind==SourcePmdEventKind::complete)check(state.version==0x12&&state.descriptor_file==state.file,"PMD completed loader state");
        if(event.site==abort_site)throw std::runtime_error("explicit read-only PMD observer abort");
    }
};
struct Fixture {
    AssetStore assets;SourceAllocationBank bank;Files files;ModelResources resources;Audit audit;
    Fixture(const std::filesystem::path& root,bool initialize=true):assets(root),files(root,bank),resources(assets,root),audit(bank){if(initialize)bank.initialize_42eab0();}
    std::uint32_t load(std::string name="car1"){return resources.load_source(bank,files,name,&audit);}
    SourcePmdModelView model(std::uint32_t id){return resources.source_at(*resources.resolve(id));}
};
struct Owned {
    std::array<VehicleRaceState,6> race{};std::array<VehiclePresentationState,6> presentation{};
    std::array<VehicleTrailInputs,6> trails{};std::array<VehicleAnchorState,6> anchors{};unsigned current=5;
    std::array<VehicleAnchorVehicle,6> view(const VehicleAnchorModel& model){
        auto make=[&](unsigned i){return VehicleAnchorVehicle{race[i],presentation[i],trails[i],anchors[i],&model};};
        return {{make(0),make(1),make(2),make(3),make(4),make(5)}};
    }
};
void execute(std::string_view name,const std::filesystem::path& root,const std::filesystem::path& fixture){
    if(name=="canonical"){
        Fixture f(root);const auto id=f.load();const auto model=f.model(id);const auto raw=read_file(root/"meshdata"/"car1.PMD");
        const auto expected=read_file(fixture/"car1-source-offset-relocation.bin");const auto actual=f.bank.bytes(model.allocation());
        check(actual.size()==raw.size()&&std::equal(actual.begin(),actual.end(),expected.begin(),expected.end()),"complete canonical PMD differs from static instruction-derived relocation bytes");
        const std::vector<std::uint32_t> sites{0x42efe0,0x42efed,0x42f006,0x42f03d,0x42f042,0x42f047,0x42f04c,0x42f080};
        check(f.audit.events.size()==sites.size(),"PMD producer event count");
        for(std::size_t i=0;i<sites.size();++i)check(f.audit.events[i].site==sites[i]&&f.audit.events[i].allocation==model.allocation(),"PMD source producer order/identity");
        check(f.files.sizes==1&&f.files.reads==1&&f.bank.live_count()==1&&f.bank.source_words().next_ordinal==1&&f.bank.source_words().ordinary_bytes==raw.size()+36,"PMD source allocation accounting");
        check(f.resources.source_loading_state().descriptor_offset==328+word(raw,0x4c),"PMD descriptor global identity");
        for(unsigned slot=0;slot<4;++slot){const auto lod=model.source_lod_index(slot);check(lod<=model.maximum_detail_level(),"source LOD alias resolution");}
        rejects([&]{(void)f.resources.at(*f.resources.resolve(id));}); //no hidden legacy geometry snapshot
    }else if(name=="mutations"){
        Fixture f(root);const auto source=f.model(f.load());ModelAnchorAdapter adapter(source,0);const auto part=source.part(0,11);
        auto transform=part.transform().value();transform.position={23,45,67,89};transform.forward={0.25f,0.5f,-0.75f,12};part.transform().set(transform);part.flags().set(17);
        auto point=part.vertices().at(0).value();point.z=65000;part.vertices().at(0).set(point);
        const auto anchor=adapter.part(11,0);check(anchor.flags==17&&std::memcmp(&anchor.position,&transform.position,16)==0&&adapter.maximum_vertex_z(11,0)==65000,"actual anchor adapter read stale copied PMD state");
        Owned owned;owned.race[0].active=1;const auto expected=65000*std::bit_cast<float>(0x3f7d70a4u);auto views=owned.view(adapter);
        initialize_vehicle_extent_anchors({views,owned.current});
        check(owned.current==0&&owned.presentation[0].field_3134==1&&std::bit_cast<std::uint32_t>(owned.anchors[0].extent)==std::bit_cast<std::uint32_t>(expected),"actual417E10 consumer ignored bank vertex mutation");
        check(std::memcmp(&owned.anchors[0].position,&transform.position,16)==0&&std::bit_cast<std::uint32_t>(owned.anchors[0].forward.x)==(std::bit_cast<std::uint32_t>(transform.forward.x)^0x80000000u),"actual417E10 consumer ignored bank transform mutation");
        const auto body=source.part(0,0);const auto header=body.attributes().at(0);auto h=header.value();h[2]^=0x10;header.set(h);
        const auto corner=body.coordinates().at(0);auto c=corner.value();c[3]^=0x100;corner.set(c);
        auto normal=source.normals().at(0).value();normal.w=37;source.normals().at(0).set(normal);
        check(body.attributes().at(0).value()==h&&body.coordinates().at(0).value()==c&&source.normals().at(0).value().w==37,"polygon-related projections lost canonical mutations");
        const auto bytes=f.bank.bytes(source.allocation());check(std::memcmp(bytes.data()+header.location().offset,h.data(),8)==0&&std::memcmp(bytes.data()+corner.location().offset,c.data(),16)==0,"projected writes went to shadow payloads");
        const auto location=part.transform().location();unsigned aliases=0;
        for(unsigned lod=0;lod<=source.maximum_detail_level();++lod)for(unsigned i=0;i<source.part_count();++i)
            if(source.part(lod,i).transform().location()==location){check(source.part(lod,i).transform().value().position.z==67,"shared transform identity copied");++aliases;}
        check(aliases>1,"real car1 fixture must exercise shared transform across LODs");
    }else if(name=="shared_skin"){
        Fixture f(root);const auto id=f.load();const auto source=f.model(id);const auto before=f.bank.bytes(source.allocation());const auto old=Bytes(before.begin(),before.end());
        std::array<LevelCarSlot,6> cars{};std::array<std::int8_t,6> random{};
        cars[0].source_word00=cars[1].source_word00=id;cars[0].settings.livery=3;cars[1].settings.livery=7;cars[0].settings.tyre=2;cars[1].settings.tyre=4;
        remap_level_vehicle_textures(f.resources,cars,random);unsigned matched=0;
        for(unsigned lod=0;lod<=source.maximum_detail_level();++lod)for(std::uint32_t i=0;i<source.texture_count();++i){const auto value=source.texture_name(i);
            if(value.find("grid")!=value.npos){check(value.ends_with("grid7"),"LOD did not observe final shared livery write");++matched;}
            if(value.find("tyre")!=value.npos){check(value.ends_with("tyre4"),"LOD did not observe final shared tyre write");++matched;}}
        check(matched==9,"real car1 shared namespace coverage");
        const auto after=f.bank.bytes(source.allocation());check(!std::equal(old.begin(),old.end(),after.begin()),"skin change never reached actual bank bytes");
        check(f.bank.live_count()==1&&f.bank.source_words().next_ordinal==1,"skin remap invented allocations");
    }else if(name=="repeat_and_free"){
        Fixture f(root);const auto id1=f.load(),id2=f.load();const auto a=f.model(id1),b=f.model(id2);const auto old=a.part(0,0).flags();const auto array=a.part(0,0).vertices();ModelAnchorAdapter adapter(a,0);
        check(id1!=id2&&!(a.allocation()==b.allocation())&&f.files.sizes==2&&f.files.reads==2&&f.bank.live_count()==2&&f.bank.source_words().next_ordinal==2,"same-name PMD request was deduplicated/cached as a model");
        const auto second=b.part(0,0).flags().value();old.set(static_cast<std::uint16_t>(second^0x101));check(b.part(0,0).flags().value()==second,"separate PMD requests share mutable state");
        (void)f.bank.release_42e980(a.allocation());
        rejects([&]{(void)old.value();});rejects([&]{old.set(1);});rejects([&]{(void)array.size();});rejects([&]{(void)adapter.part(0,0);});rejects([&]{(void)f.model(id1);});
        check(b.part_count()==25&&f.bank.live_count()==1,"free of one PMD invalidated unrelated live instance");
        const auto id3=f.load();const auto next=f.model(id3);check(id3!=id1&&!(next.allocation()==f.audit.events.front().allocation),"reloaded PMD reused stale identity/generation");rejects([&]{(void)old.value();});
    }else if(name=="owner_lifetime"){
        AssetStore assets(root);SourceAllocationBank bank;bank.initialize_42eab0();
        auto resources=std::make_unique<ModelResources>(assets,root);const auto id=resources->load_source(bank,"car1");const auto view=resources->source_at(*resources->resolve(id));const auto ref=view.allocation();const auto flags=view.part(0,0).flags();
        resources.reset();check(bank.live_count()==1&&bank.bytes(ref).size()==72096,"ModelResources destructor illegally source-freed actual PMD");
        rejects([&]{view.check();});rejects([&]{(void)flags.value();});(void)bank.release_42e980(ref);
    }else if(name=="scope"){
        Fixture f(root);const auto a=f.model(f.load());const auto ref=a.allocation();f.bank.begin_42ea80(); //core-only fixture event, not production warm transition
        check(!live(f.bank,ref)&&f.bank.live_count()==0,"source scope failed to release real PMD row");rejects([&]{a.check();});
        const auto b=f.model(f.load());check(b.part_count()==25&&f.bank.source_words().next_ordinal==1,"new PMD request after explicit source scope");
    }else if(name=="high_ordinal"){
        Fixture f(root);for(unsigned i=0;i<384;++i){const auto ref=f.bank.allocate_42e8d0(1,0);(void)f.bank.release_42e980(ref);} //synthetic ordinal prefix
        const auto model=f.model(f.load());f.bank.begin_42ea80();
        check(model.part_count()==25&&f.bank.live_count()==1&&f.bank.source_words().ordinary_bytes==72132,"exact bounded source sweep must retain ordinal384 PMD");
    }else if(name=="uninitialized"){
        Fixture f(root,false);rejects([&]{(void)f.load();});check(f.files.sizes==0&&f.files.reads==0&&f.bank.live_count()==0&&f.resources.size()==0,"uninitialized PMD had source effects");
    }else if(name=="missing"){
        Fixture f(root);rejects_at([&]{(void)f.load("__missing_source_pmd_fixture__");},0x42f038);
        check(f.files.sizes==1&&f.files.reads==1&&f.bank.live_count()==1&&f.resources.size()==0,"missing PMD lost source prefix");
        check(f.bank.bytes(f.resources.source_loading_state().file).empty()&&f.resources.source_loading_state().version==0&&f.bank.source_words().ordinary_bytes==36,"missing PMD must retain actual zero-size row and globals");
    }else if(name=="invalid_version"||name=="short_read"){
        Fixture f(root);f.files.corrupt_version=name=="invalid_version";f.files.short_read=name=="short_read";
        rejects_at([&]{(void)f.load();},name=="invalid_version"?0x42f038u:0x42f010u);
        check(f.bank.live_count()==1&&f.resources.size()==0&&f.resources.source_loading_state().file_size==72096&&f.resources.source_loading_state().version==0,"invalid PMD erased actual file/global prefix");
    }else if(name=="full_bank"){
        Fixture f(root);const auto model=f.model(f.load());const auto original=model.allocation();
        for(unsigned i=0;i<383;++i)check(bool(f.bank.allocate_42e8d0(1,0)),"synthetic capacity prefix");
        rejects_at([&]{(void)f.load();},0x42efed);
        check(f.files.sizes==2&&f.files.reads==1&&f.resources.size()==1&&f.resources.source_loading_state().file==original&&f.bank.live_count()==384,"null PMD allocation mutated prior source globals or performed unsafe read");
    }else if(name=="observer_abort"){
        Fixture f(root);f.audit.abort_site=0x42f042;rejects([&]{(void)f.load();});
        check(f.bank.live_count()==1&&f.resources.size()==0&&f.resources.source_loading_state().version==0x12&&f.resources.source_loading_state().descriptor_file==f.resources.source_loading_state().file,"observer abort lost reached source relocation prefix");
    }else if(name=="track_complete"){
        Fixture f(root);const auto track=f.model(f.load("track1")).track();
        check(f.bank.live_count()==1&&f.resources.size()==1&&f.resources.source_loading_state().version==0x12&&f.audit.events.back().site==0x42f080,"track loader did not complete exact seven-child chain");
        check(track.spline_count()==5&&track.generated_normals().size()==82,"actual TRACK1 relocation/projection differs");
    }else if(name=="mode_and_bounds"){
        Fixture f(root);const auto id=f.load();const auto model=f.model(id);SourceAllocationBank other;other.initialize_42eab0();
        rejects([&]{(void)f.resources.load("car1");});rejects([&]{(void)f.resources.load_source(other,"car1");});
        check(f.resources.size()==1&&other.live_count()==0&&f.files.sizes==1,"mode/bank rejection mutated source history");
        ModelResources legacy(f.assets,root);(void)legacy.load("car1");rejects([&]{(void)legacy.load_source(f.bank,"car1");});
        const auto part=model.part(0,0);const auto address=part.location().offset+8;const auto saved=model.bytes().field<std::uint32_t>(address).value();
        model.bytes().field<std::uint32_t>(address).set(72080);rejects([&]{(void)part.transform().value();});model.bytes().field<std::uint32_t>(address).set(saved);
        rejects([&]{(void)model.part(0,1000000);});rejects([&]{(void)model.bytes().field<std::uint32_t>(72095).value();});
    }else throw std::runtime_error("unknown PMD receiving case");
}
}
int main(int argc,char** argv){try{
    check(argc==4,"expected PMD case actual-retail-root packet-fixture-root");execute(argv[1],argv[2],argv[3]);
    std::cout<<"{\"passed\":true,\"case\":\""<<argv[1]<<"\",\"actual_pmd_bank\":true,\"actual_warm_transition\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
