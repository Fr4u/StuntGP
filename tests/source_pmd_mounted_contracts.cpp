#include "source_pmd_mounted_fixture.hpp"
#include <thread>
using namespace sgn;using namespace sgn::mounted_test;
namespace {
unsigned failures{},successes{};
struct Failure {std::string type,message;unsigned site;};
template<class F>Failure failure(F fn){try{fn();}catch(const SourceAllocationError& e){return {"SourceAllocationError",e.what(),e.source_site};}
    catch(const FormatError& e){return {"FormatError",e.what(),0};}catch(const std::out_of_range& e){return {"out_of_range",e.what(),0};}throw FormatError("expected exact mounted rejection");}
void direct_failures(const std::filesystem::path& root){
    for(bool registry:{false,true})for(bool second:{false,true})for(const auto kind:{"release","descriptor","rawhead","transform"}){
        Fixture f(root);const auto m=f.model(),ref=m;const auto allocation=m.allocation();const auto a=axes(m);
        auto c=camera(),expected=c;auto v=vehicle(),ev=v;expected_prefix(expected,ev,a,second);
        const unsigned part=m.part(m.source_lod_index(0),0).location().offset;
        Hook hook;hook.fn=[&](unsigned site){if(site!=(second?0x404ec8u:0x404e1cu))return;
            if(std::string_view(kind)=="release")f.bank.release_42e980(allocation);
            else if(std::string_view(kind)=="descriptor")m.bytes().field<unsigned>(0x4c).set(0xffffffffu);
            else if(std::string_view(kind)=="rawhead")m.descriptor().field<unsigned>(0x48).set(0);
            else m.bytes().field<unsigned>(part+8).set(1);
        };auto audit=hook.audit();
        const auto error=failure([&]{if(registry)camera_mounted_tick(c,v,f.models,f.handle,&audit);else camera_mounted_tick(c,v,ref,&audit);});
        check(camera_bytes(c)==camera_bytes(expected)&&vehicle_bytes(v)==vehicle_bytes(ev),"late direct/registry failure retained wrong complete prefix");
        check(hook.sites==std::vector<unsigned>(second?std::initializer_list<unsigned>{0x404e1c,0x404ec8}:std::initializer_list<unsigned>{0x404e1c}),"axis sequence changed");
        if(std::string_view(kind)=="release")check(error.type=="SourceAllocationError"&&error.site==0&&error.message=="allocation borrow is null, foreign, expired or already released","released axis exact diagnostic");
        else {const char* message=std::string_view(kind)=="descriptor"?"PMD normalized source offset overflow":std::string_view(kind)=="rawhead"?"PMD part was not relocated in this allocation":"PMD relocated pointer has wrong section/alignment";
            check(error.type=="FormatError"&&error.site==0&&error.message==message,"malformed axis exact diagnostic");}
        std::cout<<"failure route="<<(registry?"registry":"direct")<<" axis="<<(second?"forward":"up")<<" mutation="<<kind<<" type="<<error.type<<" site="<<error.site<<" message="<<error.message<<'\n';++failures;
    }
}
void independent_axes(const std::filesystem::path& root){
    for(bool registry:{false,true})for(const auto kind:{"pose","pointer","rawhead","poison_unused"}){
        Fixture f(root);auto m=f.model();const auto part=m.part(m.source_lod_index(0),0).location().offset;
        const auto trans=m.part(m.source_lod_index(0),0).transform();const auto other=m.part(m.source_lod_index(0),1).transform();
        auto input=trans.value();input.up={.25f,.5f,.75f,9};input.forward={1,2,3,10};trans.set(input);
        auto next=input;next.up={100,200,300,11};next.forward={4,5,6,12};
        unsigned head=0;
        if(std::string_view(kind)=="rawhead"){
            head=m.descriptor().field<unsigned>(0x4c).value();check(head!=m.descriptor().field<unsigned>(0x48).value(),"fixture second raw range");
        }
        auto c=camera(),ec=c;auto v=vehicle(),ev=v;auto expected_axes=axes(m);
        if(std::string_view(kind)!="poison_unused")expected_axes.forward=std::bit_cast<CameraVector>(next.forward);
        Hook hook;hook.fn=[&](unsigned site){if(site!=0x404ec8)return;
            check(c.up.x==-.25f&&c.up.y==.5f&&c.up.z==.75f,"forward acquisition precedes up stores");
            if(std::string_view(kind)=="pose")trans.set(next);
            else if(std::string_view(kind)=="pointer"){other.set(next);m.bytes().field<unsigned>(part+8).set(other.location().offset);}
            else if(std::string_view(kind)=="rawhead"){other.set(next);m.bytes().field<unsigned>(head+8).set(other.location().offset);m.descriptor().field<unsigned>(0x48).set(head);}
        };
        if(std::string_view(kind)=="poison_unused"){auto d=m.descriptor();d.field<unsigned>(0x58).set(0xffffffffu);d.field<unsigned>(0x5c).set(0xffffffffu);d.field<unsigned>(0x24).set(0);}
        auto audit=hook.audit();if(registry)camera_mounted_tick(c,v,f.models,f.handle,&audit);else camera_mounted_tick(c,v,m,&audit);
        camera_mounted_tick(ec,ev,expected_axes);check(camera_bytes(c)==camera_bytes(ec)&&vehicle_bytes(v)==vehicle_bytes(ev),"fresh forward chain or irrelevant field dependency");++successes;
    }
}
void frame_failures(const std::filesystem::path& root){
    for(unsigned failing:{0u,1u,3u})for(bool second:{false,true})for(bool pending:{false,true})for(bool shared:{false,true}){
        Fixture f(root);auto& s=*f.state;auto m=f.model();const auto a=axes(m);const auto allocation=m.allocation();
        std::array<CameraState,4> expected;std::array<CameraLifecycle,4> expected_life;std::array<CameraVehicle,6> ev;
        for(unsigned i=0;i<4;++i){auto& c=s.controls.controllers[i];c.active=i<=failing?1u:2u;c.vehicle=shared?0u:i;
            if(pending){s.camera_lifecycle[i].pending_record=0;s.camera_lifecycle[i].pending_mode=6;c.camera.mode=11;c.camera.pending_lens=1;c.camera.requested_lens=4;}
            expected[i]=c.camera;expected_life[i]=s.camera_lifecycle[i];}
        for(unsigned i=0;i<6;++i)ev[i]=s.vehicles.camera_snapshot(i);
        auto shared_matrix=s.shared_model_identity;auto projection=s.frame_projection;auto inv=s.inverse_depth_scale,dep=s.depth_offset;
        for(unsigned i=0;i<=failing;++i){auto& c=expected[i];auto& v=ev[shared?0:i];auto& life=expected_life[i];
            if(pending){camera_apply_record(c,life,v,s.camera_bank,s.model_draws.template_pose,shared_matrix);camera_consume_lens(c,shared_matrix);projection=c.projection;inv=c.inverse_depth_scale;dep=c.depth_offset;}
            if(i<failing){camera_mounted_tick(c,v,a);camera_integrate_velocity(c);camera_integrate_angles(c);}
            else expected_prefix(c,v,a,second);
        }
        Hook hook;hook.fn=[&](unsigned site){
            const auto i=s.controls.current;auto& orbit=s.vehicles.camera_settings[s.controls.controllers[i].vehicle].orbit_offset;
            check(orbit.x==0&&orbit.y==100&&orbit.z==0,"actual VehicleCameraSettings orbit missing before model read");
            if(i==failing&&site==(second?0x404ec8u:0x404e1cu))f.bank.release_42e980(allocation);
        };auto audit=hook.audit();auto world=f.world();const auto error=failure([&]{call_frame(world,&audit);});
        check(error.type=="SourceAllocationError"&&error.site==0&&error.message=="allocation borrow is null, foreign, expired or already released","frame real release diagnostic");
        for(unsigned i=0;i<4;++i)check(camera_bytes(s.controls.controllers[i].camera,s.camera_lifecycle[i])==camera_bytes(expected[i],expected_life[i]),"frame current/previous/future controller prefix differs");
        for(unsigned i=0;i<6;++i)check(vehicle_bytes(s.vehicles.camera_snapshot(i))==vehicle_bytes(ev[i]),"frame complete pervehicle owner differs");
        check(s.controls.current==failing&&s.frame_projection==projection&&s.shared_model_identity==shared_matrix&&s.inverse_depth_scale==inv&&s.depth_offset==dep,"frame selector/lens publication lost on failure");
        check(hook.sites.size()==2*failing+(second?2:1),"frame preflight or tail read occurred");
        std::cout<<"failure route=frame controller="<<failing<<" axis="<<(second?"forward":"up")<<" pending="<<pending<<" shared="<<shared<<" prior_complete="<<failing<<" type="<<error.type<<" site="<<error.site<<'\n';++failures;
    }
}
void frame_live(const std::filesystem::path& root){
    Fixture f(root);auto& s=*f.state;const auto m=f.model();const auto first=axes(m);const auto second=*f.models.resolve(f.models.load_source(f.bank,"car16"));const auto other=f.models.source_at(second);
    auto second_axes=axes(other);auto pose=other.part(other.source_lod_index(0),0).transform();auto v=pose.value();v.forward={4,5,6,7};v.up={.5f,.25f,.75f,8};pose.set(v);second_axes=axes(other);
    for(unsigned i=0;i<4;++i){s.controls.controllers[i].active=1;s.controls.controllers[i].vehicle=0;}
    std::array<CameraState,4> expected;for(unsigned i=0;i<4;++i)expected[i]=s.controls.controllers[i].camera;
    auto expected_vehicle=s.vehicles.camera_snapshot(0);
    for(unsigned i=0;i<4;++i){auto a=second_axes;if(i==0)a.up=first.up;camera_mounted_tick(expected[i],expected_vehicle,a);camera_integrate_velocity(expected[i]);camera_integrate_angles(expected[i]);}
    const auto before1=pmd_bytes(f.bank,m.allocation()),before2=pmd_bytes(f.bank,other.allocation());Hook hook;
    hook.fn=[&](unsigned site){if(s.controls.current==0&&site==0x404ec8){s.vehicles.models[0].model=second;s.vehicles.camera_settings[0].distance=456;}
        if(s.controls.current==1&&site==0x404e1c)check(s.vehicles.camera_settings[0].distance==456,"later controller overwrote unrelated current owner");};auto audit=hook.audit();auto world=f.world();call_frame(world,&audit);
    for(unsigned i=0;i<4;++i)check(camera_bytes(s.controls.controllers[i].camera)==camera_bytes(expected[i]),"fresh registry handle across axes/controllers");
    expected_vehicle.distance=456;check(vehicle_bytes(s.vehicles.camera_snapshot(0))==vehicle_bytes(expected_vehicle),"mounted stale snapshot overwrote settings");
    check(s.controls.current==0&&hook.sites.size()==8,"successful current reset/axis count");check(before1==pmd_bytes(f.bank,m.allocation())&&before2==pmd_bytes(f.bank,other.allocation()),"frame camera mutated PMD");++successes;
    // A skipped controller is published but its invalid vehicle/model/record is never observed.
    s.controls.controllers[3].active=2;s.controls.controllers[3].vehicle=999;s.camera_lifecycle[3].pending_record=999;
    s.current_slot=999;s.camera_bank.count=999;s.controls.current=999;for(unsigned i=0;i<3;++i)s.camera_lifecycle[i].current_record=999;
    call_frame(world);check(s.controls.current==0,"mode6 introduced unused global/record dependencies");++successes;
}
void lifetime(const std::filesystem::path& root){
    Fixture f(root);auto m=f.model();const auto a=axes(m);const auto before=pmd_bytes(f.bank,m.allocation());
    std::exception_ptr error;std::thread worker([&]{try{for(bool registry:{false,true}){auto c=camera(),ec=c;auto v=vehicle(),ev=v;expected_prefix(ec,ev,a,false);
        auto result=failure([&]{if(registry)camera_mounted_tick(c,v,f.models,f.handle);else camera_mounted_tick(c,v,m);});
        check(result.type=="SourceAllocationError"&&result.site==0&&result.message=="source allocation bank belongs to its creating thread","mounted thread guard weakened");
        check(camera_bytes(c)==camera_bytes(ec)&&vehicle_bytes(v)==vehicle_bytes(ev),"thread failure prefix");++failures;}}
        catch(...){error=std::current_exception();}});worker.join();if(error)std::rethrow_exception(error);check(before==pmd_bytes(f.bank,m.allocation()),"thread failure PMD write");
    SourcePmdModelView expired;{ModelResources scoped(f.assets,root);auto h=*scoped.resolve(scoped.load_source(f.bank,"car1"));expired=scoped.source_at(h);}
    for(auto view:{expired,SourcePmdModelView{}}){auto c=camera(),ec=c;auto v=vehicle(),ev=v;expected_prefix(ec,ev,a,false);
        auto result=failure([&]{camera_mounted_tick(c,v,view);});check(result.type=="SourceAllocationError"&&result.site==0x42efb0&&result.message=="expired PMD model metadata","metadata guard changed");
        check(camera_bytes(c)==camera_bytes(ec)&&vehicle_bytes(v)==vehicle_bytes(ev),"metadata failure prefix");++failures;}
}
}
int main(int argc,char** argv)try{check(argc==2,"mounted contracts retail root");direct_failures(argv[1]);independent_axes(argv[1]);frame_failures(argv[1]);frame_live(argv[1]);lifetime(argv[1]);
    std::cout<<"mounted_contracts failures="<<failures<<" successes="<<successes<<" mismatches=0 excluded_words=0 original_execution=0 Session_execution=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
