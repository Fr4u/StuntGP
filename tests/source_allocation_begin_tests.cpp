#include "source_allocation_begin_witness.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/scene_resources.hpp"
#include <atomic>
#include <cstring>
#include <iostream>
#include <thread>
#include <type_traits>

namespace {
using namespace sgn;
using namespace sgn::retail;
using allocation_begin_receiving::BeginWitness;
using allocation_begin_receiving::words;
unsigned checks{},matrix_cases{},real_owner_cases{},begin_calls{},thread_cases{},guard_cases{};
void check(bool value,const char* reason){++checks;if(!value)throw std::runtime_error(reason);}
template<class F>void expired(F&& f){
    bool stopped=false;try{f();}catch(const SourceAllocationError&){stopped=true;}
    check(stopped,"expired source allocation remained usable");
}
void run_begin(RaceSessionPlatform& p){
    const BeginWitness witness(p);
    p.begin_scene_allocations_42ea80();++begin_calls;
    checks+=witness.verify();
}
void no_backend(const RaceSessionPlatform& p){
    const auto h=p.host_readiness();
    check(!h.assets&&!h.audio_device&&!h.audio_stream&&!h.window_input,"begin created a host backend");
    check(!p.session_attachment_claimed(),"begin changed single-session admission");
}
SourceAllocationRef allocate(SourceAllocationBank& bank,std::uint32_t size,std::uint32_t category,std::uint8_t fill){
    const auto ref=bank.allocate_42e8d0(size,category);check(bool(ref),"unexpected full source bank");
    auto data=bank.bytes(ref);std::fill(data.begin(),data.end(),fill);return ref;
}
void matrix(const std::filesystem::path& root){
    for(const auto ordinal:{0u,1u,382u,383u,384u,385u})for(const bool active:{false,true})for(const bool force:{false,true}){
        RaceSessionPlatform p(SourceColdConstructionOnly{},root,0.0f);auto& bank=p.source_allocations();
        bank.initialize_42eab0(); // explicit fixture boot, never adapter fallback
        if(force){bank.shutdown_42eaf0();bank.begin_42ea80();}
        for(unsigned i=0;i<ordinal;++i){const auto transient=bank.allocate_42e8d0(0,0);(void)bank.release_42e980(transient);}
        if(!active)bank.release_scope_42ea00(); // fixtures may allocate while source active==0
        constexpr std::array<std::uint32_t,5> categories{0,1,2,0x80000001u,0xffffffffu};
        constexpr std::array<std::uint32_t,5> sizes{0,1,31,32,33};
        std::array<SourceAllocationRef,5> refs;
        for(unsigned i=0;i<refs.size();++i)refs[i]=allocate(bank,sizes[i],categories[i],static_cast<std::uint8_t>(17+i));
        const auto peak=bank.source_words().peak_bytes;
        run_begin(p);
        for(unsigned i=0;i<refs.size();++i){
            const bool released=active&&ordinal+i<384u&&(categories[i]!=1||force);
            if(released)expired([&]{(void)bank.bytes(refs[i]);});
            else check(bank.bytes(refs[i]).size()==sizes[i],"retained source ref was invalidated");
        }
        check(bank.source_words().peak_bytes==peak&&bank.source_words().release_persistent==static_cast<unsigned>(force),
            "begin cleared persistent latch or lifetime peak");
        run_begin(p); // another explicit source invocation; never a frontier retry
        const auto replacement=allocate(bank,17,0,0x42);
        for(const auto& ref:refs)check(ref!=replacement,"begin reset host generation identity");
        run_begin(p);expired([&]{(void)bank.bytes(replacement);});
        no_backend(p);++matrix_cases;
    }
}
void duplicates_and_full(const std::filesystem::path& root){
    {
        RaceSessionPlatform p(SourceColdConstructionOnly{},root,0.0f);auto& b=p.source_allocations();b.initialize_42eab0();
        const auto persistent=allocate(b,11,1,0x61);run_begin(p);
        const auto ordinary=allocate(b,13,0,0x62),another=allocate(b,19,1,0x63);
        check(b.source_records()[0].ordinal==b.source_records()[1].ordinal,"fixture did not create duplicate source ordinals");
        run_begin(p);expired([&]{(void)b.bytes(ordinary);});
        check(b.bytes(persistent)[0]==0x61&&b.bytes(another)[0]==0x63,"duplicate ordinal retained category lost");
        b.shutdown_42eaf0();expired([&]{(void)b.bytes(persistent);});expired([&]{(void)b.bytes(another);});
        run_begin(p);check(b.source_words().release_persistent==1,"begin cleared shutdown latch");no_backend(p);
    }
    {
        RaceSessionPlatform p(SourceColdConstructionOnly{},root,0.0f);auto& b=p.source_allocations();b.initialize_42eab0();
        std::array<SourceAllocationRef,384> refs;
        for(unsigned i=0;i<refs.size();++i)refs[i]=allocate(b,1,1,static_cast<std::uint8_t>(i));
        run_begin(p);check(b.live_count()==384u,"full persistent bank was cleared");
        const auto before=words(b);check(!b.allocate_42e8d0(1,0)&&words(b)==before,"full bank allocation changed counters");
        for(unsigned i=0;i<refs.size();++i)check(b.bytes(refs[i])[0]==static_cast<std::uint8_t>(i),"full bank payload lost");
        b.shutdown_42eaf0();check(b.live_count()==0u,"source force release left in-window persistent rows");
        run_begin(p);const auto fresh=allocate(b,1,0,7);
        for(const auto& ref:refs)check(fresh!=ref,"full-bank generation was reused");no_backend(p);
    }
}
template<class T>void append(Bytes& bytes,const T& value){
    static_assert(std::is_trivially_copyable_v<T>);
    const auto* start=reinterpret_cast<const std::uint8_t*>(&value);bytes.insert(bytes.end(),start,start+sizeof(value));
}
Bytes retained(const FrontendState& f){
    const auto& s=f.source;const auto& w=f.world();Bytes out;
    append(out,s.menu_words_5da2e8);append(out,s.frontend_clip);append(out,s.menu_counter_5da370);
    append(out,s.menu_words_5df194);append(out,s.persistence.bytes);append(out,s.cold_suffix);
    append(out,s.read_scratch.bytes);append(out,s.csv_token_scratch);append(out,s.initials_scratch);append(out,s.numeric_scratch);
    append(out,s.page_pool);append(out,s.navigation_words);append(out,s.challenge_text);append(out,s.campaign_text);
    append(out,s.feedback_gate);append(out,s.language_scratch);append(out,s.language_arena);append(out,s.language_text_handles);
    append(out,s.language_names);append(out,s.language_descriptors);append(out,s.language_remaining);append(out,s.language_input_identity);
    append(out,s.language_empty_sentinel);append(out,s.scene.menu_initialized);append(out,s.race_return_selector);
    append(out,s.menu_state);append(out,s.initializing);append(out,s.initialization_count);
    append(out,s.lookup_a);append(out,s.lookup_b);append(out,s.lookup_c);append(out,s.frontend_descriptors);
    append(out,s.campaign_launch);append(out,s.campaign_result);
    append(out,w.frontend_launch);append(out,w.selection);append(out,w.cars);append(out,w.options);
    append(out,w.scene_mode);append(out,w.frame_pause);append(out,w.normal_finish.menu_requested);append(out,w.simulation_tick);
    append(out,w.tick_clock);append(out,w.source_scheduler.frequency);append(out,w.source_scheduler.fixed_mode_a);append(out,w.source_scheduler.fixed_mode_b);
    return out;
}
void owner_cases(const std::filesystem::path& root){
    for(const bool active:{false,true}){
        auto p=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},root,0.0f);
        auto alias=std::shared_ptr<RaceSessionState>(p,&p->state());FrontendState frontend(alias,p);
        auto& raw=frontend.source;auto& w=p->state();auto& b=p->source_allocations();
        b.initialize_42eab0();if(!active)b.release_scope_42ea00();
        const AssetStore assets(root);SceneResources scene(assets,root);auto& models=scene.models();
        const auto id=models.load_source(b,"car1");const auto handle=models.resolve(id);check(bool(handle),"real model identity missing");
        const auto view=models.source_at(*handle);const auto field=view.part(view.source_lod_index(0),0).flags();
        const auto source_flags=field.value();const auto model_ref=view.allocation();
        const auto loading=models.source_loading_state().snapshot();const auto model_count=models.size();
        const auto f0=w.ui.fonts.allocate_source_scratch(0,b,nullptr),f1=w.ui.fonts.allocate_source_scratch(1,b,nullptr);
        const auto r0=w.ui.fonts.source_scratch_allocation(0,b),r1=w.ui.fonts.source_scratch_allocation(1,b);
        const auto persistent=allocate(b,47,1,0x7b);
        raw.persistence.bytes.fill(0x41);raw.cold_suffix.fill(0x12345678u);raw.page_pool.fill(0x63);
        raw.frontend_descriptors.fill(0x87654321u);raw.language_names.fill(37);raw.language_descriptors.fill(41);
        raw.language_arena.fill(0x73);raw.scene.menu_initialized=7;raw.menu_state=11;raw.initialization_count=13;
        w.frontend_launch.catalog_count=91;w.frontend_launch.prepared=83;
        w.scene_mode=1;w.options.selection_phase=2;w.normal_finish.menu_requested=1;w.frame_pause=1;
        w.simulation_tick=117;w.tick_clock={113,127,131};w.source_scheduler.frequency=137;
        w.source_scheduler.fixed_mode_a=139;w.source_scheduler.fixed_mode_b=149;
        for(unsigned i=0;i<6;++i){w.cars[i].model=id;w.selection[i].model=31+i;}
        const auto before=retained(frontend);
        const auto verify_descriptors=[&]{
            check(retained(frontend)==before,"begin changed retained frontend/catalog/page pool/scene/clock storage");
            check(&frontend.world()==&p->state()&&frontend.shared_platform().get()==p.get(),"frontend lost platform alias");
            check(models.size()==model_count&&models.resolve(id)==handle&&models.source_backed(*handle),"begin cleared model catalog");
            const auto& current=models.source_loading_state();
            check(current.file==loading.file&&current.file_size==loading.file_size&&current.version==loading.version&&
                current.descriptor_file==loading.descriptor_file&&current.descriptor_offset==loading.descriptor_offset,
                "begin rewrote retained PMD loading descriptors");
            check(w.ui.fonts.scratch_allocations()==2&&w.ui.fonts.source_word(0,4)==f0&&w.ui.fonts.source_word(1,4)==f1,
                "begin reset font descriptors instead of only allocation ownership");
            check(b.bytes(persistent)[0]==0x7b,"real-owner persistent payload lost");
        };
        run_begin(*p);verify_descriptors();
        if(!active){check(field.value()==source_flags&&b.bytes(r0).size()==1024&&b.bytes(r1).size()==1024,
            "inactive source begin released real consumers");run_begin(*p);verify_descriptors();}
        expired([&]{(void)b.bytes(model_ref);});expired([&]{(void)models.source_at(*handle);});
        expired([&]{(void)view.part_count();});expired([&]{(void)field.value();});
        expired([&]{(void)b.bytes(r0);});expired([&]{(void)b.bytes(r1);});
        // Released source identities remain in their existing owners. No PMD
        // reload/track phase, catalog cleanup, or later scene child is invoked.
        no_backend(*p);++real_owner_cases;
    }
}
void admission(const std::filesystem::path& root){
    RaceSessionPlatform p(SourceColdConstructionOnly{},root,0.0f);auto& b=p.source_allocations();
    const auto before=words(b);bool boot=false;
    try{p.begin_scene_allocations_42ea80();}catch(const SourceAllocationError& e){boot=e.source_site==0x42eab0;}
    check(boot&&!b.initialized()&&words(b)==before&&b.live_count()==0,"adapter silently initialized unbooted bank");++guard_cases;
    b.initialize_42eab0();const auto ref=allocate(b,19,0,0x53);const auto seeded=words(b);
    std::atomic<bool> rejected{false};
    std::thread foreign([&]{try{p.begin_scene_allocations_42ea80();}catch(const std::logic_error&){rejected=true;}});foreign.join();
    check(rejected&&words(b)==seeded&&b.bytes(ref)[0]==0x53,"wrong-thread begin made partial source writes");++thread_cases;
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);auto& ob=other.source_allocations();ob.initialize_42eab0();
    const auto unrelated=allocate(ob,13,0,0x67);const auto other_before=words(ob);
    run_begin(p);check(words(ob)==other_before&&ob.bytes(unrelated)[0]==0x67,"begin touched another platform");
    no_backend(p);no_backend(other);
}
}
int main(int argc,char** argv)try{
    check(argc==2,"supply NEW retail root");const std::filesystem::path root(argv[1]);
    admission(root);matrix(root);duplicates_and_full(root);owner_cases(root);
    std::cout<<"{\"passed\":true,\"checks\":"<<checks<<",\"matrix_cases\":"<<matrix_cases
        <<",\"real_owner_cases\":"<<real_owner_cases<<",\"begin_calls\":"<<begin_calls
        <<",\"wrong_thread_cases\":"<<thread_cases<<",\"uninitialized_cases\":"<<guard_cases
        <<",\"next_unadmitted_child\":\"42E6E0\",\"actual_Warm_executed\":false,\"full_442030_qualified\":false"
        <<",\"audio_calls\":0,\"native_QPC_calls\":0,\"GPU_runs\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"allocation begin: "<<e.what()<<'\n';return 1;}
