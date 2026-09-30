#include "sgn/clock.hpp"
#include "sgn/playable_driver.hpp"
#include "sgn/playable_camera.hpp"
#include "sgn/playable_scene.hpp"
#include "sgn/presentation_interpolation.hpp"
#include "sgn/race_overlay_textures.hpp"
#include "sgn/race_session_music.hpp"
#include "vehicle_oracle_state.hpp"
#include "retail_race_state.hpp"
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
using namespace sgn;using namespace sgn::retail;
void require(bool value,const char* message){if(!value)throw FormatError(message);}
struct Hash {
    std::uint64_t value=14695981039346656037ull;
    void bytes(const void* data,std::size_t size){const auto* p=static_cast<const unsigned char*>(data);for(std::size_t i=0;i<size;++i){value^=p[i];value*=1099511628211ull;}}
    template<class T>void add(const T& v){static_assert(std::is_trivially_copyable_v<T>);bytes(&v,sizeof(v));}
    std::string text()const{std::ostringstream out;out<<std::hex<<std::setfill('0')<<std::setw(16)<<value;return out.str();}
};
void pose_hash(Hash& h,const Pose& p){h.add(p.right);h.add(p.up);h.add(p.forward);h.add(p.position);h.add(p.scale);}
void view_hash(Hash& h,const View& v){
    h.add(v.eye);h.add(v.target);h.add(v.up);h.add(v.near_plane);h.add(v.far_plane);
    h.add(v.world_to_view.has_value());if(v.world_to_view)h.add(*v.world_to_view);
    h.add(v.source_projection.has_value());if(v.source_projection)h.add(*v.source_projection);
    h.add(v.retail_viewport.has_value());if(v.retail_viewport){h.add(v.retail_viewport->projection);h.add(v.retail_viewport->clip_far);h.add(v.retail_viewport->widen_horizontal);}
}
void objects_hash(Hash& h,std::span<const PresentationObject> objects,bool geometry){
    h.add(objects.size());for(const auto& o:objects){
        h.add(o.identity);h.add(o.continuity);h.add(o.mesh);pose_hash(h,o.pose);h.add(o.parts.size());
        for(const auto& part:o.parts){pose_hash(h,part.pose);h.add(part.visible);}
        for(const auto& m:o.model_to_view)h.add(m);
        if(geometry){require(o.geometry!=nullptr,"snapshot geometry owner absent");const auto& g=*o.geometry;
            h.add(g.model.minimum);h.add(g.model.maximum);h.add(g.model.kind);h.add(g.model.parts);h.add(g.model.lod_count);
            h.add(g.model.vertices.size());h.bytes(g.model.vertices.data(),g.model.vertices.size()*sizeof(Vertex));
            h.add(g.model.surfaces.size());for(const auto& s:g.model.surfaces){h.add(s.first);h.add(s.count);h.add(s.texture);h.add(s.translucent);h.add(s.two_sided);h.add(s.part);}
            for(const auto& name:g.model.textures){h.add(name.size());h.bytes(name.data(),name.size());}
            for(const auto& m:g.materials)h.add(m.blend);
        }
    }
}
Hash snapshot_hash(const PresentationSnapshot& s,bool geometry=false){Hash h;h.add(s.tick);h.add(s.camera_continuity);view_hash(h,s.view);objects_hash(h,s.objects,geometry);return h;}
Hash draw_hash(const PresentationDraw& d){Hash h;view_hash(h,d.view);objects_hash(h,d.objects,false);return h;}
std::optional<std::array<float,2>> screen_origin(const View& view,const std::array<float,16>& matrix){
    if(!view.retail_viewport)return std::nullopt;auto p=view.retail_viewport->projection;
    // Diagnostic body-origin projection for this test's fixed1280x720 launch;
    // matches the renderer's documented widescreen scaling, without a GPU.
    if(view.retail_viewport->widen_horizontal)p.x*=0.75f;
    const auto point=project_retail_vertex({},matrix,p);if(point.camera_z<=p.clip_near)return std::nullopt;
    const auto screen=retail_viewport(point,1280,720);
    if(!std::isfinite(screen.x)||!std::isfinite(screen.y)||screen.x<0||screen.x>1280||screen.y<0||screen.y>720)return std::nullopt;
    return std::array{screen.x,screen.y};
}
Hash model_hash(RaceSession& session){
    Hash h;auto& models=session.models();h.add(models.size());
    for(std::size_t index=0;index<models.size();++index){auto& m=models.at(index);
        h.add(m.previous_detail_level());h.add(m.source_model_flags_02());h.add(m.source_model_ambience());h.add(m.source_model_vector_10());
        for(unsigned lod=0;lod<m.lod_count();++lod)for(unsigned i=0;i<m.part_count(lod);++i){
            const auto p=m.part(lod,i);h.add(p.flags);h.add(p.transform);h.add(p.pivot);h.add(m.source_part_flags_02(lod,i));
            const auto a=m.source_attributes(lod,i);const auto c=m.source_coordinates(lod,i);h.bytes(a.data(),a.size_bytes());h.bytes(c.data(),c.size_bytes());
            const auto v=m.source_vertices(lod,i);h.bytes(v.data(),v.size_bytes());
        }
    }
    return h;
}
Hash source_trace(RaceSessionState& s){
    Hash h;h.add(s.simulation_tick);h.add(s.random.state);h.add(s.clock);h.add(s.current_slot);h.add(s.selected_lod);
    // Existing output-only source field serializers avoid native padding and
    // pointer identities. Never import a fixture into the live Session.
    for(unsigned i=0;i<6;++i){std::array<std::uint8_t,0x7800> bytes{};
        oracle::transfer(bytes,s.vehicles.parameters[i],s.vehicles.physics[i],false);
        race_oracle::transfer(bytes,s.vehicles.race[i],false);h.bytes(bytes.data(),bytes.size());h.add(s.vehicles.race[i].place);
    }
    h.add(s.controls.current);for(const auto& c:s.controls.controllers){
        const auto& v=c.camera;h.add(c.active);h.add(c.vehicle);h.add(v.mode);h.add(v.position);h.add(v.target);h.add(v.previous_target);
        h.add(v.up);h.add(v.side);h.add(v.velocity);h.add(v.drift);h.add(v.stabilized_up);h.add(v.twist);h.add(v.lens);
        h.add(v.projection);h.add(v.camera_to_world_rotation);h.add(v.world_to_camera_rotation);
        h.add(v.camera_to_world_reference);h.add(v.world_to_camera_reference);h.add(v.control_angles);h.add(v.angular_velocity);h.add(v.angular_acceleration);
    }
    h.add(s.view_matrices.matrices);h.add(s.view_matrices.count);h.add(s.view_matrices.cursor.has_value());if(s.view_matrices.cursor)h.add(*s.view_matrices.cursor);
    return h;
}
class Images final:public TextureReplacementDevice {
    Image image_;
public:
    explicit Images(const AssetStore& a):image_(a.texture("Overlays\\Start0")){}
    std::uint16_t destination_width(std::uint32_t slot)const override{require(slot==15&&image_.width<=65535,"overlay destination");return static_cast<std::uint16_t>(image_.width);}
    void upload_to_destination(std::uint32_t slot,const Image& image)override{require(slot==15,"overlay upload");image_=image;}
};
void pump(){MSG message;while(PeekMessageW(&message,nullptr,0,0,PM_REMOVE)){TranslateMessage(&message);DispatchMessageW(&message);}}
struct Statistics {std::uint64_t frames{},samples{},changed_intermediate{},subpixel_motion_samples{},state_checks{},deep_checks{},retained_checks{},lod_changes{},reset_inputs{},reset_epoch_checks{},camera_reset_cuts{},following_camera_cuts{},post_reset_blend_checks{},pause_samples{};
    double maximum_screen_delta{},maximum_screen_alpha{};std::uint64_t maximum_screen_tick{},maximum_screen_object{};};
}
int main(int argc,char** argv){try{
    if(argc!=7)throw FormatError("expected asset-root mode ticks hz numerator-denominator output-json (hz=0 selects variable cadence)");
    const std::filesystem::path root=argv[1],output=argv[6];const std::string mode=argv[2];
    const auto ticks=std::stoull(argv[3]),hz=std::stoull(argv[4]),denominator=std::stoull(argv[5]);
    require(ticks>=2&&ticks<=100000&&hz<=1'000'000&&denominator>=1&&denominator<=1001,"invalid cadence arguments");
    require(hz==0||hz>=30*denominator,"cadence below supported test range");require(!std::filesystem::exists(output),"preserve existing cadence evidence");
    RaceSessionLaunch launch;launch.mode=mode=="normal"?SessionMode::normal_race:mode=="trial"?SessionMode::time_trial:SessionMode::challenge;
    require(mode=="normal"||mode=="trial"||mode=="challenge","unknown test mode");launch.level_name=mode=="challenge"?"CHALL1":"TRACK1";launch.master_gain=0;
    RaceSession session(root,launch);auto& s=session.state();Images images(session.assets());RaceOverlayTextures textures(session.assets(),images,session.shared_platform());
    unsigned player_slot=6;for(const auto& controller:s.controls.controllers)if(controller.active==1){player_slot=controller.vehicle;break;}
    require(player_slot<6,"actual Session has no player");
    PlayableDriver driver(session,textures,{});PlayableScene scene(session);PresentationInterpolator interpolation;TickClock clock;Statistics count;
    Bytes before(sizeof(RaceSessionState));std::vector<std::array<std::string,2>> trace;trace.reserve(static_cast<std::size_t>(ticks));
    ImmutablePresentationSnapshot retained;std::uint64_t retained_hash{};std::array<unsigned,6> previous_lod=s.selected_lod;
    std::int64_t time{};bool paused_fixture{};std::uint64_t last_draw_hash{};
    const auto preserve_state=[&]{std::memcpy(before.data(),&s,sizeof(s));};
    const auto check_state=[&]{require(std::memcmp(before.data(),&s,sizeof(s))==0,"presentation mutated inline Session owners");++count.state_checks;};
    const auto sample=[&](double alpha){
        const auto& draw=interpolation.sample(alpha);require(!draw.objects.empty(),"presentation returned empty actual scene");
        for(const auto& o:draw.objects)for(const auto& m:o.model_to_view)for(float value:m)require(std::isfinite(value),"nonfinite actual interpolated matrix");
        const auto digest=draw_hash(draw).value;
        // Compare equivalent render values (without snapshot metadata).
        Hash endpoint;view_hash(endpoint,scene.snapshot()->view);objects_hash(endpoint,scene.snapshot()->objects,false);
        if(alpha>0&&alpha<1&&digest!=endpoint.value&&digest!=last_draw_hash)++count.changed_intermediate;
        if(alpha>0&&alpha<1){bool subpixel=false;const auto& current=*scene.snapshot();
            for(std::size_t i=0;i<draw.objects.size();++i){const auto& object=draw.objects[i];const auto& latest=current.objects[i];
                if((object.identity&0xf000000000000001ull)!=0x2000000000000000ull||object.parts.empty()||!object.parts.front().visible)continue;
                const auto a=screen_origin(draw.view,object.model_to_view.front()),b=screen_origin(current.view,latest.model_to_view.front());
                if(a&&b){const auto delta=std::hypot(double((*a)[0])-(*b)[0],double((*a)[1])-(*b)[1]);
                    if(delta>count.maximum_screen_delta){count.maximum_screen_delta=delta;count.maximum_screen_alpha=alpha;
                        count.maximum_screen_tick=current.tick;count.maximum_screen_object=object.identity;}
                    subpixel|=delta>=0.1;}
            }
            count.subpixel_motion_samples+=subpixel;
        }
        last_draw_hash=digest;++count.samples;
    };
    while(clock.ticks()<ticks){
        ++count.frames;const std::int64_t now=hz?static_cast<std::int64_t>(count.frames*1'000'000'000ull*denominator/hz):
            time+std::array<std::int64_t,8>{2'777'778,6'060'606,13'333'333,33'333'333,4'166'667,8'333'333,6'944'444,16'683'333}[(count.frames-1)%8];
        clock.advance(std::chrono::nanoseconds(now-time),[&](std::uint64_t tick)->bool{
            if(tick>=ticks)return false;pump();PlayableInput input;input.throttle=tick<1140;input.brake=tick>=1140;
            input.left=tick>=900&&tick<930;input.right=tick>=960&&tick<990;input.turbo=tick>=840&&tick<900;input.reset=tick==1020;count.reset_inputs+=input.reset;
            const auto old_epoch=s.presentation_vehicle_continuity[player_slot];const auto previous=scene.snapshot();
            if(input.reset)require(s.clock.phase==2&&s.vehicles.race[player_slot].control_enabled!=0&&s.vehicles.race[player_slot].stopped==0,
                "reset fixture did not reach a controllable live player");
            playable_camera_begin_tick(session);driver.tick(input);maintain_session_audio(session);scene.update_after_tick(session);
            const auto current=scene.snapshot();require(current&&current->tick==s.simulation_tick,"snapshot source tick mismatch");
            if(retained){require(snapshot_hash(*retained,true).value==retained_hash,"new source ticks mutated retained snapshot geometry");++count.retained_checks;retained.reset();}
            if(tick%60==0){retained=current;retained_hash=snapshot_hash(*retained,true).value;}
            preserve_state();interpolation.publish(current);check_state();
            if(input.reset){
                require(s.presentation_vehicle_continuity[player_slot]>old_epoch,"actual source reset did not advance presentation epoch");
                const auto identity=0x2000000000000000ull|std::uint64_t(player_slot)*2u;
                const auto object=std::find_if(current->objects.begin(),current->objects.end(),[&](const auto& o){return o.identity==identity;});
                require(object!=current->objects.end()&&object->continuity==s.presentation_vehicle_continuity[player_slot],
                    "reset epoch did not reach actual player PresentationObject");++count.reset_epoch_checks;
            }
            // Actual source405210 runs before the placement in418A90. The
            // reset snapshot T still has the old camera; T+1 first observes
            // the reset. Both must snap, and T+2 must resume interpolation.
            if(tick>=1020&&tick<=1022){
                require(previous&&s.controls.controllers.at(s.controls.current).vehicle==player_slot,
                    "reset sequence lost its active player camera");
                Hash endpoint;view_hash(endpoint,current->view);objects_hash(endpoint,current->objects,false);
                preserve_state();
                if(tick<=1021){
                    require(current->camera_continuity>previous->camera_continuity,
                        "reset or following source camera snapshot did not cut history");
                    if(tick==1020)++count.camera_reset_cuts;else ++count.following_camera_cuts;
                    for(double alpha:{0.0,0.5,1.0}){
                        const auto& draw=interpolation.sample(alpha);++count.samples;
                        require(draw_hash(draw).value==endpoint.value,
                            "real reset sequence blended pre-placement camera or object transforms");
                    }
                }else{
                    require(current->camera_continuity==previous->camera_continuity,
                        "camera history did not resume after observing the reset");
                    const auto intermediate_before=interpolation.stats().intermediate_samples;
                    const auto& draw=interpolation.sample(0.5);++count.samples;
                    require(interpolation.stats().intermediate_samples==intermediate_before+1&&draw_hash(draw).value!=endpoint.value,
                        "actual post-reset frame did not resume intermediate presentation");++count.post_reset_blend_checks;
                }
                check_state();
            }
            for(unsigned i=0;i<6;++i)count.lod_changes+=s.selected_lod[i]!=previous_lod[i];previous_lod=s.selected_lod;
            trace.push_back({source_trace(s).text(),snapshot_hash(*current).text()});return true;
        });time=now;
        if(!interpolation.current())continue;
        const bool deep=clock.ticks()%60==0;const auto model_before=deep?model_hash(session).value:0;
        preserve_state();sample(clock.alpha());
        if(clock.ticks()>=1000&&!paused_fixture){
            interpolation.invalidate_history();for(unsigned i=0;i<24;++i){sample(1);++count.pause_samples;}
            // Host pause presents the completed state and resumes with fresh
            // history. No source tick or input fixture is inserted here.
            paused_fixture=true;
        }
        check_state();if(deep){require(model_hash(session).value==model_before,"presentation mutated source model storage");++count.deep_checks;}
    }
    require(clock.ticks()==ticks&&trace.size()==ticks,"display cadence changed accepted source tick count");
    const auto& stats=interpolation.stats();require(stats.publications==ticks,"one publication per tick violated");
    if(ticks>1020)require(count.reset_epoch_checks==1&&count.camera_reset_cuts==1,"actual reset discontinuity was not exercised");
    if(ticks>1021)require(count.following_camera_cuts==1,"source camera reset lag was not exercised");
    if(ticks>1022)require(count.post_reset_blend_checks==1,"post-reset interpolation was not exercised");
    if(hz==0||hz>60*denominator)require(count.changed_intermediate>0,"high refresh duplicated only completed source frames");
    if(ticks>=1200&&(hz==0||hz>60*denominator))require(count.subpixel_motion_samples>0,"high refresh changes were only insignificant floating-point noise");
    std::ofstream out(output);require(bool(out),"could not open cadence report");
    out<<"{\"passed\":true,\"mode\":\""<<mode<<"\",\"ticks\":"<<ticks<<",\"hz_numerator\":"<<hz<<",\"hz_denominator\":"<<denominator
        <<",\"frames\":"<<count.frames<<",\"samples\":"<<count.samples<<",\"changed_intermediate\":"<<count.changed_intermediate
        <<",\"body_origin_motion_at_least_0_1px_samples\":"<<count.subpixel_motion_samples<<",\"maximum_body_origin_screen_delta_px\":"<<count.maximum_screen_delta
        <<",\"maximum_screen_delta_tick\":"<<count.maximum_screen_tick<<",\"maximum_screen_delta_object\":"<<count.maximum_screen_object
        <<",\"maximum_screen_delta_alpha\":"<<count.maximum_screen_alpha
        <<",\"state_checks\":"<<count.state_checks<<",\"deep_model_checks\":"<<count.deep_checks<<",\"retained_snapshot_checks\":"<<count.retained_checks
        <<",\"lod_changes\":"<<count.lod_changes<<",\"reset_inputs\":"<<count.reset_inputs<<",\"reset_epoch_checks\":"<<count.reset_epoch_checks
        <<",\"camera_reset_cuts\":"<<count.camera_reset_cuts<<",\"following_camera_cuts\":"<<count.following_camera_cuts
        <<",\"post_reset_blend_checks\":"<<count.post_reset_blend_checks<<",\"pause_samples\":"<<count.pause_samples
        <<",\"camera_cuts\":"<<stats.camera_cuts<<",\"object_cuts\":"<<stats.object_cuts<<",\"interpolated_parts\":"<<stats.interpolated_parts
        <<",\"gpu_used\":false,\"dxgi_present_executed\":false,\"trace\":[";
    for(std::size_t i=0;i<trace.size();++i){if(i)out<<',';out<<"[\""<<trace[i][0]<<"\",\""<<trace[i][1]<<"\"]";}out<<"]}\n";out.close();require(bool(out),"cadence report write failed");
    std::cout<<"PASS "<<mode<<" ticks="<<ticks<<" frames="<<count.frames<<" intermediate="<<count.changed_intermediate<<'\n';return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
