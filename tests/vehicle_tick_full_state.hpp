#pragma once
#include "sgn/race_session_vehicle_tick.hpp"
#include "vehicle_tick_vehicle_state.hpp"
#include "vehicle_tick_model_state.hpp"
#include <map>
#include <memory>

namespace sgn::retail::whole_tick_test {
constexpr unsigned global_base=0x470000,global_size=0x1c8000,car_base=0x49a1f8,car_stride=0x7724,controller_base=0x498248,replay_base=0x4d5cc0;
inline std::uint32_t get(std::span<const std::uint8_t> b,std::size_t at){if(at>b.size()||b.size()-at<4)throw FormatError("whole tick read bounds");std::uint32_t v;std::memcpy(&v,b.data()+at,4);return v;}
inline void put(std::span<std::uint8_t> b,std::size_t at,std::uint32_t value){if(at>b.size()||b.size()-at<4)throw FormatError("whole tick write bounds");std::memcpy(b.data()+at,&value,4);}
struct Region {unsigned kind{},index{},address{},length{},descriptor{},offset{};std::string name;};
struct StateCodec {
    RaceSessionState& state;
    ModelResources& models;
    const SplineSet& splines;
    std::span<const std::uint8_t> track;
    const std::vector<Region>& regions;
    const unsigned track_base;
    unsigned polygon_base{},polygon_count{};
    unsigned vertex_base{},vertex_count{},normal_base{},normal_count{};
    std::map<unsigned,std::uint32_t> model_tokens;
    std::map<std::uint32_t,unsigned> model_addresses;
    std::map<unsigned,SplineView> spline_views;
    std::map<unsigned,unsigned> spline_addresses;
    StateCodec(RaceSessionState& s,ModelResources& m,const SplineSet& routes,std::span<const std::uint8_t> pmd,const std::vector<Region>& r,
               unsigned source_track_base=0x11400000)
        :state(s),models(m),splines(routes),track(pmd),regions(r),track_base(source_track_base){
        if(std::uint64_t(track_base)+track.size()>0x100000000ull)throw FormatError("integration source track allocation overflow");
        const auto descriptor=0x148+get(track,0xa0);
        const auto offset=std::uint64_t{0x148}+get(track,0x98)+std::uint64_t(get(track,descriptor+16))*8;
        const auto geometry=contact::decode_track_collision(track);
        if(offset>track.size()||geometry.polygons.size()>(track.size()-offset)/8)throw FormatError("integration polygon table bounds");
        polygon_base=track_base+static_cast<unsigned>(offset);polygon_count=static_cast<unsigned>(geometry.polygons.size());
        const auto vector_table=[&](unsigned header,unsigned index,std::size_t count){
            const auto at=std::uint64_t{0x148}+get(track,header)+std::uint64_t(index)*16;
            if(at>track.size()||count>(track.size()-at)/16)throw FormatError("integration collision vector table bounds");
            return track_base+static_cast<unsigned>(at);
        };
        vertex_base=vector_table(0x2c,get(track,descriptor+8),geometry.vertices.size());vertex_count=static_cast<unsigned>(geometry.vertices.size());
        normal_base=vector_table(0x20,get(track,descriptor+12),geometry.normals.size());normal_count=static_cast<unsigned>(geometry.normals.size());
        for(const auto& region:regions)if(region.kind==1){model_tokens.emplace(region.descriptor,region.index+1);model_addresses.emplace(region.index+1,region.descriptor);}
        const auto base=0x148+get(track,0x78);
        for(unsigned j=0;j<get(track,0x10c)/28;++j){const auto id=get(track,base+j*28);auto route=splines.find(id);
            if(!route)throw FormatError("whole tick missing authored spline");spline_views.emplace(track_base+base+j*28,*route);spline_addresses.emplace(id,track_base+base+j*28);}
    }
    static unsigned vehicle_id(unsigned address){if(address<car_base || address>=car_base+6*car_stride || (address-car_base)%car_stride)throw FormatError("whole tick car pointer");return (address-car_base)/car_stride;}
    static unsigned vehicle_address(unsigned id){if(id>=6)throw FormatError("whole tick car ID");return car_base+id*car_stride;}
    unsigned model_id(unsigned address) const {if(!address)return 0;const auto found=model_tokens.find(address);if(found==model_tokens.end())throw FormatError("whole tick model pointer");return found->second;}
    unsigned model_address(unsigned id) const {if(!id)return 0;return model_addresses.at(id);}
    std::optional<SplineView> spline(unsigned address) const {if(!address)return {};const auto found=spline_views.find(address);if(found==spline_views.end())throw FormatError("whole tick spline pointer");return found->second;}
    unsigned spline_address(const std::optional<SplineView>& route) const {return route?spline_addresses.at(route->descriptor.id):0;}
    unsigned polygon_address(std::optional<unsigned> id)const{
        if(!id)return 0;if(*id>=polygon_count)throw FormatError("integration polygon index");return polygon_base+*id*8;
    }
    std::optional<unsigned> polygon(unsigned address)const{
        if(!address)return {};if(address<polygon_base||(address-polygon_base)%8||(address-polygon_base)/8>=polygon_count)
            throw FormatError("integration polygon pointer");return (address-polygon_base)/8;
    }
    static std::optional<unsigned> vector_id(unsigned address,unsigned base,unsigned count){
        if(!address)return {};if(address<base||(address-base)%16||(address-base)/16>=count)throw FormatError("integration collision vector pointer");
        return (address-base)/16;
    }
    static unsigned vector_address(std::optional<unsigned> id,unsigned base,unsigned count){
        if(!id)return 0;if(*id>=count)throw FormatError("integration collision vector index");return base+*id*16;
    }
    void transfer_lighting(Bytes& b,bool loading){
        auto& s=state;const Region* region=nullptr;
        for(const auto& item:regions)if(item.address==track_base&&item.length==track.size()){
            if(region)throw FormatError("integration duplicate track allocation");region=&item;}
        if(!region){
            if(loading&&(get(b,0x589e04-global_base)||s.light_selection.active_count))throw FormatError("integration active light requires captured track PMD");
            if(!loading&&(!s.lights.records.empty()||s.lights.selected))throw FormatError("integration uncaptured native lights");
            return;
        }
        const auto authored=decode_pmd_lights(track);const auto count=static_cast<unsigned>(authored.records.size());
        const auto local=0x148+get(track,0x70),base=track_base+local;
        if(loading){if(get(b,0x62449c-global_base)!=count||get(b,0x62447c-global_base)!=base)
                throw FormatError("integration light allocation metadata");s.lights=authored;}
        else{if(s.lights.records.size()!=count)throw FormatError("integration authored light count");
            put(b,0x62449c-global_base,count);put(b,0x62447c-global_base,base);}
        const auto block=[&](auto& value,unsigned at){
            if(at>b.size()||sizeof(value)>b.size()-at)throw FormatError("integration light field bounds");
            if(loading)std::memcpy(&value,b.data()+at,sizeof(value));else std::memcpy(b.data()+at,&value,sizeof(value));};
        for(unsigned j=0;j<count;++j)block(s.lights.records[j],region->offset+local+j*0x90);
        block(s.lights.authored_ambient,region->offset+0x148+get(track,0x64));
        block(s.lights.render_count,0x6244a0-global_base);
        if(s.lights.render_count>count||s.light_selection.active_count>16)throw FormatError("integration light count bounds");
        const auto light=[&](std::optional<unsigned>& value,unsigned address){const auto at=address-global_base;
            if(loading){const auto pointer=get(b,at);if(!pointer){value.reset();return;}
                if(pointer<base||(pointer-base)%0x90||(pointer-base)/0x90>=count)throw FormatError("integration light pointer");value=(pointer-base)/0x90;}
            else{if(value&&*value>=count)throw FormatError("integration light index");put(b,at,value?base+*value*0x90:0);}};
        light(s.lights.selected,0x589e04);
        if(loading&&s.light_selection.ordered.size()<16)s.light_selection.ordered.resize(16);
        for(unsigned j=0;j<16;++j){std::optional<unsigned> empty;
            auto& value=j<s.light_selection.ordered.size()?s.light_selection.ordered[j]:empty;light(value,0x624534+j*4);}
        block(s.ambient_light,0x624504-global_base);
        block(s.light_fade.target,0x477b80-global_base);block(s.light_fade.current,0x477b84-global_base);
        block(s.light_fade.blend,0x589e00-global_base);block(s.light_fade.last_gamma,0x6244f4-global_base);
        block(s.light_fade.ramp,0x592180-global_base);
    }
    const Region& strip(unsigned index) const {for(const auto& r:regions)if(r.kind==2&&r.index==index)return r;throw FormatError("whole tick strip region");}
    static std::optional<ReplayHandle> replay(unsigned address){if(!address)return {};if(address<replay_base||address>=replay_base+ReplayBank::size||(address-replay_base)%ReplayBank::stride)throw FormatError("whole tick replay pointer");return ReplayHandle{(address-replay_base)/ReplayBank::stride};}
    static unsigned replay_address(std::optional<ReplayHandle> value){return value?replay_base+ReplayBank::base(*value):0;}
    void transfer(Bytes& b,bool loading){
        auto& s=state;
        const auto field=[&](auto& value,unsigned address){static_assert(std::is_trivially_copyable_v<std::remove_reference_t<decltype(value)>>);
            if(address<global_base || address-global_base>b.size() || sizeof(value)>b.size()-(address-global_base))throw FormatError("whole tick field bounds");
            if(loading)std::memcpy(&value,b.data()+address-global_base,sizeof(value));else std::memcpy(b.data()+address-global_base,&value,sizeof(value));};
        const auto scalar=[&](auto& value,unsigned address){static_assert(sizeof(value)==4);field(value,address);};
        const auto u=[&](unsigned address){return get(b,address-global_base);};
        const auto store=[&](unsigned address,unsigned value){put(b,address-global_base,value);};
        const auto car=[&](unsigned& value,unsigned address){if(loading)value=vehicle_id(u(address));else store(address,vehicle_address(value));};
        const auto optional_car=[&](std::optional<unsigned>& value,unsigned address){if(loading){const auto p=u(address);value=p?std::optional(vehicle_id(p)):std::nullopt;}else store(address,value?vehicle_address(*value):0);};
        const auto spline_field=[&](std::optional<SplineView>& value,unsigned address){if(loading)value=spline(u(address));else store(address,spline_address(value));};
        for(unsigned j=0;j<6;++j){Bytes v(0x7800);const auto at=car_base-global_base+j*car_stride;std::copy_n(b.begin()+at,car_stride,v.begin());
            if(get(v,0x726c))throw FormatError("whole tick non-null external contact object outside fixture domain");
            if(loading){for(unsigned off:{0x26b0u,0x26b4u})put(v,off,model_id(get(v,off)));auto recording=replay(get(v,0x7268));put(v,0x7268,recording?recording->slot+1:0);
                for(unsigned k=0;k<4;++k){auto off=0x3964+k*0xd10+12,value=get(v,off);if(value){const auto base=strip(3).address;if(value<base||value>=base+8192||(value-base)%16)throw FormatError("whole tick corner pointer");value=(value-base)/16+1;}put(v,off,value);}}
            tick_test::transfer_vehicle(v,s.vehicles,j,loading,models);
            if(!loading){for(unsigned off:{0x26b0u,0x26b4u})put(v,off,model_address(get(v,off)));const auto recording=get(v,0x7268);put(v,0x7268,recording?replay_base+(recording-1)*ReplayBank::stride:0);
                for(unsigned k=0;k<4;++k){auto off=0x3964+k*0xd10+12,value=get(v,off);put(v,off,value?strip(3).address+(value-1)*16:0);}std::copy_n(v.begin(),car_stride,b.begin()+at);}
        }
        car(s.current_slot,0x4d5bb8);
        auto& viewport=s.viewport;
        field(viewport.center_x,0x576b94);field(viewport.center_y,0x576b98);
        field(viewport.scale_x,0x576b9c);field(viewport.scale_y,0x576ba0);
        if(loading){const auto p=u(0x4722d0);if(p<controller_base||p>=controller_base+4*0x74c||(p-controller_base)%0x74c)throw FormatError("whole tick controller pointer");s.controls.current=(p-controller_base)/0x74c;}
        else store(0x4722d0,controller_base+s.controls.current*0x74c);
        for(unsigned j=0;j<4;++j){auto& c=s.controls.controllers[j];auto& camera=c.camera;const auto at=controller_base+j*0x74c;
            field(c.active,at);car(c.vehicle,at+0xac);field(c.notices,at+0x4ac);
            for(unsigned k=0;k<8;++k){auto& notice=c.notices[k];if(loading)notice.source_vehicle=vehicle_id(notice.source_vehicle);else store(at+0x4ac+k*0x54+0x4c,vehicle_address(notice.source_vehicle));}
            field(c.respawn_transition.state,at+0x300);field(c.respawn_transition.phase,at+0x308);
            field(camera.mode,at+4);field(camera.position,at+8);field(camera.target,at+0x28);field(camera.up,at+0x38);
            field(camera.previous_target,at+0x68);field(camera.twist,at+0x78);field(camera.side,at+0x7c);
            field(camera.lens,at+0xb0);field(camera.near_clip,at+0xb4);field(camera.far_factor,at+0xb8);field(camera.half_width,at+0xbc);field(camera.half_height,at+0xc0);
            field(camera.velocity,at+0xc4);field(camera.linear_speed,at+0xd4);field(camera.drift,at+0xd8);field(camera.control_angles,at+0xe8);
            field(camera.angular_velocity,at+0xf4);field(camera.angular_acceleration,at+0x104);field(camera.collision_bypass_ticks,at+0x1a4);
            field(s.camera_lifecycle[j].clip_near,at+0x198);
            field(camera.stabilized_up,at+0x1ac);field(camera.settle_ticks,at+0x1bc);field(camera.speed_lift,at+0x1c0);field(camera.height_offset,at+0x1c4);
            field(camera.pending_follow_setup,at+0x1d4);field(camera.pending_lens,at+0x1e0);field(camera.requested_lens,at+0x1e4);
        }
        field(s.clock.game_mode,0x60f2b0);field(s.clock.phase,0x60f2b4);field(s.clock.countdown_pending,0x60f2c8);field(s.clock.rolling_state,0x60f2cc);
        field(s.clock.race_mode,0x60eff0);field(s.clock.remaining_ticks,0x60eff4);field(s.clock.lap_count,0x60eff8);field(s.clock.course_length,0x60f010);
        field(s.gameplay.values,0x60f2d0);field(s.score_rules.values,0x60d688);field(s.score_block,0x60d680);field(s.hud_block,0x61299c);field(s.simulation_tick,0x612cfc);
        field(s.tick_clock.scheduled,0x612d00);field(s.tick_clock.observed,0x612d04);field(s.tick_clock.increment,0x612d08);
        field(s.level.game_type,0x60df98);field(s.level.texture_suffix,0x60dfec);field(s.level.rolling_start_offset,0x60f014);field(s.pause_flag,0x4d5b20);field(s.split_setting,0x576be0);
        field(s.gravity.factor,0x482668);field(s.gravity.inverse_factor,0x48266c);field(s.gravity.magnitude,0x482670);field(s.gravity.inverse_magnitude,0x482674);field(s.gravity.vector,0x482678);field(s.gravity.unit,0x482688);
        field(s.random.state,0x4959c0);field(s.dynamics_workspace.steering,0x4d5b70);field(s.model_basis,0x4d5b30);
        field(s.frame_projection,0x61c3e8);field(s.frame_world_view,0x61c594);field(s.inverse_depth_scale,0x61c534);field(s.depth_offset,0x61c538);
        field(s.shared_model_identity,0x61c468);
        if(loading)for(auto& c:s.controls.controllers){c.camera.projection=s.frame_projection;c.camera.inverse_depth_scale=s.inverse_depth_scale;c.camera.depth_offset=s.depth_offset;}
        field(s.curves.restitution,0x497d20);field(s.curves.respawn,0x60dff0);field(s.curves.lights,0x60e1f0);field(s.curves.ambience,0x60e3f0);field(s.curves.ambience_adjacent_word,0x60e5f0);field(s.curves.width_left,0x60e7f0);field(s.curves.width_right,0x60ebf0);
        spline_field(s.grid.start_spline,0x571328);spline_field(s.rail.spline,0x4d5c9c);
        if(loading){const auto value=u(0x474670);s.rail.tube_glass_texture=value==0xffffffff?std::nullopt:std::optional(value);}else store(0x474670,s.rail.tube_glass_texture.value_or(0xffffffff));
        field(s.rail.source_word_4d5ca0,0x4d5ca0);
        field(s.reset_globals.transition_active,0x4d5ca4);field(s.reset_globals.special_value,0x4d5ca8);field(s.reset_globals.special_timer,0x4d5cac);
        field(s.automatic.initialized,0x49a164);field(s.automatic.route_distances,0x49a16c);field(s.automatic.target,0x49a148);field(s.automatic.target_speed,0x499fa8);field(s.automatic.braking_allowed,0x499fac);field(s.automatic.response_scale,0x499fb0);
        if(loading){if(u(0x49a158)){std::array<SplineView,3> routes;for(unsigned j=0;j<3;++j){auto route=spline(u(0x49a158+j*4));if(!route)throw FormatError("whole tick partial AI cache");routes[j]=*route;}s.automatic.routes=routes;}
            else{if(u(0x49a15c)||u(0x49a160))throw FormatError("whole tick partial AI cache");s.automatic.routes.reset();}}
        else for(unsigned j=0;j<3;++j)store(0x49a158+j*4,s.automatic.routes?spline_address((*s.automatic.routes)[j]):0);
        field(s.records.previous_lap_ticks,0x60effc);field(s.records.lap_ticks,0x60f000);field(s.records.score,0x60f008);
        field(s.finish.best_total_ticks,0x60f004);field(s.finish.best_total_score,0x60f00c);field(s.finish.count,0x612d94);
        for(unsigned j=0;j<16;++j)optional_car(s.finish.order[j],0x612d54+j*4);
        field(s.ranking.count,0x612d10);for(unsigned j=0;j<6;++j)optional_car(s.ranking.slots[j],0x612d14+j*4);
        field(s.trial_selection.availability,0x60d668);field(s.ghosts.reference_completed,0x60f284);
        for(unsigned j=0;j<4;++j){auto& role=s.ghosts.roles[j];const auto at=0x60d628+j*16;field(role.active,at);car(role.vehicle,at+4);field(role.lap_ticks,at+12);
            if(loading){auto handle=replay(u(at+8));if(!handle)throw FormatError("whole tick null ghost handle");role.replay=*handle;}else store(at+8,replay_address(role.replay));}
        for(unsigned j=0;j<6;++j){auto& c=s.cars[j];const auto at=0x60f02c+j*100;field(c,at);
            if(loading){c.source_word00=model_id(c.source_word00);c.source_word04=model_id(c.source_word04);}
            else{store(at,model_address(c.source_word00));store(at+4,model_address(c.source_word04));}}
        auto bank=s.replays.bytes();if(loading)std::copy_n(b.begin()+replay_base-global_base,bank.size(),bank.begin());
        //ReplayBank already retains the actual source32 header+10 words.
        if(!loading)std::copy(bank.begin(),bank.end(),b.begin()+replay_base-global_base);
        if(loading){s.replay_workspace.recording=replay(u(0x570ac0));s.replay_workspace.frame_offset=u(0x570ac4)-replay_base;s.replay_workspace.previous_frame_offset=u(0x4d5cb8)-replay_base;}
        else{store(0x570ac0,replay_address(s.replay_workspace.recording));store(0x570ac4,s.replay_workspace.frame_offset+replay_base);store(0x4d5cb8,s.replay_workspace.previous_frame_offset+replay_base);}
        field(s.score_popups.popups,0x496990);for(unsigned j=0;j<16;++j){auto& popup=s.score_popups.popups[j];if(loading)popup.vehicle=vehicle_id(popup.vehicle);else store(0x496990+j*0x138+4,vehicle_address(popup.vehicle));}
        field(s.score_popups.sound_class,0x497d10);field(s.score_popups.sound_id,0x497d14);field(s.score_popups.sound_volume,0x497d18);
        field(s.vehicle_audio.controller_gain,0x4746c0);field(s.vehicle_audio.motion_gain,0x4747b8);field(s.audio_frontier,0x570ac8);
        auto& a=s.audio_driver;field(a.initialized,0x62e0ac);field(a.spatial_mode,0x4897b4);field(a.range,0x4897a0);field(a.range_squared,0x4897a4);field(a.inverse_range,0x4897a8);field(a.duplicate_count,0x62f5b0);field(a.playing_count,0x62f5b4);field(a.spatial_template,0x4897b8);
        for(unsigned j=0;j<128;++j){auto& sample=a.samples[j];const auto at=0x62e118+j*0x20;if(loading){sample.buffer.value=u(at);sample.spatial.value=u(at+4);}else{store(at,static_cast<unsigned>(sample.buffer.value));store(at+4,static_cast<unsigned>(sample.spatial.value));}field(sample.type,at+8);field(sample.frequency,at+0x14);}
        field(s.audio_level.scale,0x489790);field(s.audio_level.input_a,0x62e0a8);field(s.audio_level.input_b,0x62e0f0);field(s.audio_level.output_a,0x62e0f8);field(s.audio_level.output_b,0x489798);field(s.audio_level.output_b_copy,0x48979c);
        field(s.contact_workspace.query_mass,0x4980c0);field(s.contact_workspace.query_motion,0x4980e8);field(s.contact_workspace.last_normal,0x498198);field(s.contact_workspace.average_payload,0x49a198);
        field(s.contact_workspace.impact.velocity_delta,0x49a17c);field(s.contact_workspace.impact.rotation_delta,0x49a19c);field(s.contact_workspace.impact.normal_speed,0x49a1dc);
        field(s.contact_workspace.inverse_probe_count,0x49a1e0);field(s.contact_workspace.response.contact_load,0x49a1e4);field(s.contact_workspace.response.alignment,0x49a1e8);field(s.contact_workspace.response.body_touched,0x49a1ec);
        if(loading){if(u(0x49a1f0)>1)throw FormatError("whole tick impact bool");s.contact_workspace.impact.touched=u(0x49a1f0)!=0;}else store(0x49a1f0,s.contact_workspace.impact.touched?1u:0u);
        auto& q=s.contact_cast;field(q.start,0x4980c4);field(q.end,0x4980d4);field(q.motion_payload,0x4980f4);field(q.step,0x498118);field(q.direction,0x498128);field(q.step_length,0x498148);field(q.travel_limit,0x49814c);field(q.remaining,0x498150);field(q.radius,0x498154);field(q.radius_squared,0x498158);
        field(q.center,0x4981a8);field(q.surface,0x4981b8);field(q.hit_flag,0x4981e4);field(q.polygon_list,0x49823c);field(q.mode,0x4754a0);
        if(loading)q.polygon=polygon(u(0x498164));else store(0x498164,polygon_address(q.polygon));
        if(loading){q.internals.plane_vertex=vector_id(u(0x49815c),vertex_base,vertex_count);q.internals.plane_normal=vector_id(u(0x498160),normal_base,normal_count);}
        else{store(0x49815c,vector_address(q.internals.plane_vertex,vertex_base,vertex_count));store(0x498160,vector_address(q.internals.plane_normal,normal_base,normal_count));}
        field(q.internals.endpoint_plane_distance,0x498168);field(q.internals.start_plane_distance,0x49816c);
        field(q.internals.auxiliary,0x498188);field(q.internals.candidate_distance,0x4981c8);
        field(q.internals.tree_tier,0x498240);field(q.internals.contact_kind,0x5715cc);
        const oracle::particles::WorldOffsets offsets{0x612da4-global_base,0x612d98-global_base,0x612d9c-global_base,0x612da0-global_base,0x61297c-global_base,
            0x62a078-global_base,0x62d478-global_base,0x4d5bc0-global_base,0x4d5c94-global_base};
        oracle::particles::transfer_world(b,offsets,s.impact_particles,s.effect_particles,s.effect_staging,loading);
        field(s.light_selection.active_count,0x62452c);
        transfer_lighting(b,loading);
        const auto block=[&](auto& value,unsigned index){const auto& r=strip(index);if(sizeof(value)!=r.length)throw FormatError("whole tick generated buffer size");if(loading)std::memcpy(&value,b.data()+r.offset,sizeof(value));else std::memcpy(b.data()+r.offset,&value,sizeof(value));};
        block(s.strip.polygons,0);block(s.strip.corner_indices,1);block(s.strip.vertices,2);block(s.strip.corners,3);field(s.strip.normals,0x570ad8);field(s.strip.texture_names,0x5712e0);field(s.strip.field_5712f0,0x5712f0);field(s.strip.field_5712f4,0x5712f4);field(s.strip.field_5712f8,0x5712f8);field(s.strip.texture_id,0x5712fc);
        const auto half=[&](std::uint16_t& value,unsigned offset){if(loading)std::memcpy(&value,b.data()+offset,2);else std::memcpy(b.data()+offset,&value,2);};
        const auto part=strip(4).offset,descriptor=strip(5).offset;
        half(s.strip.part_flags,part);half(s.strip.part_field20,part+0x20);half(s.strip.part_field22,part+0x22);half(s.strip.polygon_count,part+0x24);half(s.strip.corner_count,part+0x26);half(s.strip.vertex_count,part+0x28);half(s.strip.model_field20,descriptor+0x20);half(s.strip.model_field22,descriptor+0x22);
        // Historical whole-tick corpus represents profiles0..4; keep its exact scope.
        for(unsigned j=0;j<5;++j)field(s.trail_profiles.profiles[j],0x474af8+j*0x110);
        for(unsigned j=0;j<5;++j){auto& profile=s.trail_profiles.profiles[j];const auto address=0x474af8+j*0x110+16;
            if(loading){const auto pointer=profile.shape_identity;if(pointer!=0 && pointer!=0x474988 && pointer!=0x474a78)throw FormatError("whole tick trail shape pointer");
                profile.shape_identity=pointer==0?0u:pointer==0x474988?1u:2u;}
            else{if(profile.shape_identity>2)throw FormatError("whole tick trail shape identity");store(address,profile.shape_identity==0?0u:profile.shape_identity==1?0x474988u:0x474a78u);}}
        for(unsigned j=0;j<2;++j){auto& shape=s.trail_profiles.shapes[j];if(loading)shape.identity=j+1;
            else if(shape.identity!=j+1)throw FormatError("whole tick trail shape owner changed");field(shape.points,j==0?0x474988:0x474a78);}
    }
};
}
