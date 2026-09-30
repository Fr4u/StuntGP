#include "sgn/race_session.hpp"
#include "sgn/session_world_materials.hpp"
#include "sgn/retail_world_sprite_model.hpp"
#include "source_allocation_begin_witness.hpp"
#include "source_resource_registry_expectations.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{},prefix_calls{},publication_stores{},clear_stores{},interrupted_cases{};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);++checks;}
template<class F>void rejects(F&& call,const char* message){
    bool rejected=false;try{call();}catch(const std::exception&){rejected=true;}check(rejected,message);
}
template<class T>std::vector<std::uint8_t> bytes_of(const T& value){
    const auto* first=reinterpret_cast<const std::uint8_t*>(&value);return {first,first+sizeof(T)};
}
struct BankUnchanged {
    const SourceAllocationBank& bank;
    allocation_begin_receiving::Words words;
    std::array<SourceAllocationRecord,384> rows;
    std::array<std::vector<std::uint8_t>,384> payloads;
    explicit BankUnchanged(const SourceAllocationBank& b):bank(b),words(allocation_begin_receiving::words(b)){
        std::ranges::copy(b.source_records(),rows.begin());
        for(unsigned i=0;i<384;++i)if(rows[i].raw){
            const auto* first=reinterpret_cast<const std::uint8_t*>(rows[i].aligned);
            payloads[i].assign(first,first+rows[i].requested_bytes);
        }
    }
    void verify() const{
        check(words==allocation_begin_receiving::words(bank),"registry reset changed allocation control words");
        for(unsigned i=0;i<384;++i){
            check(allocation_begin_receiving::same(rows[i],bank.source_records()[i]),"registry reset changed an allocation row");
            if(rows[i].raw)check(std::equal(payloads[i].begin(),payloads[i].end(),reinterpret_cast<const std::uint8_t*>(rows[i].aligned)),"registry reset changed live allocation bytes");
        }
    }
};
struct RejectBackend final:MaterialTextureBackend {
    unsigned calls{};
    [[noreturn]] void hit(){++calls;throw std::runtime_error("backend must not be reached");}
    void release_all() override{hit();}
    std::uint8_t& current_transparency() override{hit();}
    void begin_pending_record(std::uint32_t) override{hit();}
    CompletedMaterialUpload upload(PreparedMaterialPixels) override{hit();}
    CompletedMaterialUpload generate(std::uint32_t) override{hit();}
};
struct Fixture {
    std::shared_ptr<RaceSessionPlatform> platform;
    std::unique_ptr<SessionWorldMaterials> materials;
    std::array<SelectionRecord,6> selection{};
    std::array<LevelCarSlot,6> cars{};
    SourceAllocationRef persistent;
    explicit Fixture(const std::filesystem::path& root):platform(std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f)){
        platform->prepare_host_assets();platform->prepare_source_scene_resources();
        auto* identity=&platform->source_scene_resources();platform->prepare_source_scene_resources();
        check(identity==&platform->source_scene_resources(),"source registry owner replaced");
        materials=std::make_unique<SessionWorldMaterials>(SourceColdMaterialAttachmentOnly{},platform);
        auto flags=materials->registry_flags();for(unsigned i=0;i<16;++i)flags[i]=0x91370000u+i*17u;
        auto& bank=platform->source_allocations();bank.initialize_42eab0();
        persistent=bank.allocate_42e8d0(31,1);std::ranges::fill(bank.bytes(persistent),std::uint8_t{0x5b});
        // Actual retained polygon owner, CPU only. This is test setup, not
        // work attributed to42E510 or a Session readiness transition.
        allocate_world_polygon_frame(platform->state().polygon_frame);
        auto& allocation=*platform->state().polygon_frame.allocations;
        allocation.indexed[0]=&allocation.vertices[3];
    }
    SceneResources& scene(){return platform->source_scene_resources();}
    void fill(){
        platform->begin_scene_allocations_42ea80();
        auto& world=platform->state();
        world.effect_resources.colors.initialize_allocation_434090(world.model_draws,scene());
        VehicleStripHeaders headers;initialize_vehicle_strip(world.strip,headers,scene());
        world.sprite_packets.positions[3].x=13.25f;world.strip.vertices[19].y=-31.5f;
        // Actual input/matrix/scene sentinels outside the bounded prefix.
        world.scene_mode=0x78563412;world.view_matrices.count=73;world.view_matrices.cursor=91;
        world.frame_pause=17;world.pause.physical_input.token=39;
        world.startup.fields_62dcf0.fill(0x19283746);world.startup.fields_62d480.fill(0xabcdef12);
        std::array<LevelAssetRequest,14> requests;
        for(unsigned i=0;i<14;++i)requests[i]={"car1",i+2};
        selection[1].kind=selection[4].kind=2;cars[0].source_word00=777;
        struct Publication final:SourceResourceRegistryObserver {
            Fixture& f;std::array<SceneResourceRecord,16> before;unsigned seen{};
            explicit Publication(Fixture& owner):f(owner){std::ranges::copy(f.scene().source_records(),before.begin());}
            void published(SourceResourceStore store,const SceneResources& scene) override{
                const auto slot=2+seen/3,phase=seen%3;
                const std::array<std::uint32_t,3> pcs{0x42f0f8,0x42f104,0x42f10a},offsets{0,8,12};
                check(store.slot==slot&&store.offset==offsets[phase]&&store.instruction==pcs[phase]&&store.address==0x636418+slot*16+offsets[phase],"42F090 publication order/address mismatch");
                const auto rows=scene.source_records();const auto& loading=f.scene().models().source_loading_state();
                const auto* binding=std::get_if<ScenePmdBinding>(&rows[slot].descriptor);
                check(binding&&binding->handle+1==f.scene().models().size(),"descriptor was not published after actual loader");
                check(rows[slot].file==(phase>=1?loading.file:before[slot].file),"file publication crossed its store");
                check(rows[slot].file_size==(phase>=2?loading.file_size:before[slot].file_size),"size publication crossed its store");
                for(unsigned i=slot+1;i<16;++i)check(rows[i]==before[i],"future request was published early");
                check(f.materials->registry_flags_for(scene).data()==f.materials->registry_flags().data(),"flags are copied");
                ++seen;++publication_stores;
            }
        } observer(*this);
        scene().bind_source_models(platform->source_allocations(),requests,selection,cars,nullptr,&observer);
        check(observer.seen==42,"missing PMD publications");
        check(cars[0].source_word00==777&&cars[1].source_word00==scene().models().identity(scene().model_handle(4))&&
            cars[4].source_word04==scene().models().identity(scene().model_handle(7)),"physical selection holes or pair mapping lost");
        for(unsigned slot=2;slot<16;slot+=2){
            const auto handle=*scene().model_handle(slot);const auto ref=scene().source_records()[slot].file;
            (void)platform->source_allocations().release_42e980(ref);
            rejects([&]{(void)scene().models().source_at(handle);},"expired PMD became readable before reset");
            check(scene().source_records()[slot].file==ref&&scene().source_records()[slot].file_size>0,"release silently cleared registry");
        }
    }
};
struct Stop final:std::exception {};
struct PrefixObserver final:SourceResourceRegistryObserver {
    Fixture& f;int stop;unsigned seen{};
    std::array<SceneResourceRecord,16> before;
    std::array<std::uint32_t,16> flags;
    const std::uint32_t* flags_identity;
    BankUnchanged bank;
    std::vector<std::uint8_t> sprites,strip,draws;
    std::size_t models;
    PrefixObserver(Fixture& owner,int interruption):f(owner),stop(interruption),flags_identity(f.materials->registry_flags().data()),
        bank(f.platform->source_allocations()),sprites(bytes_of(f.platform->state().sprite_packets)),
        strip(bytes_of(f.platform->state().strip)),draws(bytes_of(f.platform->state().model_draws)),models(f.scene().models().size()){
        std::ranges::copy(f.scene().source_records(),before.begin());std::ranges::copy(f.materials->registry_flags(),flags.begin());
    }
    void verify(unsigned stores) const{
        const auto rows=f.scene().source_records();
        for(unsigned slot=0;slot<16;++slot){
            // Closed-form completed-prefix membership, independent of the
            // production loop. The49th store repeats slot1 without new scope.
            const auto& old=before[slot];
            check(3*slot<stores?std::holds_alternative<std::monostate>(rows[slot].descriptor):rows[slot].descriptor==old.descriptor,"descriptor differs from exact interrupted prefix");
            check(rows[slot].file==(3*slot+1<stores?SourceAllocationRef{}:old.file),"file differs from exact interrupted prefix");
            check(rows[slot].file_size==(3*slot+2<stores?0:old.file_size),"size differs from exact interrupted prefix");
        }
        check(std::ranges::equal(flags,f.materials->registry_flags()),"prefix modified retained flags");
        check(sprites==bytes_of(f.platform->state().sprite_packets)&&strip==bytes_of(f.platform->state().strip)&&
            draws==bytes_of(f.platform->state().model_draws),"binding reset modified geometry/working draw storage");
        check(f.scene().models().size()==models,"prefix discarded PMD metadata");
        const auto& w=f.platform->state();
        check(w.scene_mode==0x78563412&&w.view_matrices.count==73&&w.view_matrices.cursor==91u&&w.frame_pause==17&&w.pause.physical_input.token==39,
            "prefix executed later scene/matrix/input reset");
        check(std::ranges::all_of(w.startup.fields_62dcf0,[](auto v){return v==0x19283746;})&&
            std::ranges::all_of(w.startup.fields_62d480,[](auto v){return v==0xabcdef12;}),"prefix cleared later keyboard arrays");
        check(w.polygon_frame.allocations&&w.polygon_frame.allocations->indexed[0]==&w.polygon_frame.allocations->vertices[3],
            "prefix crossed next required42E53C into polygon owner");
    }
    void cleared(SourceResourceStore store,const SceneResources& scene,std::span<const std::uint32_t,16> observed_flags) override{
        check(&scene==&f.scene()&&seen<expected_registry_stores.size(),"observer got foreign registry or extra store");
        const auto expected=expected_registry_stores[seen];
        check(store.instruction==expected.instruction&&store.address==expected.address&&store.slot==expected.slot&&store.offset==expected.offset,
            "source-proof store order/address mismatch");
        check(observed_flags.data()==flags_identity&&std::ranges::equal(flags,observed_flags),"observer flags are a copy or modified");
        ++seen;++clear_stores;verify(seen);bank.verify();
        if(static_cast<int>(seen-1)==stop)throw Stop{};
    }
};
void prefix_cases(const std::filesystem::path& root){
    Fixture f(root);
    for(int stop=-1;stop<49;++stop){
        f.fill();PrefixObserver observer(f,stop);bool stopped=false;
        const auto alive_handle=*f.scene().model_handle(3),expired_handle=*f.scene().model_handle(2);
        ++prefix_calls;
        try{
            const auto result=reset_source_scene_resource_prefix_42e510(f.scene(),*f.materials,&observer);
            check(stop==-1&&result.next_required_instruction==0x42e53c,"prefix claimed full42E510 or swallowed interrupted store");
        }catch(const Stop&){stopped=true;++interrupted_cases;}
        check(stopped==(stop>=0)&&observer.seen==(stop<0?49u:static_cast<unsigned>(stop+1)),"incorrect interruption boundary");
        observer.verify(observer.seen);observer.bank.verify();
        check(f.scene().models().source_at(alive_handle).part_count()>0,"clear freed a live PMD");
        rejects([&]{(void)f.scene().models().source_at(expired_handle);},"clear revived expired PMD");
        rejects([&]{f.scene().require_sprite_binding(f.platform->state().model_draws,f.platform->state().sprite_packets);},"null sprite registry resolved retained geometry");
        const auto old_draws=bytes_of(f.platform->state().model_draws);
        rejects([&]{auto& w=f.platform->state();seed_world_model_draws(w.model_draws,f.scene().models(),std::nullopt,std::nullopt,w.sprite_packets,0,1,w.view_matrices,&f.scene());},"real seed consumer ignored cleared sprite binding");
        check(old_draws==bytes_of(f.platform->state().model_draws),"failed sprite borrow changed queues");
        if(observer.seen>=4)rejects([&]{(void)f.scene().strip_geometry();},"null strip registry resolved retained geometry");
        const auto host=f.platform->host_readiness();
        check(host.assets&&!host.audio_device&&!host.audio_stream&&!host.window_input&&!f.platform->session_attachment_claimed(),"prefix admitted native host or new Session");
    }
}
void failures_and_guards(const std::filesystem::path& root){
    Fixture f(root);f.fill();auto& scene=f.scene();
    // Actual second file failure, after first completed42F090 publication.
    const auto before_count=scene.models().size();const auto old4=scene.source_records()[4];const auto cars=bytes_of(f.cars);
    const std::array<LevelAssetRequest,2> requests{{{"car1",3},{"missing-source-registry-proof-file",4}}};
    rejects([&]{scene.bind_source_models(f.platform->source_allocations(),requests,f.selection,f.cars);},"missing PMD unexpectedly loaded");
    check(scene.models().size()==before_count+1&&scene.model_handle(3)==before_count&&scene.source_records()[3].file&&scene.source_records()[3].file_size>0,
        "later file failure rolled back earlier successful publication");
    check(scene.source_records()[4]==old4&&cars==bytes_of(f.cars),"failed request changed uncompleted slot or reached car assignment");
    Fixture other(root);other.fill();const auto before=std::vector(scene.source_records().begin(),scene.source_records().end());
    rejects([&]{reset_source_scene_resource_prefix_42e510(scene,*other.materials);},"foreign material flags were admitted");
    check(std::ranges::equal(before,scene.source_records()),"foreign preflight performed stores");
    rejects([&]{SessionWorldMaterials duplicate(SourceColdMaterialAttachmentOnly{},f.platform);},"second material owner admitted");
    std::atomic<unsigned> rejected{};
    std::thread foreign([&]{
        try{reset_source_scene_resource_prefix_42e510(scene,*f.materials);}catch(const std::exception&){++rejected;}
        try{(void)f.materials->registry_flags();}catch(const std::exception&){++rejected;}
        try{SessionWorldMaterials duplicate(SourceColdMaterialAttachmentOnly{},f.platform);}catch(const std::exception&){++rejected;}
        try{f.platform->prepare_source_scene_resources();}catch(const std::exception&){++rejected;}
    });foreign.join();check(rejected==4&&std::ranges::equal(before,scene.source_records()),"foreign thread changed registry/owner");
    RejectBackend backend;WorldMaterialLoadingContext context;
    rejects([&]{f.materials->load(context,backend);},"source attachment admitted load");
    rejects([&]{(void)f.materials->state();},"source attachment admitted loaded state");
    rejects([&]{f.materials->publish_count_42e772();},"source attachment admitted material publication");
    check(backend.calls==0,"source attachment called a texture backend");
    check(std::ranges::equal(before,scene.source_records()),"rejected loading changed registry");
    f.materials.reset();rejects([&]{SessionWorldMaterials duplicate(SourceColdMaterialAttachmentOnly{},f.platform);},"destruction reset source material reservation");
    auto failed=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    rejects([&]{failed->prepare_source_scene_resources();},"scene preparation implicitly read assets");
    check(!failed->host_readiness().assets,"missing asset precondition performed archive I/O");
    rejects([&]{SessionWorldMaterials missing(SourceColdMaterialAttachmentOnly{},failed);},"unprepared scene accepted");
    failed->prepare_host_assets();failed->prepare_source_scene_resources();
    rejects([&]{SessionWorldMaterials retry(SourceColdMaterialAttachmentOnly{},failed);},"failed constructor reset owner reservation");
    rejects([&]{SessionWorldMaterials absent(SourceColdMaterialAttachmentOnly{},{});},"null platform accepted");
    auto threaded=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
    threaded->prepare_host_assets();threaded->prepare_source_scene_resources();
    std::thread wrong([&]{try{SessionWorldMaterials m(SourceColdMaterialAttachmentOnly{},threaded);}catch(const std::exception&){++rejected;}});wrong.join();
    SessionWorldMaterials correct(SourceColdMaterialAttachmentOnly{},threaded);check(rejected==5,"foreign constructor touched reservation");
    threaded.reset();rejects([&]{(void)correct.registry_flags();},"expired platform still exposes flags");
    rejects([&]{reset_source_scene_resource_prefix_42e510(scene,correct);},"expired material platform admitted prefix");
    check(std::ranges::equal(before,scene.source_records()),"expired owner preflight performed stores");
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected pinned NEW retail data root");prefix_cases(argv[1]);failures_and_guards(argv[1]);
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"prefix_calls\":"<<prefix_calls<<",\"publication_stores\":"<<publication_stores
        <<",\"clear_stores\":"<<clear_stores<<",\"interrupted_cases\":"<<interrupted_cases
        <<",\"next_required_instruction\":\"42E53C\",\"actual_Warm_executed\":false,\"full_42E510_qualified\":false,\"native_runs\":0,\"GPU_runs\":0,\"audio_calls\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
