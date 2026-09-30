// Authored receiving cases for Main's exclusive compiler/execution slot.
// Synthetic stimuli are confined to this test; no fixture supplies runtime data.
#include "sgn/session_effect_resources.hpp"
#include "sgn/session_world_frame.hpp"
#include "sgn/retail_world_sprite_model.hpp"
#include "sgn/retail_world_view_prefix.hpp"
#include "sgn/retail_time_trial.hpp"
#include <algorithm>
#include <iostream>
#include <memory>
#include <type_traits>

namespace {
using namespace sgn;using namespace sgn::retail;using namespace contact::effects;
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
template<class F> void rejects(F&& f,const char* why){bool caught=false;try{f();}catch(const FormatError&){caught=true;}require(caught,why);}
struct Gamma final:LightGammaDevice{void apply(std::span<const std::uint16_t,768>) override{throw std::runtime_error("publisher called gamma");}};
struct Fixture {
    SourceSpritePacketStorage packets;
    SessionEffectResourceStorage resources{packets};
    EffectParticleBank particles;
    CameraState camera;CameraLifecycle lifecycle;CameraPresentationState presentation;ControllerOverlayTransition overlay;
    Fixture(){
        camera.camera_to_world_rotation={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
        presentation.forward_axis={0,0,1,0};
        presentation.planes={CameraVector{1,0,-1,0},CameraVector{-1,0,-1,0},CameraVector{0,1,-1,0},CameraVector{0,-1,-1,0}};
        lifecycle.clip_far=10000;particles.active_signal=1;
        auto& p=particles.particles[0];p.active=1;p.render_enabled=1;p.position.z=1000;
        p.scale=p.base_scale=1;p.alpha=p.base_alpha=255;p.color=p.base_color=0x12345678;
    }
    void append(){
        SessionWorldFrameResources current{};resources.publish(packets,current);
        append_effect_particle_sprites(particles,packets,{camera,lifecycle,presentation,overlay},
            current.effect_animations,current.adjacent_angles,current.effect_colors);
    }
};
static_assert(!std::is_move_constructible_v<EffectAdjacentAngleStorage>);
static_assert(!std::is_copy_constructible_v<SessionEffectResourceStorage>);
}
int main(int argc,char** argv) try {
    using namespace sgn;using namespace sgn::retail;using namespace contact::effects;
    require(argc==2,"supply NEW original retail root for read-only AssetStore");
    unsigned cases=0;
    {
        EffectAnimationStorage owner;
        require(owner.source_cursor()==0&&owner.source_flag_577658()==0&&owner.textures().empty()&&owner.frames().empty(),"PE initial owner");
        for(unsigned i=0;i<16;++i)for(auto word:owner.source_record(i))require(word==0,"BSS descriptor word");
        PhysicalMaterialStorage materials;rejects([&]{owner.recovered_entry_425a70(materials,1,0);},"inactive cursor accepted");
        ++cases;
    }
    {
        EffectAnimationStorage owner;PhysicalMaterialStorage materials;
        materials.reset_loading_42c480();owner.reset_loading_cursor_42c480();
        // Controlled backend record words, not an actual GPU upload claim.
        for(unsigned i=0;i<5;++i)materials.complete_next(1000+i,2000+i,32);
        owner.recovered_entry_425a70(materials,3,7);
        constexpr std::array<std::uint32_t,7> expected{0x62280c,1002,3,0,1,7,2};
        require(std::equal(expected.begin(),expected.end(),owner.source_record(0).begin()),"425A70 seven words");
        require(owner.source_cursor()==0x61c624&&owner.source_flag_577658()==0,"425A70 cursor/flag");
        require(owner.textures()[0].identity==0x61c608&&owner.textures()[0].first_texture==2&&owner.frames()[0].frame_count==3,"texture row confused with GPU handle");
        ++cases;
        owner.recovered_entry_425a70(materials,1,0);
        const auto retained=owner.source_record(1)[1];
        materials.reset_loading_42c480();owner.reset_loading_cursor_42c480();
        require(owner.textures().size()==2&&owner.source_record(1)[1]==retained&&owner.source_flag_577658()==1,"loading reset lost retained descriptor/flag");
        materials.complete_next(77,88,32);owner.recovered_entry_425a70(materials,1,9);
        require(owner.textures()[0].first_texture==0&&owner.frames()[0].frame_count==1&&owner.textures().size()==2,"reload overwrite/high-water projection");
        require(owner.source_record(1)[1]==retained&&owner.source_flag_577658()==1,"reload invented clearing");++cases;
        owner.clear_scene_42e510();require(owner.source_cursor()==0&&owner.textures().empty()&&owner.frames().empty()&&owner.source_flag_577658()==1,"scene clear flag retention");
        for(unsigned i=0;i<16;++i)for(auto word:owner.source_record(i))require(word==0,"scene bank not cleared");++cases;
        owner.reset_loading_cursor_42c480();for(unsigned i=0;i<16;++i)owner.recovered_entry_425a70(materials,1,9);
        const auto end=owner.source_cursor();rejects([&]{owner.recovered_entry_425a70(materials,1,0);},"descriptor overflow accepted");
        require(owner.source_cursor()==end&&owner.textures().size()==16,"overflow mutated cursor");++cases;
    }
    {
        auto f=std::make_unique<Fixture>();Gamma gamma;CameraMatrix rotation{};WorldPolygonClipAudit clipping{};
        SessionWorldFrameResources current{};current.gamma=&gamma;current.geometry_audit={};
        current.geometry_audit.track.rotation_scratch=&rotation;current.geometry_audit.clipping=&clipping;current.submission_budget={17,31,47};
        f->resources.publish(f->packets,current);auto* tags=current.effect_colors;
        require(current.effect_animations.empty()&&current.adjacent_angles.size()==1&&current.adjacent_angles[0].index==10240,"cold publication");
        require(std::bit_cast<std::uint32_t>(current.adjacent_angles[0].value)==0x0048c5d8,"source adjacent pointer bits lost");
        tags->source_undefined[6]=true;f->resources.publish(f->packets,current);
        require(current.effect_colors==tags&&tags->source_undefined[6],"publish reset diagnostic");
        require(current.gamma==&gamma&&current.geometry_audit.track.rotation_scratch==&rotation&&current.geometry_audit.clipping==&clipping&&
            current.submission_budget.retained_payload_bytes==17&&current.submission_budget.transaction_payload_bytes==31&&current.submission_budget.packet_visits==47,"publisher changed unrelated resource");
        auto foreign=std::make_unique<SourceSpritePacketStorage>();
        rejects([&]{f->resources.publish(*foreign,current);},"foreign sprite allocation accepted");
        require(current.effect_colors==tags&&tags->source_undefined[6],"foreign rejection altered publication");++cases;
    }
    {
        auto f=std::make_unique<Fixture>();auto& p=f->particles.particles[0];p.flags=4;p.angle=360;
        f->append();require(f->packets.attribute_count==4,"known adjacent angle not consumed");++cases;
        const auto before=f->packets.attribute_count;p.angle=361;
        rejects([&]{f->append();},"unknown adjacent angle accepted");
        require(f->packets.attribute_count==before,"unknown adjacent preflight wrote packets");++cases;
    }
    {
        auto f=std::make_unique<Fixture>();PhysicalMaterialStorage materials;
        materials.reset_loading_42c480();for(unsigned i=0;i<5;++i)materials.complete_next(1000+i,2000+i,32);
        f->resources.animations.reset_loading_cursor_42c480();f->resources.animations.recovered_entry_425a70(materials,3,1);
        auto& p=f->particles.particles[0];p.animation_binding=0x61c608;p.animation_frame=4096;
        f->append();require(f->packets.headers[0][1]==3,"45E4F0 did not consume descriptor row/frame");
        p.animation_frame=8192;p.lifetime_mode=1;
        update_effect_particles(f->particles,{0,0,0},f->resources.animations.frames());
        require(p.animation_frame==0&&p.active==2,"45E180 did not consume same descriptor+8");++cases;
    }
    {
        const AssetStore assets(argv[1]);ModelResources models(assets,argv[1]);
        auto f=std::make_unique<Fixture>();auto model=std::make_unique<WorldModelDrawState>();auto matrices=std::make_unique<ViewMatrixArena>();
        f->resources.colors.initialize_allocation_434090(*model);reset_view_matrix_cursor(*matrices);
        auto& audit=f->resources.colors.borrow_for(f->packets);audit.source_undefined[0]=true;audit.source_undefined[2047]=true;
        f->packets.attributes[0].color=0xdeadbeef;f->packets.attribute_count=4;f->packets.polygons=1;
        seed_world_model_draws(*model,models,std::nullopt,std::nullopt,f->packets,0,1,*matrices);
        require(f->packets.attribute_count==0&&f->packets.attributes[0].color==0xdeadbeef&&audit.source_undefined[0]&&audit.source_undefined[2047],"actual434030 lost retained bytes/tags");
        f->particles.particles[0].source_undefined[6]=false;f->append();
        require(!audit.source_undefined[0]&&audit.source_undefined[2047],"defined particle write failed range audit");
        f->resources.colors.initialize_allocation_434090(*model);
        require(std::none_of(audit.source_undefined.begin(),audit.source_undefined.end(),[](bool x){return x;}),"actual434090 failed new diagnostic lifetime");++cases;
    }
    std::cout<<"{\"receiving_cases\":"<<cases<<",\"errors\":0,\"native_device_calls\":0}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}
