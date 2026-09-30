#include "source_pmd_pair_trails_fixture.hpp"
#include <thread>
using namespace sgn;using namespace sgn::pair_trail_test;
namespace {
void inactive(const std::filesystem::path& root,bool disabled){
    for(bool b:{false,true}){Fixture f(root,"car1sh");const auto stale=f.view();AssetStore assets(root);ModelResources old(assets,root);const auto handle=*old.resolve(old.load("car1sh"));
        for(unsigned mode=0;mode<4;++mode){State s;s.tick=disabled?60u:59u;s.inputs.field_28a0=disabled?0u:1u;const auto before=s.record();
            if(mode==0){const auto bytes=snapshot(f.bank,f.view().allocation());source(s,b,f.models,f.handle);unchanged_pmd(f,bytes);}
            else if(mode==1)source(s,b,f.models,std::size_t(-1));
            else if(mode==2){f.bank.release_42eaa0();source(s,b,stale);}
            else source(s,b,old,handle);
            check(s.record()==before,"inactive pair accessed model or wrote state");}
    }
    std::cout<<"GREEN "<<(disabled?"disabled":"early")<<": both pairs; car1sh, invalid handle, expired view and legacy registry; full no-op\n";
}
void active_shadow(const std::filesystem::path& root){
    Fixture f(root,"car1sh");const auto view=f.view();const auto basis=observe_basis(view);const auto location=transform(view,0);
    check(location+80<=view.bytes().size(),"car1sh part0 extent");
    for(float v:{basis.base_forward.x,basis.base_forward.y,basis.base_forward.z,basis.base_up.x,basis.base_up.y,basis.base_up.z})check(std::isfinite(v),"car1sh part0 nonfinite");
    const auto crossed=vehicle_cross(basis.base_forward,basis.base_up);
    check(double(crossed.x)*crossed.x+double(crossed.y)*crossed.y+double(crossed.z)*crossed.z>0,"car1sh part0 degenerate basis");
    AssetStore assets(root);ModelResources old(assets,root);const auto handle=*old.resolve(old.load("car1sh"));
    check(old.at(handle).part_count(0)<24,"car1sh no longer demonstrates missing part23");
    for(bool b:{false,true}){State s;s.model=basis;State expected=s;const auto before=s.record();legacy(expected,b);
        const auto pmd=snapshot(f.bank,view.allocation());source(s,b,f.models,f.handle);check(s.record()==expected.record(),"active car1sh canonical differs");unchanged_pmd(f,pmd);
        check(s.record()!=before,"active car1sh did not append");State legacy_registry;legacy_registry.model=basis;source(legacy_registry,b,old,handle);
        check(legacy_registry.record()==expected.record(),"active car1sh lazy legacy differs");}
    std::cout<<"GREEN active: both pairs; real car1sh part0 finite/nondegenerate/extent verified; source and legacy registry append; part_count="<<old.at(handle).part_count(0)<<"\n";
}
void unused_part23(const std::filesystem::path& root){
    Fixture f(root);const auto m=f.view();const auto basis=observe_basis(m);pointer(m,23,0xffffffffu);
    m.descriptor().field<unsigned>(0x24).set(0);m.current_detail_level().set(0xffffffffu);
    //The invalid pointer is real and checked, but only the separate control reads it.
    rejects<FormatError>([&]{(void)transform(m,23);});
    for(bool b:{false,true})for(unsigned flag:{2u,0xffffffffu}){State s;s.inputs.field_28a0=flag;s.inputs.pair_b_available={0,0};s.model=basis;State expected=s;legacy(expected,b);
        const auto before=snapshot(f.bank,m.allocation());source(s,b,f.models,f.handle);check(s.record()==expected.record(),"unused part23/current/count/availability affected pair");unchanged_pmd(f,before);}
    std::cout<<"GREEN unused part23: invalid pointer control rejects; both active pairs ignore it, logical count0/currentFFFFFFFF and availability0\n";
}
void retail_cases(const std::filesystem::path& root){
    unsigned pairs{},shadows{};std::uint64_t intact{};
    for(unsigned id=1;id<=20;++id){const auto name="car"+std::to_string(id);Fixture f(root,name);AssetStore assets(root);ModelResources old(assets,root);const auto h=*old.resolve(old.load(name));
        for(unsigned current:{0u,1u,3u,0xffffffffu})for(bool b:{false,true}){State actual;actual.model=old.at(h).trail_model();actual.inputs.field_28a0=2;
            actual.inputs.pair_b_available={0,0};State expected=actual;const auto m=f.view();m.current_detail_level().set(current);m.descriptor().field<unsigned>(0x24).set(0);
            const auto before=snapshot(f.bank,m.allocation());source(actual,b,f.models,f.handle);legacy(expected,b);check(actual.record()==expected.record(),"retail pair differs from existing observation");intact+=unchanged_pmd(f,before);++pairs;}
        Fixture shadow(root,name+"sh");for(bool b:{false,true}){State s;s.model=observe_basis(shadow.view());State expected=s;const auto before=snapshot(shadow.bank,shadow.view().allocation());
            source(s,b,shadow.models,shadow.handle);legacy(expected,b);check(s.record()==expected.record(),"retail shadow active pair differs");intact+=unchanged_pmd(shadow,before);++shadows;}}
    std::cout<<"{\"car_pairs\":"<<pairs<<",\"active_shadows\":"<<shadows<<",\"unchanged_pmd_bytes\":"<<intact<<"}\n";
}
void first_failure(const std::filesystem::path& root){
    for(bool b:{false,true}){Fixture f(root);const auto m=f.view();pointer(m,0,0xffffffffu);State s;const auto before=s.record();
        rejects<FormatError>([&]{source(s,b,f.models,f.handle);});check(s.record()==before,"first basis failure wrote pair state");
        //Existing host capacity guard remains before the provider, without changing source profiles.
        if(b)s.settings.pair_b.capacity=0;else s.settings.pair_a.capacity=0;
        rejects<std::domain_error>([&]{source(s,b,f.models,f.handle);});check(s.record()==before,"capacity rejection wrote pair state");}
}
struct Intervention {
    Fixture& fixture;State& state;bool second;bool fail;unsigned calls{};bool error{};
    static void receive(void* context,const float&,VehicleTrailOpaqueLane)noexcept{
        auto& h=*static_cast<Intervention*>(context);++h.calls;if(h.calls!=4)return;
        try{const auto model=h.fixture.view();
            if(h.fail)pointer(model,0,0xffffffffu);
            else{model.descriptor().field<unsigned>(0x48).set(model.descriptor().field<unsigned>(0x4c).value());
                if(h.second)h.state.inputs.pair_b[1].z+=31.0f;else h.state.inputs.pair_a[1].z+=31.0f;}
        }catch(...){h.error=true;}
    }
};
void second_observation(const std::filesystem::path& root,bool fail){
    for(bool b:{false,true}){Fixture f(root);const auto m=f.view();const auto raw0=m.descriptor().field<unsigned>(0x48).value();
        //Make raw1's part0 basis measurably different; original part0 remains valid.
        const auto other=transform(m,1);const auto raw1part=m.part(m.source_lod_index(1),0).location().offset;
        check(other!=transform(m,0)&&m.descriptor().field<unsigned>(0x4c).value()!=raw0,"alias witness requires distinct physical records");
        m.bytes().field<unsigned>(raw1part+8).set(other);
        m.bytes().field<VehicleVector>(other+32).set({1,0,0,std::bit_cast<float>(0x7fc07890u)});m.bytes().field<VehicleVector>(other+48).set({0,1,0,0});
        State actual;actual.model=observe_basis(m);State expected=actual;append_one(expected,b,0);
        if(!fail){m.descriptor().field<unsigned>(0x48).set(m.descriptor().field<unsigned>(0x4c).value());expected.model=observe_basis(m);
            m.descriptor().field<unsigned>(0x48).set(raw0);if(b)expected.inputs.pair_b[1].z+=31.0f;else expected.inputs.pair_a[1].z+=31.0f;
            const auto original_model=actual.model;append_one(expected,b,1);expected.model=original_model;}
        Intervention intervention{f,actual,b,fail};VehicleTrailWriteAudit hook{&intervention,&Intervention::receive};actual.audit=&hook;
        if(fail)rejects<FormatError>([&]{source(actual,b,f.models,f.handle);});else source(actual,b,f.models,f.handle);
        check(!intervention.error,"intervention failed");check(intervention.calls==(fail?4u:8u),"pair provider/audit call count");
        check(actual.record()==expected.record(),fail?"second failure did not preserve exact first result":"second append reused basis or input snapshot");
        if(fail)std::cout<<"GREEN second failure pair "<<(b?'b':'a')<<": first projected result retained, second ring untouched; authored intervention\n";
    }
}
struct Audit {
    std::array<const float*,8> addresses{};std::array<unsigned,8> bits{};std::array<VehicleTrailOpaqueLane,8> lanes{};unsigned count{};bool overflow{};
    static void receive(void* p,const float& value,VehicleTrailOpaqueLane lane)noexcept{auto& a=*static_cast<Audit*>(p);if(a.count>=8){a.overflow=true;return;}
        a.addresses[a.count]=&value;a.bits[a.count]=std::bit_cast<unsigned>(value);a.lanes[a.count]=lane;++a.count;}
};
void audit_ring_projection(const std::filesystem::path& root){
    Fixture f(root);for(bool b:{false,true})for(bool reject_projection:{false,true}){State s;s.model=observe_basis(f.view());
        s.projection.clip_near=reject_projection?1.0e10f:-1.0e10f;const auto capacity=b?9u:5u;
        for(unsigned i=0;i<2;++i){(b?s.storage.cursor_b[i]:s.storage.cursor_a[i])=0xffffffffu;s.physics.camera_effects.values[(b?6u:4u)+i]=i?capacity+7:capacity-1;}
        s.scratch[3]=std::bit_cast<float>(0x7fc01234u);s.scratch[7]=std::bit_cast<float>(0x7fc05678u);State expected=s;legacy(expected,b);
        Audit audit;VehicleTrailWriteAudit hook{&audit,&Audit::receive};s.audit=&hook;source(s,b,f.models,f.handle);
        check(s.record()==expected.record(),"ring/projection result differs");check(audit.count==8&&!audit.overflow,"pair audit not eight observations");
        for(unsigned i=0;i<2;++i){const auto& ring=b?s.storage.pair_b[i]:s.storage.pair_a[i];const auto base=i*4;
            const std::array<const float*,4> addresses{&s.scratch[3],&s.scratch[7],&ring[capacity].w,&ring[0].w};
            for(unsigned j=0;j<4;++j){check(audit.addresses[base+j]==addresses[j],"pair audit owner identity");check(audit.bits[base+j]==std::bit_cast<unsigned>(*addresses[j]),"pair opaque W changed");}
            check(audit.lanes[base]==VehicleTrailOpaqueLane::basis_x&&audit.lanes[base+1]==VehicleTrailOpaqueLane::basis_y&&audit.lanes[base+2]==VehicleTrailOpaqueLane::latest&&audit.lanes[base+3]==VehicleTrailOpaqueLane::cyclic,"pair audit lane order");
            check((b?s.storage.cursor_b[i]:s.storage.cursor_a[i])==0,"pair unsigned cursor wrap");
            check(s.physics.camera_effects.values[(b?6u:4u)+i]==(i?capacity+7:capacity),"pair count increment/saturation");
            check((ring[0].x==ring[capacity].x&&ring[0].y==ring[capacity].y&&ring[0].z==ring[capacity].z)==reject_projection,"projection accept/reject witness missing");}
    }
}
void lifetime(const std::filesystem::path& root){
    for(bool b:{false,true}){Fixture f(root),other(root);const auto stale=f.view();f.bank.release_42eaa0();State s;const auto before=s.record();
        rejects<SourceAllocationError>([&]{source(s,b,stale);});check(s.record()==before,"expired active pair wrote state");
        rejects<std::out_of_range>([&]{source(s,b,other.models,std::size_t(-1));});bool caught=false;
        std::thread worker([&]{try{State foreign;source(foreign,b,other.models,other.handle);}catch(const SourceAllocationError&){caught=true;}});worker.join();check(caught,"foreign thread pair accepted");
        source(s,b,other.models,other.handle);}
}
void unused_up_w(const std::filesystem::path& root){Fixture f(root);for(bool b:{false,true}){State a,before;const auto m=f.view();const auto at=transform(m,0);
    m.bytes().field<unsigned>(at+60).set(0x7fc03456u);source(a,b,f.models,f.handle);m.bytes().field<unsigned>(at+60).set(0xffffffffu);source(before,b,f.models,f.handle);
    check(a.record()==before.record(),"unused upW affects pair");}}
}
int main(int argc,char** argv)try{check(argc==3,"expected pair contract and retail root");const std::string_view mode=argv[1];
    if(mode=="early_green")inactive(argv[2],false);else if(mode=="disabled_green")inactive(argv[2],true);else if(mode=="active_shadow_green")active_shadow(argv[2]);
    else if(mode=="unused_part23")unused_part23(argv[2]);else if(mode=="retail40")retail_cases(argv[2]);else if(mode=="first_failure")first_failure(argv[2]);
    else if(mode=="second_failure")second_observation(argv[2],true);else if(mode=="fresh_basis_input")second_observation(argv[2],false);else if(mode=="audit_ring_projection")audit_ring_projection(argv[2]);
    else if(mode=="lifetime")lifetime(argv[2]);else if(mode=="unused_up_w")unused_up_w(argv[2]);else throw std::runtime_error("unknown pair contract");
    std::cout<<"passed "<<mode<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
