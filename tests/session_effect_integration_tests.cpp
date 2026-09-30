#include "sgn/session_effect_resources.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/session_world_frame.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
template<class F> void rejects(F action,const char* message){
    bool rejected=false;try{action();}catch(const FormatError&){rejected=true;}require(rejected,message);
}
unsigned cases{};
template<class F> void check(const char* name,F action){action();++cases;std::cout<<"PASS "<<name<<'\n';}
auto words(const EffectAnimationStorage& animations){
    std::array<std::uint32_t,112> result{};
    for(std::size_t i=0;i<16;++i){const auto record=animations.source_record(static_cast<unsigned>(i));
        std::copy(record.begin(),record.end(),result.begin()+static_cast<std::ptrdiff_t>(i*7));}
    return result;
}
template<class T> auto representation(const T& value){
    const auto* p=reinterpret_cast<const unsigned char*>(&value);return std::vector<unsigned char>(p,p+sizeof(value));
}
// The only scripted boundary in this case is resource release. All reset
// instructions and both persistent source owners are actual production code.
class ReleaseObserver final:public MaterialTextureBackend {
    EffectAnimationStorage& effects_;PhysicalMaterialStorage& materials_;
    std::uint32_t effects_cursor_,materials_cursor_;
    std::uint8_t transparency_{};
public:
    unsigned calls{};bool fail{};
    ReleaseObserver(EffectAnimationStorage& e,PhysicalMaterialStorage& m)
        :effects_(e),materials_(m),effects_cursor_(e.source_cursor()),materials_cursor_(m.source_cursor()){}
    void release_all() override{
        ++calls;require(effects_.source_cursor()==effects_cursor_&&materials_.source_cursor()==materials_cursor_,
            "source cursor reset occurred before backend release");
        if(fail)throw std::runtime_error("scripted backend release failure");
    }
    std::uint8_t& current_transparency() override{return transparency_;}
    void begin_pending_record(std::uint32_t) override{throw std::runtime_error("reset began an upload");}
    CompletedMaterialUpload upload(PreparedMaterialPixels) override{throw std::runtime_error("reset uploaded pixels");}
    CompletedMaterialUpload generate(std::uint32_t) override{throw std::runtime_error("reset generated a texture");}
};
}

int main(int argc,char** argv)try{
    require(argc==2,"supply the NEW retail asset root");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    auto session=std::make_unique<RaceSession>(argv[1],launch);
    auto& s=session->state();auto& effects=s.effect_resources;
    SessionWorldFrameResources current{};current.submission_budget={17,31,47};
    std::unique_ptr<SessionWorldMaterials> materials;
    check("actual-session-cold-initializer-and-packet-owner",[&]{
        publish_session_effect_resources(*session,current);
        require(current.effect_animations.empty()&&borrow_session_effect_animation_frames(*session).empty(),"cold descriptors fabricated");
        require(current.effect_colors==&effects.colors.borrow_for(s.sprite_packets),"different sprite audit owner");
        require(std::none_of(current.effect_colors->source_undefined.begin(),current.effect_colors->source_undefined.end(),[](bool x){return x;}),
            "actual434090 did not initialize diagnostic tags");
        // Nonzero source434183..434191 witness: zero construction cannot pass.
        for(std::size_t i=0;i<s.sprite_packets.indices.size();++i)
            require(s.sprite_packets.indices[i]==static_cast<std::uint16_t>(i),"actual434090 did not initialize sprite indices");
        require(current.adjacent_angles.size()==1&&current.adjacent_angles[0].index==10240&&
            std::bit_cast<std::uint32_t>(current.adjacent_angles[0].value)==0x0048c5d8,"wrong adjacent source word");
        require(current.submission_budget.retained_payload_bytes==17&&current.gamma==nullptr,"publisher changed unrelated resources");
    });
    check("material-wrapper-binds-session-owner-without-clearing-it",[&]{
        // Explicit receiving stimulus; no runtime producer call is inferred.
        PhysicalMaterialStorage physical;physical.reset_loading_42c480();
        physical.complete_next(10,20,32);physical.complete_next(11,21,32);
        effects.animations.reset_loading_cursor_42c480();effects.animations.recovered_entry_425a70(physical,2,0);
        const auto before=words(effects.animations);const auto cursor=effects.animations.source_cursor();
        materials=std::make_unique<SessionWorldMaterials>(*session);
        require(materials->state().effect_animation_storage==&effects.animations,"wrapper uses another effect bank");
        require(words(effects.animations)==before&&effects.animations.source_cursor()==cursor,"material wrapper synthesized a scene reset");
    });
    check("both-session-adapters-reject-foreign-thread-before-publication",[&]{
        const auto before=representation(current);const auto source_before=words(effects.animations);
        std::array<std::string,2> failures;
        std::thread worker([&]{
            try{publish_session_effect_resources(*session,current);}catch(const std::logic_error& e){failures[0]=e.what();}
            catch(...){failures[0]="wrong exception";}
            try{(void)borrow_session_effect_animation_frames(*session);}catch(const std::logic_error& e){failures[1]=e.what();}
            catch(...){failures[1]="wrong exception";}
        });worker.join();
        for(const auto& failure:failures)require(failure=="race world and platform belong to the creating game thread","missing owner-thread guard");
        require(representation(current)==before&&words(effects.animations)==source_before,"rejected thread mutated publication/source");
    });
    check("fresh-session-borrows-follow-descriptor-writes-and-clear",[&]{
        const auto first=borrow_session_effect_animation_frames(*session);require(first.size()==1,"first count");
        PhysicalMaterialStorage physical;physical.reset_loading_42c480();
        physical.complete_next(10,20,32);physical.complete_next(11,21,32);physical.complete_next(12,22,32);
        effects.animations.recovered_entry_425a70(physical,3,7);
        const auto next=borrow_session_effect_animation_frames(*session);
        require(first.size()==1&&next.size()==2&&next.data()==first.data()&&next[1].frame_count==3,"frame count cached or bank replaced");
        publish_session_effect_resources(*session,current);
        require(current.effect_animations.size()==2&&current.effect_animations.data()==effects.animations.textures().data(),"texture projection stale");
        auto& p=s.effect_particles.particles[0];s.effect_particles.active_signal=1;
        p.active=1;p.animation_binding=0x61c608;p.animation_frame=4096;p.lifetime_mode=1;
        contact::effects::update_effect_particles(s.effect_particles,{0,0,0},borrow_session_effect_animation_frames(*session));
        require(p.animation_frame==0&&p.active==2,"real lifetime child missed Session descriptor count");
        effects.animations.clear_scene_42e510();publish_session_effect_resources(*session,current);
        require(borrow_session_effect_animation_frames(*session).empty()&&current.effect_animations.empty()&&
            effects.animations.source_flag_577658()==1,"fresh clear view or retained source flag wrong");
    });
    check("actual-material-reset-retains-descriptors-and-stops-on-release-failure",[&]{
        auto loading=std::make_unique<WorldMaterialLoadingState>();loading->effect_animation_storage=&effects.animations;
        auto& physical=loading->names.materials;physical.reset_loading_42c480();
        physical.complete_next(31,32,32);physical.complete_next(41,42,32);
        effects.animations.reset_loading_cursor_42c480();effects.animations.recovered_entry_425a70(physical,2,0);
        const auto before=words(effects.animations);const auto cursor=effects.animations.source_cursor();
        ReleaseObserver backend(effects.animations,physical);backend.fail=true;
        bool failed=false;try{reset_world_material_loading(*loading,backend);}
        catch(const std::runtime_error& e){failed=std::string(e.what())=="scripted backend release failure";}
        require(failed&&words(effects.animations)==before&&effects.animations.source_cursor()==cursor&&loading->cache_initialized==0,
            "failed release changed source reset state");
        backend.fail=false;reset_world_material_loading(*loading,backend);
        require(backend.calls==2&&physical.source_cursor()==0x6227d4&&effects.animations.source_cursor()==0x61c608,
            "bound source reset did not reset both cursors");
        require(words(effects.animations)==before&&effects.animations.source_flag_577658()==1&&
            borrow_session_effect_animation_frames(*session).size()==1,"loading reset erased retained descriptor data");
    });
    check("frontend-retains-effects-and-stale-material-wrapper-refuses-access",[&]{
        auto platform=session->shared_platform();auto alias=session->shared_state();
        auto frontend=std::make_unique<FrontendState>(alias,platform);std::weak_ptr<RaceSessionPlatform> weak=platform;
        auto* address=&effects;effects.colors.borrow_for(s.sprite_packets).source_undefined[17]=true;
        const auto before=words(effects.animations);session.reset();alias.reset();platform.reset();
        require(!weak.expired() && &frontend->world().effect_resources==address,"Session release replaced effect owner");
        auto& retained=frontend->world();
        require(words(retained.effect_resources.animations)==before&&
            retained.effect_resources.colors.borrow_for(retained.sprite_packets).source_undefined[17],"Session release reset effects");
        rejects([&]{(void)materials->state();},"material wrapper accepted detached level");
        materials.reset();frontend.reset();require(weak.expired(),"effect lifetime contains a retained cycle");
        // No old world/resource reference is accessed after final release.
    });
    std::cout<<"session_effect_integration_cases="<<cases<<" GPU_runs=0 original_execution_cases=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
