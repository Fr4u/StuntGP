#include "sgn/model_resources.hpp"
#include "sgn/source_pmd_spline.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <memory>
#include <thread>

namespace {
using namespace sgn;using namespace sgn::retail;
void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
template<class F>void rejects(F&& action){bool caught=false;try{action();}catch(const std::exception&){caught=true;}check(caught,"expected checked PMD rejection");}
std::uint32_t word(std::span<const std::uint8_t> bytes,std::uint32_t at){
    check(at<=bytes.size()&&bytes.size()-at>=4,"receiving word bounds");std::uint32_t value;std::memcpy(&value,bytes.data()+at,4);return value;
}
void put(std::span<std::uint8_t> bytes,std::uint32_t at,std::uint32_t value){
    check(at<=bytes.size()&&bytes.size()-at>=4,"receiving write bounds");std::memcpy(bytes.data()+at,&value,4);
}
struct Files final:SourcePmdFileSource {
    SourcePmdFileSystem actual;const SourceAllocationBank& bank;
    std::vector<std::pair<std::uint32_t,std::uint32_t>> faults;
    unsigned sizes{},reads{};
    Files(const std::filesystem::path& root,const SourceAllocationBank& b):actual(root),bank(b){}
    std::uint32_t size(std::string_view name) override{++sizes;return actual.size(name);}
    SourcePmdFileRead read(std::string_view name,std::span<std::uint8_t> destination) override{
        ++reads;const auto rows=bank.source_records();const auto address=reinterpret_cast<std::uintptr_t>(destination.data());
        check(std::any_of(rows.begin(),rows.end(),[&](const auto& row){return row.raw&&row.aligned==address&&row.category==0&&row.requested_bytes==destination.size();}),"track read did not target actual bank allocation");
        const auto result=actual.read(name,destination);
        for(const auto& [at,value]:faults)put(destination,at,value); //faults alter destination, never imported PMD
        return result;
    }
};
struct Audit final:SourcePmdObserver {
    std::vector<SourcePmdEvent> events;std::uint32_t abort_site{};
    void observe(const SourcePmdEvent& event,const SourceAllocationBank& bank,const SourcePmdLoadingState&) override{
        events.push_back(event);check(bank.live_count()>0,"missing actual track allocation");
        if(event.site==abort_site)throw std::runtime_error("explicit receiving abort after reached track child");
    }
};
struct Fixture {
    AssetStore assets;SourceAllocationBank bank;Files files;ModelResources resources;Audit audit;
    explicit Fixture(const std::filesystem::path& root):assets(root),files(root,bank),resources(assets,root){bank.initialize_42eab0();}
    std::uint32_t load(std::string name="track1"){return resources.load_source(bank,files,name,&audit);}
    SourcePmdModelView model(std::uint32_t token){return resources.source_at(*resources.resolve(token));}
};
std::array<SourcePmdLocation,13> pointers(const SourcePmdTrackGlobals& g){
    return {g.spatial_vertex_indices,g.spatial_polygon_indices,g.spatial_bounds,g.spatial_nodes,g.splines,g.opaque58,g.lights,
        g.collision,g.texture_indices,g.texture_motion,g.normals,g.generated_normals,g.ambient};
}
void check_globals(const SourcePmdTrackGlobals& now,const SourcePmdTrackGlobals& old,SourceAllocationRef allocation,unsigned stage){
    const auto actual=pointers(now),prior=pointers(old);
    // Exact actual TRACK1 pointer projections, independently derived from PE stores.
    constexpr std::array<std::uint32_t,13> offsets{604192,564576,443232,664128,829280,829440,808416,806816,807840,806880,121696,194144,808352};
    constexpr std::array<unsigned,13> published{0,0,0,0,2,4,3,1,5,5,6,6,3};
    for(unsigned i=0;i<actual.size();++i)check(actual[i]==(published[i]<=stage?SourcePmdLocation{allocation,offsets[i]}:prior[i]),"track global pointer publication identity/order");
    check(now.spline_count==(stage>=2?5u:old.spline_count),"track spline scalar publication");
    check(now.light_count==(stage>=3?3u:old.light_count)&&now.render_light_count==(stage>=3?2u:old.render_light_count),"track light scalars publication");
    check(now.opaque58_count==(stage>=4?0u:old.opaque58_count),"opaque track scalar publication");
}
void execute(std::string_view name,const std::filesystem::path& root,const std::filesystem::path& fixture){
    if(name=="canonical"){
        Fixture f(root);const auto model=f.model(f.load());const auto track=model.track();const auto bytes=f.bank.bytes(model.allocation());
        const auto expected=read_file(fixture/"track1-source-offset-relocation.bin");
        check(bytes.size()==829440&&std::equal(bytes.begin(),bytes.end(),expected.begin(),expected.end()),"full TRACK1 canonical bytes differ from static PE relocation");
        check(f.files.sizes==1&&f.files.reads==1&&f.bank.live_count()==1&&f.bank.source_words().next_ordinal==1&&f.bank.source_words().ordinary_bytes==829476,"track branch added an allocation or read");
        const std::array<std::uint32_t,15> sites{0x42efe0,0x42efed,0x42f006,0x42f03d,0x42f042,0x42f047,0x42f04c,0x42f05d,0x42f062,0x42f067,0x42f06c,0x42f071,0x42f076,0x42f07b,0x42f080};
        check(f.audit.events.size()==sites.size(),"track producer events count");
        for(std::size_t i=0;i<sites.size();++i)check(f.audit.events[i].site==sites[i]&&f.audit.events[i].allocation==model.allocation(),"track caller sequence/identity");
        check_globals(f.resources.source_loading_state().track,{},model.allocation(),6);
        check(track.spatial_nodes().size()==12928&&track.spatial_bounds().size()==7584&&track.spatial_vertex_indices().size()==29953&&track.spatial_polygon_indices().size()==19802,"TRACK1 spatial byte shapes");
        check(track.collision_descriptor().size()==56&&track.collision_tree(0).size()==25680&&track.collision_tree(1).size()==13776,"TRACK1 collision roots");
        check(track.collision_tree_radius(0).value()==50&&track.collision_tree_radius(1).value()==100,"TRACK1 authored tree radii");
        check(track.spline_count()==5&&track.spline_point_storage().size()==20400&&track.spline_points(4).size()==255,"TRACK1 spline projections");
        check(track.light_count()==3&&track.render_light_count()==2&&track.light_records().size()==432&&track.ambient().size()==40,"TRACK1 light projections");
        check(track.texture_motion_bytes().size()==960&&track.texture_index_bytes().size()==512&&track.generated_normals().size()==82,"TRACK1 texture/normal regions");
        check(track.opaque58_count()==0&&track.opaque58_location().offset==829440,"actual empty opaque pointer must retain end-of-file offset");
        rejects([&]{(void)f.resources.at(*f.resources.resolve(1));}); //source-mode table cannot project a copied legacy model
    }else if(name=="projection_mutations"){
        Fixture f(root);const auto model=f.model(f.load());const auto track=model.track();
        std::array<SourcePmdByteView,7> records{track.spatial_nodes(),track.collision_polygons(),track.collision_materials(),track.collision_tree(0),track.light_records(),track.ambient(),track.texture_motion_bytes()};
        for(const auto& record:records){const auto field=record.field<std::uint32_t>(0);const auto changed=field.value()^0x801u;field.set(changed);
            check(word(f.bank.bytes(model.allocation()),field.location().offset)==changed,"track projected write went to a copy");}
        auto normal=track.generated_normals().at(0).value();normal.w=123;track.generated_normals().at(0).set(normal);
        check(model.normals().at(4528).value().w==123&&track.collision_normals().at(4528).value().w==123,"generated/source/collision normal aliases diverged");
        const auto d=track.collision_descriptor();const auto first=d.field<std::uint32_t>(8).value();
        const auto second=track.collision_vertices().at(1).value();d.field<std::uint32_t>(8).set(first+16);
        const auto now=track.collision_vertices().at(0).value();check(std::memcmp(&second,&now,16)==0,"collision accessor cached the relocated pointer");
        d.field<std::uint32_t>(8).set(first+1);rejects([&]{(void)track.collision_vertices();});
    }else if(name=="spline_mutations"){
        Fixture f(root);const auto model=f.model(f.load());SourcePmdSplineSet set(model);const auto spline=*set.find(1);
        const auto field=model.track().spline_points(0).at(0);auto point=field.value();point={111,222,333,444};field.set(point);
        const auto sampled=spline.sample(0).value;check(std::memcmp(&sampled,&point,16)==0,"recovered spline sampler did not read actual bank mutation");
        const auto distance=spline.distance_start(0);check(std::memcmp(&distance.position,&point,16)==0,"distance placement ignored canonical point");
        const auto grid=spline.grid({1,0,0,0,0,0},3);check(grid[0]&&grid[0]->position.x==111&&grid[0]->position.y==222&&grid[0]->position.z==333&&grid[0]->copied_position_w==444,"original grid consumer ignored canonical point");
    }else if(name=="spline_pointer"){
        Fixture f(root);const auto model=f.model(f.load());const auto track=model.track();SourcePmdSplineSet set(model);const auto spline=*set.find(1);
        const auto second=track.spline_points(1).at(0).value();const auto pointer=track.spline_descriptor(0).field<std::uint32_t>(4);
        pointer.set(track.spline_descriptor(1).field<std::uint32_t>(4).value());
        const auto actual=spline.sample(0).value;check(std::memcmp(&actual,&second,16)==0&&spline.descriptor().point_index==255,"existing spline adapter cached original pointer");
        pointer.set(pointer.value()+1);rejects([&]{(void)spline.sample(0);});
        pointer.set(829424);rejects([&]{(void)spline.point(0);});
    }else if(name=="descriptor_mutations"){
        Fixture f(root);const auto model=f.model(f.load());SourcePmdSplineSet set(model);const auto original=*set.find(1);const auto d=model.track().spline_descriptor(0);
        d.field<std::uint32_t>(0).set(3);check(!set.find(1)&&set.find(3)->location()==original.location(),"spline find did not use first current descriptor ID");
        d.field<std::uint32_t>(0).set(1);model.track().spline_descriptor(2).field<std::uint32_t>(0).set(30);
        check(set.ai_routes()[0].location()==original.location(),"actual missing-ID3 route failed to fall back to first ID1");
        const auto length=d.field<float>(24).value();d.field<float>(24).set(length*0.5f);
        const auto sample=original.indexed(length*0.125f);check(sample.selected_index==63,"spline adapter cached authored length");
        d.field<std::uint32_t>(8).set(0xffffffffu);rejects([&]{(void)original.sample(0);});
    }else if(name=="adjacent"){
        Fixture f(root);const auto model=f.model(f.load());SourcePmdSplineSet set(model);
        const auto a=*set.find(1),b=*set.find(2),last=*set.find(5);
        const auto adjacent=a.point(255),first=b.point(0);check(adjacent.adjacent_storage_read&&std::memcmp(&adjacent.value,&first.value,16)==0,"original adjacent authored read was wrapped/replaced");
        rejects([&]{(void)last.point(255);}); //padding/descriptors beyond authored vectors are not a sentinel
    }else if(name=="repeat_and_free"){
        Fixture f(root);const auto a=f.model(f.load()),b=f.model(f.load());SourcePmdSplineSet set(a);const auto spline=*set.find(1);const auto checked=a.track().light_records();
        check(!(a.allocation()==b.allocation())&&f.files.reads==2&&f.bank.live_count()==2,"same-name track request was deduplicated");
        const auto before=b.track().spline_points(0).at(0).value();a.track().spline_points(0).at(0).set({1,2,3,4});
        const auto after=b.track().spline_points(0).at(0).value();check(std::memcmp(&before,&after,16)==0,"separate track loads share payload");
        const auto old=a.allocation();(void)f.bank.release_42e980(old);
        rejects([&]{(void)checked.size();});rejects([&]{(void)spline.sample(0);});rejects([&]{(void)set.find(1);});
        const auto c=f.model(f.load());check(!(c.allocation()==old)&&b.track().spline_count()==5,"stale track identity reused or unrelated model invalidated");
        rejects([&]{(void)spline.point(0);});
    }else if(name=="owner_lifetime"){
        AssetStore assets(root);SourceAllocationBank bank;bank.initialize_42eab0();auto resources=std::make_unique<ModelResources>(assets,root);
        const auto token=resources->load_source(bank,"track1");const auto model=resources->source_at(*resources->resolve(token));const auto ref=model.allocation();
        SourcePmdSplineSet splines(model);const auto spline=*splines.find(1);resources.reset();
        check(bank.bytes(ref).size()==829440&&bank.live_count()==1,"model metadata destruction illegally source-freed track");
        rejects([&]{(void)spline.descriptor();});rejects([&]{(void)splines.find(1);});(void)bank.release_42e980(ref);
    }else if(name=="scope"){
        Fixture f(root);const auto model=f.model(f.load());SourcePmdSplineSet set(model);const auto spline=*set.find(1);
        const auto retained=f.resources.source_loading_state().track;f.bank.begin_42ea80();
        check(f.bank.live_count()==0&&f.resources.source_loading_state().track==retained,"source scope must expire track payload, retaining source globals");
        rejects([&]{(void)spline.sample(0);});check(f.model(f.load()).track().spline_count()==5,"track reload after explicit source scope");
    }else if(name=="retained_globals"){
        Fixture f(root);const auto model=f.model(f.load());const auto retained=f.resources.source_loading_state().track;
        const auto car=f.model(f.load("car1"));check(car.kind()==2&&f.resources.source_loading_state().track==retained,"non-track load changed track globals");
        (void)f.bank.release_42e980(model.allocation());check(f.resources.source_loading_state().track==retained,"free cleared retained source pointers");
        rejects([&]{(void)f.bank.bytes(retained.splines.allocation);});rejects([&]{(void)car.track();});
    }else if(name=="stage_prefixes"){
        constexpr std::array<std::uint32_t,7> sites{0x42f05d,0x42f062,0x42f067,0x42f06c,0x42f071,0x42f076,0x42f07b};
        constexpr std::array<const char*,7> labels{"spatial","collision","splines","lighting","opaque58","textures","normals"};
        for(unsigned stage=0;stage<sites.size();++stage){Fixture f(root);
            f.files.faults={{0x10c,112},{0xfc,1},{0x100,1},{0xe8,77}};(void)f.load();
            const auto old=f.resources.source_loading_state().track;f.files.faults.clear();f.audit.abort_site=sites[stage];
            rejects([&]{(void)f.load();});const auto state=f.resources.source_loading_state().snapshot();
            check(f.bank.live_count()==2&&f.resources.size()==1&&f.files.reads==2&&f.audit.events.back().site==sites[stage],"aborted track child lost reached allocation/model prefix");
            const auto expected=read_file(fixture/(std::string("track1-prefix-")+labels[stage]+".bin"));const auto actual=f.bank.bytes(state.file);
            check(std::equal(actual.begin(),actual.end(),expected.begin(),expected.end()),"aborted child canonical prefix differs from PE stores");
            check_globals(state.track,old,state.file,stage);
        }
    }else if(name=="spline_floor_zero"){
        for(const auto length:std::array<std::uint32_t,3>{0,27,139}){Fixture f(root);f.files.faults={{0x10c,length}};
            const auto model=f.model(f.load());const auto track=model.track();check(track.spline_count()==length/28&&f.resources.source_loading_state().track.spline_count==length/28,"42EE50 floor division differs");
            SourcePmdSplineSet set(model);if(length<28){check(!set.find(1),"zero spline count invented a fallback");rejects([&]{(void)set.ai_routes();});}
            else check(set.size()==4&&set.ai_routes()[2].descriptor().id==1,"four-route file did not retain missing5 fallback");
            const auto raw=read_file(root/"meshdata"/"track1.PMD");const auto base=328+word(raw,0x78);
            for(auto i=length/28;i<5;++i)check(model.bytes().field<std::uint32_t>(base+i*28+4).value()==word(raw,base+i*28+4),"floor count relocated an unvisited descriptor");
        }
    }else if(name=="bad_collision"){
        Fixture f(root);f.files.faults={{806816+20,0xffffffffu}};rejects([&]{(void)f.load();});const auto state=f.resources.source_loading_state().snapshot();const auto bytes=f.bank.bytes(state.file);
        check(f.resources.size()==0&&state.track.collision==SourcePmdLocation{state.file,806816}&&f.audit.events.back().site==0x42f05d,"bad collision lost descriptor global or advanced caller");
        check(word(bytes,806816)==725952&&word(bytes,806820)==677056&&word(bytes,806824)==36096&&word(bytes,806828)==121696&&word(bytes,806832)==701216,"bad sixth relocation erased preceding five stores");
        check(word(bytes,806836)==0xffffffffu&&word(bytes,806844)==0&&word(bytes,806852)==248&&!state.track.splines.allocation,"bad collision fabricated later stores");
    }else if(name=="bad_spline"){
        Fixture f(root);f.files.faults={{829284+2*28,0xffffffffu}};rejects([&]{(void)f.load();});const auto state=f.resources.source_loading_state().snapshot();const auto bytes=f.bank.bytes(state.file);
        check(f.audit.events.back().site==0x42f062&&!state.track.splines.allocation&&state.track.spline_count==0,"failed spline relocation prematurely published globals");
        check(word(bytes,829284)==808864&&word(bytes,829312)==812944&&word(bytes,829340)==0xffffffffu&&word(bytes,829368)==765,"failed spline relocation lost or invented partial stores");
    }else if(name=="normal_bounds"){
        Fixture f(root);f.files.faults={{0xe4,4611}};rejects([&]{(void)f.load();});const auto state=f.resources.source_loading_state().snapshot();
        check(f.audit.events.back().site==0x42f076&&!state.track.normals.allocation&&!state.track.generated_normals.allocation&&bool(state.track.texture_motion.allocation),"normal boundary lost prefix or published unsupported range");
        Fixture g(root);const auto model=g.model(g.load());model.bytes().field<std::uint32_t>(0xe4).set(4611);rejects([&]{(void)model.track().generated_normals();});
    }else if(name=="opaque_count"){
        Fixture f(root);f.files.faults={{0xe8,7}};const auto model=f.model(f.load());
        check(f.resources.source_loading_state().track.opaque58_count==7&&model.track().opaque58_count()==7&&model.track().opaque58_location().offset==829440,"opaque count was replaced with fabricated zero/data");
    }else if(name=="foreign_thread"){
        Fixture f(root);const auto model=f.model(f.load());SourcePmdSplineSet splines(model);const auto spline=*splines.find(1);
        bool sample_rejected=false,write_rejected=false;const auto field=model.track().spline_points(0).at(0);const auto before=field.value();
        std::thread worker([&]{try{(void)spline.sample(0);}catch(const SourceAllocationError&){sample_rejected=true;}
            try{field.set({1,2,3,4});}catch(const SourceAllocationError&){write_rejected=true;}});worker.join();
        const auto after=field.value();check(sample_rejected&&write_rejected&&std::memcmp(&before,&after,16)==0,"foreign thread accessed actual PMD payload");
    }else throw std::runtime_error("unknown track PMD receiving case");
}
}
int main(int argc,char** argv){try{
    check(argc==4,"expected track-case actual-retail-root track-fixtures");execute(argv[1],argv[2],argv[3]);
    std::cout<<"{\"passed\":true,\"case\":\""<<argv[1]<<"\",\"actual_track_bank\":true,\"actual_Session_activation\":false}\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
