#include "sgn/source_world_track_view.hpp"
#include <algorithm>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <string_view>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
unsigned checks{};
void check(bool value,const char* message){++checks;if(!value)throw FormatError(message);}
template<class Exception,class F>void rejects(F&& action){
    bool rejected=false;try{action();}catch(const Exception&){rejected=true;}check(rejected,"missing expected typed rejection");
}
template<class T>Bytes raw(const T& value){
    const auto* first=reinterpret_cast<const std::uint8_t*>(&value);return {first,first+sizeof(T)};
}
CameraMatrix translated(float x,float y,float z){
    CameraMatrix m{1,0,0,0,0,1,0,0,0,0,1,0,x,y,z,1};return m;
}
void same_matrix(const CameraMatrix& actual,const CameraMatrix& expected){check(raw(actual)==raw(expected),"full16-DWORD authored matrix mismatch");}
struct Audit {
    std::vector<std::uint32_t> sites;
    std::function<void(std::uint32_t)> action;
    SourceTrackInitialViewAudit binding(){return {this,[](void* context,std::uint32_t site){
        auto& self=*static_cast<Audit*>(context);self.sites.push_back(site);if(self.action)self.action(site);
    }};}
};
struct Fixture {
    AssetStore assets;SourceAllocationBank bank;SceneResources scene;
    std::unique_ptr<WorldModelDrawState> draws=std::make_unique<WorldModelDrawState>();
    std::unique_ptr<SourceSpritePacketStorage> sprites=std::make_unique<SourceSpritePacketStorage>();
    ViewMatrixArena arena;CameraState camera{};
    CameraMatrix identity=translated(0,0,0),destination=translated(991,992,993);
    std::uint32_t accepted=99;
    explicit Fixture(const std::filesystem::path& root):assets(root),scene(assets,root){
        bank.initialize_42eab0(); //receiving bootstrap only, never candidate behavior
        scene.bind_sprite_4340f9(*draws,*sprites);
        const std::array<LevelAssetRequest,1> requests{{{"track1",3}}};
        const std::array<SelectionRecord,6> selection{};std::array<LevelCarSlot,6> cars{};
        scene.bind_source_models(bank,requests,selection,cars);
        check(bank.live_count()==1&&scene.models().size()==1,"fixture did not load one actual canonical track");
        check(model().part_count()==8,"pinned track1 part count changed");
        camera.position={1,2,3,0};camera.world_to_camera_reference=1;
        arena.cursor=0;arena.count=0;arena.matrices[0]=identity;
        draws->template_pose.position={10,20,30};draws->template_pose.tail[1]=1;draws->template_pose.tail[2]=1;
        draws->background.records[0]={scene.models().identity(scene.model_handle(3)),draws->template_pose};
        draws->background.records[0].pose.position={100,200,300};
        model().descriptor().field<VehicleVector>(0x10).set({0,0,0,0});
        auto first=model().part(0,0);auto second=model().part(0,1);
        check(first.transform().location()!=second.transform().location(),"authored distinct-part witness needs distinct transforms");
        prepare_part(first,{3,4,5,0});prepare_part(second,{7,8,9,0});
    }
    static void prepare_part(SourcePmdPartView part,VehicleVector position){
        auto transform=part.transform().value();transform.position=position;transform.scale={1,1,1,0};
        transform.tail[0]=1;transform.tail[1]=1;part.transform().set(transform);part.pivot().set({0,0,0,0});
    }
    SourcePmdModelView model(){return scene.models().source_at(*scene.model_handle(3));}
    SourceWorldTrackContext context(){return SourceWorldTrackContext(scene,*draws,*sprites);}
    SourcePmdField<std::uint32_t> outer(){return model().descriptor().field<std::uint32_t>(0x0c);}
    Bytes pmd(){const auto bytes=bank.bytes(model().allocation());return {bytes.begin(),bytes.end()};}
    void template_binding(){check(context().publish_initial_template_42e7cc(),"track initial publication skipped");}
    void prefix(Audit* audit=nullptr){auto c=context();prepare_source_track_initial_view(c,camera,arena,identity,destination,accepted,audit?audit->binding():SourceTrackInitialViewAudit{});}
    void count(std::uint32_t n){model().descriptor().field<std::uint32_t>(0x24).set(n);}
};
struct Owners {
    Bytes pmd,draws,sprites,arena,camera,identity;
    explicit Owners(Fixture& f):pmd(f.pmd()),draws(raw(*f.draws)),sprites(raw(*f.sprites)),arena(raw(f.arena)),camera(raw(f.camera)),identity(raw(f.identity)){}
    void unchanged(Fixture& f) const{
        check(pmd==f.pmd(),"prefix changed canonical PMD");check(draws==raw(*f.draws),"prefix changed retained draw owners");
        check(sprites==raw(*f.sprites),"prefix changed sprite storage");check(arena==raw(f.arena),"prefix wrote matrix arena");
        check(camera==raw(f.camera)&&identity==raw(f.identity),"prefix changed camera/shared identity");
    }
};
void only_outer_change(Fixture& f,Bytes before,std::uint32_t expected){
    const auto at=f.outer().location().offset;std::memcpy(before.data()+at,&expected,sizeof(expected));
    check(before==f.pmd(),"publication changed bytes outside actual canonical descriptor0C");
}
void completed_sites(const Audit& a,bool part){
    std::vector<std::uint32_t> expected{0x42b04b,0x42b055,0x44fbe3,0x44fbe6,0x44fbeb};
    if(part)expected.push_back(0x44fc00);expected.push_back(0x44fca2);expected.push_back(0x44fd50);
    check(a.sites==expected,"statement boundary order differs");
}
void run(std::string_view name,const std::filesystem::path& root){
    auto storage=std::make_unique<Fixture>(root);auto& f=*storage;auto context=f.context();
    if(name=="cold_template_publication"){
        const auto before=f.pmd();const auto ordinal=f.bank.source_words().next_ordinal;
        check(context.publish_initial_template_42e7cc(),"missing actual track publication");only_outer_change(f,before,0x4864e8);
        f.draws->template_pose.position.x=71;
        const auto& pose=context.resolve_captured_outer(f.model(),f.outer().value());
        check(&pose==&f.draws->template_pose&&pose.position.x==71,"template was copied");
        check(f.bank.live_count()==1&&f.bank.source_words().next_ordinal==ordinal,"binding created source allocation");
        SceneResources empty(f.assets,root);auto draws=std::make_unique<WorldModelDrawState>();auto sprites=std::make_unique<SourceSpritePacketStorage>();
        empty.bind_sprite_4340f9(*draws,*sprites);SourceWorldTrackContext absent(empty,*draws,*sprites);
        check(!absent.publish_initial_template_42e7cc()&&empty.models().size()==0,"absent track publication invented a model");
    }else if(name=="previous_template_before_queue_store"){
        f.template_binding();const Owners owners(f);Audit audit;f.prefix(&audit);
        same_matrix(f.destination,translated(12,22,32));check(f.accepted==0,"accepted count not reset");completed_sites(audit,true);owners.unchanged(f);
        //Captured0C remains the template even if the actual word changes AFTER44FBE3.
        audit.sites.clear();audit.action=[&](auto site){if(site==0x44fbe3)f.outer().set(0x632cfc);};f.prefix(&audit);
        same_matrix(f.destination,translated(12,22,32));check(f.outer().value()==0x632cfcu,"observer mutation was rolled back");
        f.template_binding();const auto before=f.pmd();context.publish_background_track_42b072();only_outer_change(f,before,0x632cfc);
        f.prefix();same_matrix(f.destination,translated(102,202,302));
    }else if(name=="previous_background_alias"){
        context.publish_background_track_42b072();const auto word=f.outer().value();const auto before=f.pmd();
        for(std::uint32_t i=0;i<168;++i){
            const auto& bg=f.draws->background.records[i].pose;const auto& regular=f.draws->regular.records[i].pose;
            check(context.pose_reference(bg)==0x632cfcu+84u*i,"background encoding");
            check(context.pose_reference(regular)==0x62f5dcu+84u*i,"regular encoding");
            check(&context.resolve_captured_outer(f.model(),0x632cfcu+84u*i)==&bg,"background identity lost");
            check(&context.resolve_captured_outer(f.model(),0x62f5dcu+84u*i)==&regular,"regular identity lost");
        }
        f.draws->background.records[0].pose.position.x=404;
        check(context.resolve_captured_outer(f.model(),word).position.x==404,"published alias did not see mutation");
        check(f.outer().value()==word&&f.pmd()==before,"resolver changed PMD selector");
    }else if(name=="queue_reset_retention"){
        context.publish_background_track_42b072();const auto pose=raw(f.draws->background.records[0].pose);const auto before=f.pmd();
        f.draws->background.cursor=12;f.draws->regular.cursor=8;
        reset_model_draw_queues(f.draws->regular,f.draws->background);
        check(f.draws->background.records[0].model_identity==0&&f.draws->background.cursor==0u,"actual reset did not execute");
        check(raw(context.resolve_captured_outer(f.model(),f.outer().value()))==pose,"terminator/cursor invalidated retained pose");
        check(f.pmd()==before,"reset cleared descriptor0C");
    }else if(name=="actual_reseed_overwrite"){
        context.publish_background_track_42b072();const auto word=f.outer().value();const auto old=raw(f.draws->background.records[0].pose);
        reset_model_draw_queues(f.draws->regular,f.draws->background);
        f.draws->template_pose.position={77,88,99};
        append_model_draw(f.draws->background,f.scene.models(),*f.scene.model_handle(3),0,f.draws->template_pose,0,f.arena);
        const auto& pose=context.resolve_captured_outer(f.model(),word);
        check(&pose==&f.draws->background.records[0].pose&&raw(pose)==raw(f.draws->template_pose)&&raw(pose)!=old,"actual enqueue did not update retained alias");
        check(f.outer().value()==word&&f.bank.live_count()==1,"enqueue invented/cleared outer owner");
        check(pose.tail[1]>=2&&pose.tail[2]>=2&&f.arena.count>0,"existing matrix producer did not execute");
    }else if(name=="active_template_publication"){
        //Reached42AE39 store only. No fake42ADD0/rate/lighting completion.
        context.publish_background_track_42b072();const auto before=f.pmd();const auto active=f.model();
        context.publish_active_track_template_42ae39(active);only_outer_change(f,before,0x4864e8);
        reset_model_draw_queues(f.draws->regular,f.draws->background);f.draws->template_pose.position.y=808;
        check(&context.resolve_captured_outer(active,f.outer().value())==&f.draws->template_pose,"active publisher lost actual template identity");
        check(context.resolve_captured_outer(active,f.outer().value()).position.y==808,"template retained snapshot");
    }else if(name=="raw44_distinct_from48"){
        f.template_binding();auto model=f.model();const auto descriptor=model.descriptor();
        const auto first=descriptor.field<std::uint32_t>(0x44).value();const auto second=model.part(0,1).location().offset;
        check(first!=second,"raw44 witness records coincide");descriptor.field<std::uint32_t>(0x44).set(second);
        const auto old48=descriptor.field<std::uint32_t>(0x48).value(),old5c=model.current_detail_level().value();
        Audit a;a.action=[&](auto site){if(site==0x42b055)descriptor.field<std::uint32_t>(0x44).set(first);};f.prefix(&a);
        same_matrix(f.destination,translated(16,26,36));completed_sites(a,true);
        check(descriptor.field<std::uint32_t>(0x48).value()==old48&&model.current_detail_level().value()==old5c,"raw projection mutated LOD owners");
        check(model.captured_base_part_42b055(second).location().offset==second,"captured projection ignored actual offset");
    }else if(name=="part_skip_count0_and1"){
        f.template_binding();const auto d=f.model().descriptor();d.field<std::uint32_t>(0x44).set(0xffffffffu);
        d.field<std::uint32_t>(0x48).set(0xffffffffu);d.field<std::uint32_t>(0x58).set(0xffffffffu);
        for(auto count:{0u,1u}){f.count(count);const Owners owners(f);Audit a;f.prefix(&a);same_matrix(f.destination,translated(9,18,27));completed_sites(a,false);owners.unchanged(f);}
        f.count(1);Audit a;a.action=[&](auto site){if(site==0x44fbeb)f.count(2);};f.prefix(&a);
        same_matrix(f.destination,translated(9,18,27));completed_sites(a,false);
    }else if(name=="part_skip_null44"){
        f.template_binding();f.count(2);const auto d=f.model().descriptor();d.field<std::uint32_t>(0x44).set(0);
        d.field<std::uint32_t>(0x48).set(0xffffffffu);d.field<std::uint32_t>(0x58).set(0xffffffffu);
        const Owners owners(f);Audit a;f.prefix(&a);same_matrix(f.destination,translated(9,18,27));completed_sites(a,false);owners.unchanged(f);
    }else if(name=="invalid_relocated_part"){
        f.template_binding();const auto d=f.model().descriptor();const auto in_bounds=d.location().offset;
        check(f.model().bytes().subview(in_bounds,64).size()==64,"fixture is not in bounds");
        d.field<std::uint32_t>(0x44).set(in_bounds);const Owners owners(f);
        rejects<FormatError>([&]{(void)f.model().captured_base_part_42b055(in_bounds);});
        rejects<FormatError>([&]{f.prefix();});same_matrix(f.destination,f.identity);check(f.accepted==0,"reached reset lost on part rejection");owners.unchanged(f);
    }else if(name=="invalid_outer_identity"){
        f.count(1);for(auto word:{0u,0x632cfdu,0x63641cu,0xffffffffu,0x148u}){
            f.outer().set(word);const Owners owners(f);rejects<FormatError>([&]{f.prefix();});same_matrix(f.destination,f.identity);owners.unchanged(f);
        }
        ModelTransform foreign{};rejects<FormatError>([&]{(void)context.pose_reference(foreign);});
        //The one-past regular pose address is legitimately background[0].
        check(&context.resolve_captured_outer(f.model(),0x62f5dcu+168u*84u)==&f.draws->background.records[0].pose,"adjacent source arrays lost address semantics");
        f.draws->background.records[0].model_identity=0;const auto before=f.pmd();
        rejects<FormatError>([&]{context.publish_background_track_42b072();});check(f.pmd()==before,"failed publication changed PMD");
    }else if(name=="canonical_bank_expiry"){
        f.template_binding();const auto model=f.model();const auto captured=model.descriptor().field<std::uint32_t>(0x44).value();
        const auto before=raw(f.destination);const auto accepted=f.accepted;
        f.bank.release_42e980(model.allocation());
        rejects<SourceAllocationError>([&]{f.prefix();});check(raw(f.destination)==before&&f.accepted==accepted,"preflight failure wrote destination/count");
        rejects<SourceAllocationError>([&]{(void)context.resolve_captured_outer(model,0x4864e8);});
        rejects<SourceAllocationError>([&]{(void)context.publish_initial_template_42e7cc();});
        rejects<SourceAllocationError>([&]{(void)model.captured_base_part_42b055(captured);});
        check(f.bank.live_count()==0,"expired payload resurrected");
        SourcePmdModelView expired_metadata;
        {auto temporary=std::make_unique<Fixture>(root);expired_metadata=temporary->model();}
        rejects<SourceAllocationError>([&]{(void)expired_metadata.captured_base_part_42b055(captured);});
    }else if(name=="wrong_scene_binding"){
        auto other=std::make_unique<Fixture>(root);f.template_binding();const Owners owners(f);
        rejects<FormatError>([&]{(void)SourceWorldTrackContext(f.scene,*other->draws,*f.sprites);});
        rejects<FormatError>([&]{(void)SourceWorldTrackContext(f.scene,*f.draws,*other->sprites);});
        rejects<FormatError>([&]{context.publish_active_track_template_42ae39(other->model());});
        rejects<FormatError>([&]{(void)context.resolve_captured_outer(other->model(),0x4864e8);});owners.unchanged(f);
        f.scene.bind_sprite_4340f9(*other->draws,*other->sprites);
        const auto before=raw(f.destination);const auto accepted=f.accepted;
        rejects<FormatError>([&]{prepare_source_track_initial_view(context,f.camera,f.arena,f.identity,f.destination,f.accepted);});
        check(raw(f.destination)==before&&f.accepted==accepted,"rebound scene was not rejected before stores");
    }else if(name=="current_matrix_alias"){
        f.template_binding();f.count(1);f.draws->template_pose.tail[1]=2;const auto selector=f.outer().value();
        {const Owners owners(f);f.prefix();same_matrix(f.destination,translated(9,18,27));owners.unchanged(f);}
        f.arena.matrices[0][0]=2;
        {const Owners owners(f);f.prefix();auto expected=translated(9,18,27);expected[0]=2;same_matrix(f.destination,expected);owners.unchanged(f);}
        check(f.outer().value()==selector,"matrix update changed pose selector");
    }else if(name=="prefix_failure_publication"){
        struct Stop{};f.template_binding();const Owners owners(f);
        for(auto site:{0x42b04bu,0x44fbe6u,0x44fc00u,0x44fd50u}){
            f.destination=translated(991,992,993);f.accepted=99;Audit a;
            a.action=[=](auto reached){if(reached==site)throw Stop{};};
            rejects<Stop>([&]{f.prefix(&a);});check(f.accepted==0,"prefix reset rolled back");
            if(site==0x42b04b)same_matrix(f.destination,translated(991,992,993));
            else if(site==0x44fd50)same_matrix(f.destination,translated(12,22,32));
            else same_matrix(f.destination,f.identity);
            check(!a.sites.empty()&&a.sites.back()==site,"failure boundary did not stop there");owners.unchanged(f);
        }
    }else throw FormatError("unknown42B040 receiving case");
}
}
int main(int argc,char** argv)try{
    if(argc!=3)throw sgn::FormatError("usage: source_world_track_view_contracts retail-root case");
    run(argv[2],argv[1]);
    std::cout<<"{\"case\":\""<<argv[2]<<"\",\"passed\":true,\"checks\":"<<checks<<",\"original_execution\":false}\n";
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
