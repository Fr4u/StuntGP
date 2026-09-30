#include "source_pmd_launch_fixture.hpp"
#include <limits>
using namespace sgn;using namespace sgn::launch_test;
namespace {
struct Legacy {
    AssetStore assets;ModelResources models;std::size_t handle;
    Legacy(const std::filesystem::path& root,std::string name="car1"):assets(root),models(assets,root),handle(*models.resolve(models.load(name))){}
};
void old(State& s,unsigned tick,Legacy& l,VehicleModelEffectResource* replacement=nullptr){
    auto& m=l.models.at(l.handle);const auto tm=m.trail_model();const auto settings=initial_vehicle_trail_settings();
    VehicleModelEffectResourceView view(m,l.models.current_detail_level(l.handle));
    VehicleTrailBindings b{s.physics,s.parameters,s.trail_storage,s.basis,s.inputs,tm,s.projection,settings,tick};
    update_vehicle_launch_effect(b,s.effects,s.anchors,replacement?*replacement:view,s.particles,s.random);
}
struct Stops final:VehicleModelEffectResource {
    VehicleModelEffectResourceView view;unsigned stop_part{},stop_uv{};
    Stops(Legacy& l,unsigned p,unsigned uv):view(l.models.at(l.handle),l.models.current_detail_level(l.handle)),stop_part(p),stop_uv(uv){}
    std::uint32_t part_count() const override{return view.part_count();}
    unsigned selected_lod() const override{return view.selected_lod();}
    VehicleModelEffectPart part(unsigned lod,unsigned i) override{if(i==stop_part)throw FormatError("expected legacy point failure");return view.part(lod,i);}
    std::span<std::array<std::uint8_t,8>> attributes(unsigned lod,unsigned i) override{return view.attributes(lod,i);}
    std::span<std::array<std::uint32_t,4>> coordinates(unsigned lod,unsigned i) override{if(i==stop_uv)throw FormatError("expected legacy coordinate failure");return view.coordinates(lod,i);}
};
void same(State& a,State& b){check(packed(a)==packed(b),"complete host state differs from legacy");}
void no_change(State& s,const Record& before){check(packed(s)==before,"unexpected early/prefix state change");}
void zeros(SourcePmdModelView m){coord_span(m,19,0xffffffffu,0);coord_span(m,20,0xffffffffu,0);}
struct Event {unsigned address,at,value;};
struct Hook {
    SourcePmdModelView m;unsigned action{},target{},alternate{};bool failed{};std::vector<Event> events;
    static void call(void* raw,unsigned address,unsigned at,unsigned value) noexcept{
        auto& h=*static_cast<Hook*>(raw);try{
            h.events.push_back({address,at,value});
            if(address==0x4166a6&&h.action==1){h.m.current_detail_level().set(2);h.m.descriptor().field<unsigned>(0x4c).set(0xffffffffu);}
            if(address==0x4167b5&&h.action==2){h.m.bytes().field<unsigned>(at).set(h.alternate);h.m.bytes().field<std::uint16_t>(at-6).set(1);}
            if((address==0x4167d3||address==0x4167ee)&&h.m.bytes().field<unsigned>(at).value()!=value)h.failed=true;
        }catch(...){h.failed=true;}
    }
    VehicleLaunchAudit audit(){return {this,call};}
};
void gates(const std::filesystem::path& root){
    for(bool disabled:{false,true})for(unsigned endpoint=0;endpoint<4;++endpoint){
        Fixture f(root,"car1sh");auto s=state();if(disabled){s->physics.turbo_rpm=0;s->physics.launch_effect_level=0;}else s->anchors.effect_available[0]=2;
        auto before=packed(*s);if(disabled)put(before,0x226c,0);auto view=f.view();const auto allocation=view.allocation();const auto bytes=snapshot(f.bank,allocation);
        if(endpoint==0)source(*s,90,f);
        if(endpoint==1)update_vehicle_launch_effect(binding(*s,90),s->effects,s->anchors,f.models,std::numeric_limits<std::size_t>::max(),s->particles,s->random);
        if(endpoint==2){f.bank.release_42e980(allocation);update_vehicle_launch_effect(binding(*s,90),s->effects,s->anchors,view,s->particles,s->random);}
        if(endpoint==3){Legacy l(root,"car1sh");update_vehicle_launch_effect(binding(*s,90),s->effects,s->anchors,l.models,l.handle,s->particles,s->random);}
        no_change(*s,before);if(endpoint!=2)check(bytes==snapshot(f.bank,allocation),"gate touched PMD");
    }
    std::cout<<"8 checked outer gates incl expired direct, invalid registry, physical shadow\n";
}
void timer_capture(const std::filesystem::path& root){
    {Fixture f(root);auto s=state();s->physics.launch_effect_timer=0;f.view().current_detail_level().set(4);auto expected=packed(*s);put(expected,0x226c,1);
        rejects<FormatError>([&]{source(*s,89,f);});no_change(*s,expected);}
    {Fixture f(root);Legacy l(root);auto s=state(),expected=state();s->physics.launch_effect_timer=0;expected->physics.launch_effect_timer=0;
        f.view().current_detail_level().set(1);f.view().descriptor().field<unsigned>(0x4c).set(0xffffffffu);Hook h{f.view()};auto audit=h.audit();source(*s,89,f,&audit);old(*expected,89,l);same(*s,*expected);
        check(!h.failed&&h.events.size()>1&&h.events[1].address==0x416666&&h.events[1].value==0xffffffffu,"timer0 failed to capture head without payload dereference");}
    std::cout<<"active timer0 validates slot and captures unchecked backing head after enabled\n";
}
void stored_basis(const std::filesystem::path& root){
    Fixture f(root);Legacy l(root);auto s=state(),expected=state();s->basis[0]=2;s->basis[5]=3;s->basis[10]=4;expected->basis=s->basis;
    member(f.view(),0,0,8,0xffffffffu);source(*s,89,f);old(*expected,89,l);same(*s,*expected);
    std::cout<<"tick89 uses previous shared basis despite unavailable raw0 transform\n";
}
void point_failure(const std::filesystem::path& root,unsigned part){
    Fixture f(root);Legacy l(root);auto s=state(),expected=state();s->particles.cursor=31;expected->particles.cursor=31;
    member(f.view(),0,part,8,0xffffffffu);Stops stop(l,part,0);
    rejects<FormatError>([&]{source(*s,89,f);});rejects<FormatError>([&]{old(*expected,89,l,&stop);});same(*s,*expected);
    check(s->physics.launch_effect_timer==0&&s->effects.enabled==1,"point failure lost leading stores");
    check(s->particles.emitted==(part==19?0u:1u),"point failure rolled back particle prefix");
    check((s->random.state!=12345)==(part==20),"point failure RNG prefix");
    std::cout<<"part"<<part<<" failure exact complete legacy prefix including RNG and W\n";
}
void captured_head(const std::filesystem::path& root){
    Fixture f(root);Legacy l(root);auto s=state(),expected=state();f.view().current_detail_level().set(1);l.models.at(l.handle).current_detail_level()=1;
    Hook h{f.view(),1};auto audit=h.audit();source(*s,89,f,&audit);old(*expected,89,l);same(*s,*expected);
    check(!h.failed&&s->particles.emitted==2&&f.view().current_detail_level().value()==2,"head switch was not exercised");
    check(f.view().descriptor().field<unsigned>(0x4c).value()==0xffffffffu,"captured head field not invalidated");
    unsigned captured{};for(auto e:h.events)if(e.address==0x416666)++captured;check(captured==1,"head captured more than once");
    std::cout<<"real current/head switch between particles retained original head and full legacy output\n";
}
void basis_failure(const std::filesystem::path& root){
    Fixture f(root);auto s=state();member(f.view(),0,0,8,0xffffffffu);auto expected=packed(*s);put(expected,0x226c,1);
    rejects<FormatError>([&]{source(*s,90,f);});no_change(*s,expected);std::cout<<"first append basis failure retains enabled only\n";
}
void coordinate_order(const std::filesystem::path& root){
    Fixture f(root);auto s=state();s->physics.launch_effect_timer=0;s->anchors.effect_available[1]=0;auto m=f.view();const auto start=section(m,0x28);
    m.bytes().field<float>(start+4).set(.99f);coord_span(m,19,start,0);Hook h{m,2,0,start+16};auto audit=h.audit();const auto before=snapshot(f.bank,m.allocation());
    source(*s,89,f,&audit);check(!h.failed,"coordinate audit observed wrong stored value");
    std::vector<Event> stores;for(auto e:h.events)if(e.address==0x4167d3||e.address==0x4167ee)stores.push_back(e);
    check(stores.size()==2&&stores[0].at==start+4&&stores[1].at==start+4,"pointer was not captured before count");
    const auto advanced=vehicle_f32(double(.99f)+std::bit_cast<float>(0x3ca3d70au));
    check(stores[0].address==0x4167d3&&stores[0].value==std::bit_cast<unsigned>(advanced),"first UV store missing");
    check(stores[1].address==0x4167ee&&stores[1].value==std::bit_cast<unsigned>(vehicle_f32(double(advanced)-1)),"second UV wrap store missing");
    std::set<unsigned> allowed;for(unsigned j=0;j<4;++j){allowed.insert(start+4+j);allowed.insert(part_at(m,0,19)+0x2c+j);}allowed.insert(part_at(m,0,19)+0x26);allowed.insert(part_at(m,0,19)+0x27);
    brake_test::footprint(before,snapshot(f.bank,m.allocation()),allowed);
    std::cout<<"UV pointer captured before count, two observable stores, complete outside-byte retention\n";
}
void coordinate_prefix(const std::filesystem::path& root){
    Fixture f(root);Legacy l(root);auto s=state(),expected=state();s->physics.launch_effect_timer=0;expected->physics.launch_effect_timer=0;
    const auto m=f.view();const auto last=section(m,0x28)+m.bytes().field<unsigned>(0xbc).value()-16;
    m.bytes().field<float>(last+4).set(.99f);coord_span(m,19,last,2);const auto before=snapshot(f.bank,m.allocation());Stops stop(l,0,19);
    rejects<FormatError>([&]{source(*s,89,f);});rejects<FormatError>([&]{old(*expected,89,l,&stop);});same(*s,*expected);
    const auto advanced=vehicle_f32(double(.99f)+std::bit_cast<float>(0x3ca3d70au));check(m.bytes().field<float>(last+4).value()==vehicle_f32(double(advanced)-1),"first coordinate prefix rolled back");
    brake_test::footprint(before,snapshot(f.bank,m.allocation()),{last+4,last+5,last+6,last+7});std::cout<<"late UV bounds failure retains first coordinate and complete preceding legacy scalar state\n";
}
void coordinate_alias(const std::filesystem::path& root){
    for(unsigned flag:{0u,1u,2u}){Fixture f(root);auto s=state();s->anchors.effect_available[1]=flag;const auto m=f.view();const auto start=section(m,0x28);
        m.bytes().field<float>(start+4).set(2.99f);coord_span(m,19,start,1);coord_span(m,20,start,1);Hook h{m};auto audit=h.audit();source(*s,89,f,&audit);
        float y=2.99f;for(unsigned i=0;i<(flag?2u:1u);++i){y=vehicle_f32(double(y)+std::bit_cast<float>(0x3ca3d70au));if(y>1)y=vehicle_f32(double(y)-1);}
        check(m.bytes().field<float>(start+4).value()==y&&!h.failed,"aliased UV spans lost sequential increments");check(s->particles.emitted==(flag==1?2u:1u),"nonboolean flag particle gate");}
    {Fixture f(root);auto s=state();s->physics.launch_effect_timer=0;zeros(f.view());source(*s,89,f);}
    std::cout<<"aliased coordinate spans sequential; second flag2 enables scale/UV only; count0 skips invalid pointer\n";
}
void lifetime(const std::filesystem::path& root){
    {Fixture f(root);auto view=f.view();f.bank.release_42e980(view.allocation());auto s=state();s->physics.launch_effect_timer=0;auto expected=packed(*s);put(expected,0x226c,1);
        rejects<SourceAllocationError>([&]{update_vehicle_launch_effect(binding(*s,89),s->effects,s->anchors,view,s->particles,s->random);});no_change(*s,expected);}
    {Fixture f(root);auto view=f.view();auto s=state();bool caught{};std::thread worker([&]{try{update_vehicle_launch_effect(binding(*s,89),s->effects,s->anchors,view,s->particles,s->random);}catch(const SourceAllocationError&){caught=true;}});worker.join();check(caught,"cross-thread source access escaped guard");check(s->effects.enabled==1,"cross-thread check moved before enabled");}
    std::cout<<"expired allocation and cross-thread guards remain at active dereference\n";
}
struct TrailHook {
    State& state;SourcePmdModelView model;unsigned count{};bool failed{};std::unique_ptr<Record> prefix;
    static void call(void* raw,const float&,VehicleTrailOpaqueLane lane) noexcept{
        auto& h=*static_cast<TrailHook*>(raw);try{if(lane==VehicleTrailOpaqueLane::cyclic&&++h.count==1){*h.prefix=packed(h.state);member(h.model,0,0,8,0xffffffffu);}}catch(...){h.failed=true;}
    }
};
void second_basis_failure(const std::filesystem::path& root){
    Fixture f(root);auto s=state();const auto before=packed(*s);TrailHook h{*s,f.view(),0,false,std::make_unique<Record>()};VehicleTrailWriteAudit audit{&h,TrailHook::call};
    rejects<FormatError>([&]{source(*s,90,f,nullptr,&audit);});check(h.count==1&&!h.failed,"first trail audit not reached");no_change(*s,*h.prefix);
    check(s->effects.cursor[0]==4&&s->effects.cursor[1]==4&&s->physics.camera_effects.values[0]==3&&s->physics.camera_effects.values[1]==3,"wrong first trail prefix counters");
    check(s->physics.launch_effect_timer==1&&s->random.state==12345&&s->effects.phase==8190,"second basis failure reached later work");
    for(unsigned at=0x3400;at<0x3500;at+=4)check(word(packed(*s),at)==word(before,at),"second ring touched before second basis");
    std::cout<<"second fresh basis fails after exact completed first trail, with timer/RNG/tail untouched\n";
}
void logical_and_alias(const std::filesystem::path& root){
    for(unsigned lod:{0u,1u,2u,3u}){Fixture f(root);Legacy l(root);auto s=state(),expected=state();const auto m=f.view();m.current_detail_level().set(lod);l.models.at(l.handle).current_detail_level()=lod;
        m.descriptor().field<unsigned>(0x24).set(1);member(m,0,23,8,0xffffffffu);source(*s,90,f);old(*expected,90,l);same(*s,*expected);}
    {Fixture f(root);auto s=state(),expected=state();const auto m=f.view();m.current_detail_level().set(1);const auto ptr=transform_at(m,1,19);member(m,1,20,8,ptr);
        //A genuine legacy view is modified to the same authored shared position;
        //only the value used by launch_position differs, no launch formula is copied.
        Legacy l(root);l.models.at(l.handle).current_detail_level()=1;auto& mm=l.models.at(l.handle);mm.source_part(mm.source_lod_index(1),20).transform.position=mm.source_part(mm.source_lod_index(1),19).transform.position;
        source(*s,89,f);old(*expected,89,l);same(*s,*expected);}
    std::cout<<"four raw/current slots, physical parts beyond logicalcount1, unused part23 and shared point transform\n";
}
void scalar_boundaries(const std::filesystem::path& root){
    unsigned count{};for(unsigned timer:{0u,1u,2u,0xffffffffu})for(int level:{std::numeric_limits<int>::min(),-1,0,255,std::numeric_limits<int>::max()})for(float turbo:{-0.0f,10.0f,10.000001f}){
        Fixture f(root);Legacy l(root);auto s=state(),expected=state();s->physics.launch_effect_timer=timer;expected->physics.launch_effect_timer=timer;s->physics.launch_effect_level=level;expected->physics.launch_effect_level=level;s->physics.turbo_rpm=turbo;expected->physics.turbo_rpm=turbo;
        s->random.state=0xffffffffu;expected->random.state=0xffffffffu;s->particles.cursor=0xffffffffu;expected->particles.cursor=0xffffffffu;source(*s,89,f);old(*expected,89,l);same(*s,*expected);++count;
    }std::cout<<count<<" finite scalar boundaries exact genuine legacy: signed level wrap/clamp, signed-zero gate, timer unsigned decrement, RNG/cursor wrap\n";
}

void retail_cases(const std::filesystem::path& root){
    unsigned cases{};std::uint64_t intact{};
    for(unsigned car=1;car<=20;++car)for(unsigned tick:{89u,90u})for(unsigned flag:{1u,2u}){
        const auto name="car"+std::to_string(car);Fixture f(root,name);Legacy l(root,name);auto s=state(),expected=state();s->anchors.effect_available[1]=flag;expected->anchors.effect_available[1]=flag;
        s->particles.cursor=31;expected->particles.cursor=31;const auto m=f.view();const auto before=snapshot(f.bank,m.allocation());std::set<unsigned> allowed;
        for(unsigned i:{19u,20u}){const auto p=m.part(m.source_lod_index(0),i);for(unsigned j=0;j<p.coordinates().size();++j){const auto at=p.coordinates().at(j).location().offset+4;for(unsigned k=0;k<4;++k)allowed.insert(at+k);}}
        source(*s,tick,f);old(*expected,tick,l);same(*s,*expected);intact+=brake_test::footprint(before,snapshot(f.bank,m.allocation()),allowed);
        for(unsigned i:{19u,20u}){const auto coords=l.models.at(l.handle).source_coordinates(0,i);const auto part=m.part(m.source_lod_index(0),i);check(coords.size()==part.coordinates().size(),"retail coordinate count");
            for(unsigned j=0;j<coords.size();++j)check(coords[j]==part.coordinates().at(j).value(),"retail UV differs from genuine legacy");}
        ++cases;
    }
    std::cout<<"{\"retail_cases\":"<<cases<<",\"unchanged_pmd_bytes\":"<<intact<<",\"mismatches\":0}\n";
}
}
int main(int argc,char** argv)try{
    check(argc==3,"expected launch contract and retail root");const std::string_view mode=argv[1];const std::filesystem::path root=argv[2];
    if(mode=="gates")gates(root);else if(mode=="timer_capture")timer_capture(root);else if(mode=="stored_basis")stored_basis(root);
    else if(mode=="first_point_failure")point_failure(root,19);else if(mode=="second_point_failure")point_failure(root,20);else if(mode=="captured_head")captured_head(root);
    else if(mode=="basis_failure")basis_failure(root);else if(mode=="coordinate_order")coordinate_order(root);else if(mode=="coordinate_prefix")coordinate_prefix(root);
    else if(mode=="coordinate_alias")coordinate_alias(root);else if(mode=="lifetime")lifetime(root);else if(mode=="second_basis_failure")second_basis_failure(root);else if(mode=="logical_and_alias")logical_and_alias(root);else if(mode=="scalar_boundaries")scalar_boundaries(root);else if(mode=="retail80")retail_cases(root);else throw std::runtime_error("unknown launch contract");return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
