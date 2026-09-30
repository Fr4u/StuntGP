#include "source_pmd_track_phase_fixture.hpp"
using namespace sgn;using namespace sgn::track_phase_test;
namespace {
void source(Fixture& f,Bank& b,Events* events=nullptr){advance_track_part_animations(b,f.models,f.handle,events?&events->audit:nullptr);if(events)events->checked();}
void compare_steps(Fixture& f,Bank& actual,unsigned count,unsigned steps=1){auto expected=actual;const auto ref=f.view().allocation();const auto pmd=snapshot(f.bank,ref);
    for(unsigned i=0;i<steps;++i){advance_track_part_animations(expected,count);source(f,actual);}check(same(actual,expected),"canonical/legacy full bank mismatch");check(pmd==snapshot(f.bank,ref),"phase wrote PMD bytes");}
void run(std::string_view name,const std::filesystem::path& root){
    if(name=="null"){Fixture f(root);auto b=state(),before=b;const auto ref=f.view().allocation();f.bank.release_42e980(ref);Events events;
        advance_track_part_animations(b,f.models,std::nullopt,&events.audit);advance_track_part_animations({},f.models,std::nullopt,&events.audit);
        std::exception_ptr error;std::thread t([&]{try{advance_track_part_animations(b,f.models,std::nullopt,&events.audit);}catch(...){error=std::current_exception();}});t.join();if(error)std::rethrow_exception(error);
        check(events.values.empty()&&same(b,before),"null touched owner or records");return;}
    if(name=="retail30"){const auto data=corpus(root.parent_path().parent_path()/"reports/level-work/track-part-animation/probe/track-part-animation.dat");
        SourceAllocationBank bank;bank.initialize_42eab0();AssetStore assets(root);ModelResources canonical(assets,root),legacy(assets,root);unsigned cases=0;std::uint64_t bytes=0;
        for(const auto& r:data){if(r.op!=0)continue;const auto model=r.name.substr(0,r.name.size()-4);const auto source_handle=*canonical.resolve(canonical.load_source(bank,model));
            //Receiving-only decoded importer binding for mirrored files absent from lvscript.
            //Use the script-bound base track namespace. This does not implement a mirror/texture producer.
            std::string texture_set;if(model.ends_with('m')){const auto base=assets.model(model.substr(0,model.size()-1));
                for(const auto& texture:base.textures)if(texture.starts_with("trackset")){const auto found=texture.substr(0,10);check(texture_set.empty()||texture_set==found,"ambiguous mirrored receiving texture namespace");texture_set=found;}
                check(!texture_set.empty(),"missing base-track receiving texture namespace");}
            const auto legacy_handle=*legacy.resolve(legacy.load(model,texture_set));
            const auto m=canonical.source_at(source_handle);check(m.part_count()==r.count,"retail track count projection");const auto before=snapshot(bank,m.allocation());auto a=state(),b=a;
            for(unsigned i=0;i<8;++i){advance_track_part_animations(a,canonical,source_handle);advance_track_part_animations(b,legacy,legacy_handle);check(same(a,b),"retail source and decoded owner differ");}
            check(before==snapshot(bank,m.allocation()),"retail phase wrote PMD");bytes+=before.size();++cases;}
        check(cases==30,"retail track coverage");std::cout<<"retail_tracks="<<cases<<" calls=240 compared_words=138240 unchanged_pmd_bytes="<<bytes<<'\n';return;}
    Fixture f(root);auto b=state();auto m=f.view();
    if(name=="count_gates"){for(unsigned count:{0u,1u}){f.count(count);m.descriptor().field<unsigned>(0x48).set(0xffffffffu);m.descriptor().field<unsigned>(0x58).set(0xffffffffu);m.current_detail_level().set(0xffffffffu);
        const auto before=b;Events e;source(f,b,&e);advance_track_part_animations({},m);check(same(b,before)&&e.values==std::vector<Event>{{0x427150,0,count}},"count gate touched head/state");}return;}
    if(name=="unused_head"){f.count(32);m.descriptor().field<unsigned>(0x48).set(0xffffffffu);m.descriptor().field<unsigned>(0x58).set(0xffffffffu);m.current_detail_level().set(0xffffffffu);
        compare_steps(f,b,32,64);return;}
    if(name=="captured_count"){f.count(5);auto expected=b;advance_track_part_animations(expected,5);Events e;e.action=[&](Event v){if(v.at==0x427150)m.descriptor().field<unsigned>(0x24).set(0);if(v.at==0x42715c)m.descriptor().field<unsigned>(0x24).set(0xffffffffu);};
        source(f,b,&e);check(same(b,expected)&&e.values.size()==10&&e.values.front().value==5&&e.values.back().index==4,"count was not captured once");return;}
    if(name=="captured_descriptor"){f.count(3);const auto head=m.descriptor().field<unsigned>(0x48).value();auto expected=b;advance_track_part_animations(expected,3);Events e;
        e.action=[&](Event v){if(v.at==0x427150)m.bytes().field<unsigned>(0x4c).set(0xffffffffu);};source(f,b,&e);
        check(same(b,expected)&&e.values.at(1)==Event{0x42715c,0,head},"raw head not read from captured descriptor");return;}
    if(name=="release_after_count"){for(unsigned count:{0u,1u,3u}){Fixture x(root);x.count(count);auto a=state(),before=a;auto direct=x.view();const auto ref=direct.allocation();Events e;e.action=[&](Event v){if(v.at==0x427150)x.bank.release_42e980(ref);};
        if(count<=1)advance_track_part_animations(a,direct,&e.audit);else rejects<SourceAllocationError>([&]{advance_track_part_animations(a,direct,&e.audit);});
        e.checked();check(same(a,before)&&e.values==std::vector<Event>{{0x427150,0,count}},"release after count prefix");}return;}
    if(name=="release_after_head"){f.count(32);auto expected=b;advance_track_part_animations(expected,32);const auto ref=m.allocation();Events e;e.action=[&](Event v){if(v.at==0x42715c)f.bank.release_42e980(ref);};
        source(f,b,&e);check(same(b,expected)&&e.values.size()==64,"PMD was borrowed after unused head");return;}
    if(name=="partial_prefix"){for(unsigned count:{2u,4u,32u,0xffffffffu})for(unsigned length:{0u,1u,2u,3u,31u}){f.count(count);auto a=state(),expected=a;const unsigned completed=std::min(count,length);
        if(completed>1)advance_track_part_animations(expected,completed);Events e;
        if(count>length)rejects<FormatError>([&]{advance_track_part_animations(std::span(a).first(length),m,&e.audit);});else advance_track_part_animations(std::span(a).first(length),m,&e.audit);
        e.checked();check(same(a,expected),"current record failure lost prefix or wrote suffix");const auto n=completed>1?completed-1:0;check(e.values.size()==2+n*2,"partial record event extent");
        auto legacy=state(),original=legacy;if(count>length){rejects<FormatError>([&]{advance_track_part_animations(std::span(legacy).first(length),count);});check(same(legacy,original),"legacy whole-span preflight changed");}}
        return;}
    if(name=="captured_kind"){f.count(3);b[1].kind=1;b[2].kind=2;auto expected=b;advance_track_part_animations(expected,3);expected[1].kind=16;Events e;
        e.action=[&](Event v){if(v.at==0x427168&&v.index==1)b[1].kind=16;};source(f,b,&e);check(same(b,expected),"kind dispatch was reread after load");return;}
    if(name=="fresh_next_record"){f.count(3);b[1].kind=1;b[2].kind=0;auto expected=b;expected[2].kind=16;expected[2].angles.x=0.5f;advance_track_part_animations(expected,3);Events e;
        e.action=[&](Event v){if(v.at==0x4271eb&&v.index==1){b[2].kind=16;b[2].angles.x=0.5f;}};source(f,b,&e);check(same(b,expected),"future record was snapshotted early");return;}
    if(name=="lifetime"){const auto before=b;const auto ref=m.allocation();f.bank.release_42e980(ref);Events e;
        rejects<SourceAllocationError>([&]{advance_track_part_animations(b,m,&e.audit);});rejects<SourceAllocationError>([&]{source(f,b,&e);});
        f.bank.begin_42ea80();const auto second=*f.models.resolve(f.models.load_source(f.bank,"track1"));check(f.models.source_at(second).allocation()!=ref,"generation reused identity");
        rejects<SourceAllocationError>([&]{advance_track_part_animations(b,m,&e.audit);});rejects<std::out_of_range>([&]{advance_track_part_animations(b,f.models,999999u,&e.audit);});
        SourcePmdModelView expired;{ModelResources scoped(f.assets,root);const auto h=*scoped.resolve(scoped.load_source(f.bank,"track1"));expired=scoped.source_at(h);}
        rejects<SourceAllocationError>([&]{advance_track_part_animations(b,expired,&e.audit);});check(same(b,before)&&e.values.empty(),"expired owner changed phase prefix");return;}
    if(name=="thread_guard"){const auto before=b;Events e;std::exception_ptr error;std::thread t([&]{try{
        rejects<SourceAllocationError>([&]{advance_track_part_animations(b,m,&e.audit);});rejects<SourceAllocationError>([&]{source(f,b,&e);});}catch(...){error=std::current_exception();}});t.join();if(error)std::rethrow_exception(error);
        check(same(b,before)&&e.values.empty(),"foreign thread touched state");return;}
    if(name=="unrelated_owner"){f.count(3);const auto other=*f.models.resolve(f.models.load_source(f.bank,"track2"));f.bank.release_42e980(f.models.source_at(other).allocation());compare_steps(f,b,3);return;}
    if(name=="all_kinds"){f.count(32);for(unsigned kind=0;kind<=18;++kind){for(auto& p:b)p.kind=kind==18?0xffffffffu:kind;compare_steps(f,b,32,64);}return;}
    if(name=="event_order"){f.count(4);const auto head=m.descriptor().field<unsigned>(0x48).value();Events e;source(f,b,&e);std::vector<Event> expected{{0x427150,0,4},{0x42715c,0,head}};
        for(unsigned i=1;i<4;++i){expected.push_back({0x427168,i,b[i].kind});expected.push_back({0x4271eb,i,b[i].phase_u});}check(e.values==expected,"phase source event order");return;}
    throw FormatError("unknown track phase contract");
}
}
int main(int argc,char** argv)try{check(argc==3,"expected track phase contract and retail root");run(argv[1],argv[2]);std::cout<<"PASS track phase contract "<<argv[1]<<'\n';return 0;}
catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
