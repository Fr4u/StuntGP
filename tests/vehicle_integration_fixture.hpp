#pragma once
#include "vehicle_tick_full_state.hpp"
#include <fstream>
#include <iostream>
#include <limits>

namespace sgn::retail::integration_test {
using namespace sgn;
using namespace sgn::retail;
using namespace sgn::retail::whole_tick_test;
struct Reader {
    std::ifstream input;
    explicit Reader(const char* path):input(path,std::ios::binary){if(!input)throw FormatError("cannot open whole tick corpus");}
    Bytes bytes(unsigned count){if(count>8*1024*1024)throw FormatError("whole tick field too large");Bytes b(count);if(!input.read(reinterpret_cast<char*>(b.data()),count))throw FormatError("truncated whole tick corpus");return b;}
    unsigned word(){return get(bytes(4),0);}
    std::string text(unsigned limit){const auto count=word();if(count>limit)throw FormatError("whole tick text too long");const auto b=bytes(count);if(std::find(b.begin(),b.end(),0)!=b.end())throw FormatError("whole tick embedded NUL");return {b.begin(),b.end()};}
};
// The source runs initialized==0. Any native OS request is a domain failure,
// rather than a no-op substitute for a source game procedure.
struct DisabledDevice final:AudioDevice {
    [[noreturn]] static AudioResult fail(){throw FormatError("whole tick disabled source audio reached OS boundary");}
    AudioResult status(AudioBufferId,std::uint32_t&) override{return fail();}
    AudioResult duplicate(AudioBufferId,AudioBufferId&) override{return fail();}
    AudioResult query_spatial(AudioBufferId,AudioSpatialId&) override{return fail();}
    std::uint32_t release(AudioBufferId) override{fail();}
    std::uint32_t release_spatial(AudioSpatialId) override{fail();}
    AudioResult frequency(AudioBufferId,std::uint32_t) override{return fail();}
    AudioResult volume(AudioBufferId,std::int32_t) override{return fail();}
    AudioResult pan(AudioBufferId,std::int32_t) override{return fail();}
    AudioResult play(AudioBufferId,std::uint32_t,std::uint32_t,std::uint32_t) override{return fail();}
    AudioResult stop(AudioBufferId) override{return fail();}
    AudioResult spatial_parameters(AudioSpatialId,const AudioSpatialParameters&,std::uint32_t) override{return fail();}
    AudioResult spatial_mode(AudioSpatialId,std::uint32_t,std::uint32_t) override{return fail();}
    AudioResult spatial_position(AudioSpatialId,Vec3,std::uint32_t) override{return fail();}
    AudioResult spatial_velocity(AudioSpatialId,Vec3,std::uint32_t) override{return fail();}
    AudioResult listener_position(AudioListenerId,Vec3,std::uint32_t) override{return fail();}
    AudioResult listener_velocity(AudioListenerId,Vec3,std::uint32_t) override{return fail();}
    AudioResult listener_orientation(AudioListenerId,Vec3,Vec3,std::uint32_t) override{return fail();}
};
struct RequestObserver final:AudioRequestObserver {
    std::array<std::array<unsigned,7>,256> events{};
    std::array<std::pair<const unsigned*,unsigned>,64> handles{};
    unsigned count{},handle_count{};bool invalid{};
    explicit RequestObserver(RaceSessionState& s){
        auto add=[&](const unsigned& value,unsigned source){if(handle_count==handles.size())throw FormatError("whole tick observer handle capacity");handles[handle_count++]={&value,source};};
        for(unsigned j=0;j<6;++j){auto& a=s.vehicles.audio[j];const auto base=car_base+j*car_stride;
            add(a.horn_handle,base+0x2338);add(a.impact_handle,base+0x2370);add(a.grip_handle,base+0x237c);add(a.scrape_handle,base+0x2380);add(a.rolling_handle,base+0x2384);
            add(a.boost_handle,base+0x2388);add(a.idle_handle,base+0x238c);add(a.rev_handle,base+0x2390);add(a.motion_handle,base+0x239c);}
        add(s.audio_frontier.steering_handle,0x570ad0);
    }
    void observe(unsigned sample,unsigned flags,unsigned frequency,unsigned volume,std::int32_t pan,unsigned force,const unsigned* handle) noexcept override {
        unsigned address=0;if(handle){for(unsigned j=0;j<handle_count;++j)if(handles[j].first==handle)address=handles[j].second;if(address==0)invalid=true;}
        if(count==events.size()){invalid=true;return;}events[count++]={sample,flags,frequency,volume,std::bit_cast<unsigned>(pan),force,address};
    }
};
struct Fixture {
    std::filesystem::path root;
    AssetStore assets;
    Bytes track;
    SplineSet splines;
    contact::TrackGeometry geometry;
    std::vector<std::string> labels;
    DecodedTrackSplines track_splines{splines,labels,track}; //R18: decoded TrackSplineSource over the same bytes
    ModelResources models;
    std::unique_ptr<RaceSessionState> state{std::make_unique<RaceSessionState>()};
    std::vector<Bytes> authored;
    std::vector<Region> regions;
    std::unique_ptr<StateCodec> codec;
    Bytes actual;
    DisabledDevice device;
    RequestObserver observer{*state};
    VehicleMatrix rotation{};
    contact::effects::EffectParticleFrameAudit frames;
    VehicleSkidWriteAudit skid;
    contact::ContactCastPayloadAudit cast;
    contact::solver::PayloadAudit payload;
    std::map<unsigned,unsigned> opaque;
    std::array<std::pair<unsigned,unsigned>,32> additional_retained{};
    unsigned additional_retained_count{};
    bool compare_launch_position_provenance{};
    std::map<const float*,unsigned> trail_addresses;
    std::array<std::pair<const float*,VehicleTrailOpaqueLane>,128> trail_events{};
    unsigned trail_event_count{};
    bool trail_invalid{};
    bool trace{};
    unsigned last_call{};
    void trace_call(unsigned address) noexcept {
        last_call=address;if(!trace)return;const auto& s=*state;const auto current=s.current_slot;
        const auto selected=s.controls.controllers[s.controls.current].vehicle;
        std::cerr<<"trace "<<std::hex<<address<<" rng "<<s.random.state<<" throttle "<<std::bit_cast<unsigned>(s.vehicles.physics[current].throttle)
                 <<" rpm "<<std::bit_cast<unsigned>(s.vehicles.physics[current].rpm)<<" gear "<<s.vehicles.physics[current].gear
                 <<" pulse "<<s.vehicles.ai[current].start_throttle_ticks<<" selected "<<selected<<" countdown "<<s.vehicles.race[selected].start_countdown
                 <<" skill "<<s.cars[current].settings.ai_skill<<" trail_tick "<<s.impact_particles.rate_selector
                 <<" trail_enable "<<s.vehicles.trail_inputs[current].field_28a0<<" trail_events "<<trail_event_count<<std::dec<<'\n';
    }
    Fixture(std::filesystem::path path,std::string name,std::vector<Region> layout,Bytes before,std::vector<std::string> texts)
        :root(std::move(path)),assets(root),track(read_file(root/"meshdata"/name)),splines(track),geometry(contact::decode_track_collision(track)),
         labels(decode_pmd(track).textures),models(assets,root),regions(std::move(layout)),actual(std::move(before)) {
        for(const auto& region:regions)if(region.kind==1){const auto token=models.load(std::filesystem::path(region.name).stem().string());if(token!=region.index+1)throw FormatError("whole tick model token order");
            authored.push_back(read_file(root/"meshdata"/region.name));if(authored.back().size()!=region.length)throw FormatError("whole tick PMD size differs");
            if(region.descriptor-region.address!=0x148+get(authored.back(),0x4c))throw FormatError("whole tick authored descriptor identity");
            tick_test::apply_model_visual(models.at(region.index),authored.back(),std::span(actual).subspan(region.offset,region.length));
            auto& model=models.at(region.index);const auto descriptor=region.offset+region.descriptor-region.address;
            model.previous_detail_level()=get(actual,descriptor+0x64);
            std::memcpy(&model.source_model_flags_02(),actual.data()+descriptor+2,2);
            std::memcpy(&model.source_model_ambience(),actual.data()+descriptor+4,2);
            std::memcpy(&model.source_model_vector_10(),actual.data()+descriptor+0x10,16);
            const auto attributes=model.source_attribute_table();const auto at=0x148+get(authored.back(),0x24);
            if(at>region.length||attributes.size_bytes()>region.length-at)throw FormatError("whole tick model attribute table bounds");
            std::memcpy(attributes.data(),actual.data()+region.offset+at,attributes.size_bytes());}
        codec=std::make_unique<StateCodec>(*state,models,splines,track,regions);codec->transfer(actual,true);
        if(state->audio_driver.initialized!=0)throw FormatError("whole tick fixture requires disabled audio device");
        for(unsigned j=0;j<6;++j){const auto id=models.identity(state->vehicles.models[j].model);if(id==0)throw FormatError("whole tick no car model");
            const auto& region=regions.at(id);state->selected_lod[j]=get(actual,region.offset+region.descriptor-region.address+0x5c);}
        unsigned cursor=0;for(unsigned j=0;j<texts.size();++j){auto& target=state->game_text;if(texts[j].size()+1>target.arena.size()-cursor)throw FormatError("whole tick text arena capacity");
            target.offsets[j]=cursor;std::copy(texts[j].begin(),texts[j].end(),target.arena.begin()+cursor);cursor+=static_cast<unsigned>(texts[j].size());target.arena[cursor++]=0;}
        std::memcpy(rotation.data(),actual.data()+0x61c3a4-global_base,64);
        trail_addresses.emplace(&state->model_basis[3],0x4d5b3c-global_base);
        trail_addresses.emplace(&state->model_basis[7],0x4d5b4c-global_base);
        for(unsigned j=0;j<6;++j){const auto base=car_base-global_base+j*car_stride;auto& v=state->vehicles;
            const auto bind=[&](auto& ring,unsigned at){for(unsigned k=0;k<ring.size();++k)trail_addresses.emplace(&ring[k].w,base+at+k*16+12);};
            bind(v.trails[j].speed,0x351c);bind(v.trails[j].pair_a[0],0x2d14);bind(v.trails[j].pair_a[1],0x2f2c);
            bind(v.trails[j].pair_b[0],0x28d0);bind(v.trails[j].pair_b[1],0x2aec);
            bind(v.model_effects[j].trails[0],0x32c0);bind(v.model_effects[j].trails[1],0x3400);}
        auto roundtrip=actual;save(roundtrip);
        for(unsigned at=0;at<actual.size();at+=4)if(get(roundtrip,at)!=get(actual,at)){
            std::cerr<<"initial owner roundtrip offset "<<std::hex<<at<<" actual "<<get(roundtrip,at)<<" original "<<get(actual,at)<<std::dec<<'\n';throw FormatError("whole tick initial owner roundtrip differs");}
    }
    void save(Bytes& output){codec->transfer(output,false);std::memcpy(output.data()+0x61c3a4-global_base,rotation.data(),64);
        for(const auto& region:regions)if(region.kind==1){
            for(const auto [offset,value]:tick_test::capture_model_visual(models.at(region.index),authored.at(region.index)))put(output,region.offset+offset,value);
            auto& model=models.at(region.index);const auto descriptor=region.offset+region.descriptor-region.address;
            put(output,descriptor+0x64,model.previous_detail_level());
            std::memcpy(output.data()+descriptor+2,&model.source_model_flags_02(),2);
            std::memcpy(output.data()+descriptor+4,&model.source_model_ambience(),2);
            std::memcpy(output.data()+descriptor+0x10,&model.source_model_vector_10(),16);
            const auto attributes=model.source_attribute_table();const auto at=0x148+get(authored.at(region.index),0x24);
            std::memcpy(output.data()+region.offset+at,attributes.data(),attributes.size_bytes());}
        for(unsigned j=0;j<6;++j){const auto id=models.identity(state->vehicles.models[j].model);const auto& region=regions.at(id);
            put(output,region.offset+region.descriptor-region.address+0x5c,state->selected_lod[j]);}}
    using Invoke=VehicleTickObservation (*)(RaceSessionState&,const RaceSessionVehicleResources&,VehicleTickAudit*);
    VehicleTickObservation step(unsigned slot,unsigned tick,Invoke invoke){
        state->current_slot=slot;state->simulation_tick=tick;
        auto& s=*state;const auto reset_route=codec->spline(get(actual,0x624474-global_base));if(!reset_route)throw FormatError("whole tick reset spline missing");
        VehicleResetParameters reset;reset.stage=std::bit_cast<std::int32_t>(s.clock.phase);reset.mode=std::bit_cast<std::int32_t>(s.clock.game_mode);reset.selector=std::bit_cast<std::int32_t>(s.level.game_type);
        reset.normal_energy_cost=s.gameplay.values[1];reset.upgraded_energy_cost=s.gameplay.values[3];reset.course_length=s.clock.course_length;
        reset.spacing_word=get(track,get(actual,0x624474-global_base)-codec->track_base+0x24);reset.spline=*reset_route;reset.mask=s.curves.respawn;reset.unit_gravity=s.gravity.unit;reset.tick=s.simulation_tick;
        for(unsigned j=0;j<6;++j)reset.vehicle_names[j]=s.cars[j].name.data();reset.special_end_text=*s.game_text.text((0x6127fc-0x6127a0)/4);reset.retired_text=*s.game_text.text((0x6127e4-0x6127a0)/4);
        const auto f=[&](unsigned address){return std::bit_cast<float>(get(actual,address-global_base));};
        VehicleTrailProjection projection{s.frame_world_view,s.frame_projection[0],s.frame_projection[5],s.viewport.center_x,s.viewport.center_y,s.viewport.scale_x,s.viewport.scale_y,f(controller_base+s.controls.current*0x74c+0x198)};
        RaceSessionVehicleResources resources{models,track_splines,geometry,device,reset,projection,&observer};
        observer.count=0;observer.invalid=false;
        last_call=0;
        VehicleTickAudit audit{&frames,&skid,&cast,&rotation,&payload,this,[](void* context,unsigned address) noexcept{static_cast<Fixture*>(context)->trace_call(address);}};
        trail_event_count=0;trail_invalid=false;
        VehicleTrailWriteAudit trail_audit{this,[](void* context,const float& value,VehicleTrailOpaqueLane lane) noexcept{
            auto& f=*static_cast<Fixture*>(context);if(f.trail_event_count==f.trail_events.size()){f.trail_invalid=true;return;}
            f.trail_events[f.trail_event_count++]={&value,lane};}};
        audit.trail_writes=&trail_audit;
        VehicleTickObservation result;
        const auto previous=actual;
        additional_retained_count=0;
        try{result=invoke(s,resources,&audit);}catch(const std::exception&){std::cerr<<"native slot "<<slot<<" last source caller "<<std::hex<<last_call<<std::dec<<'\n';throw;}
        if(observer.invalid)throw FormatError("whole tick audio observer identity/capacity");save(actual);
        for(unsigned i=0;i<additional_retained_count;++i){const auto [at,writer]=additional_retained[i];
            if(get(actual,at)!=get(previous,at))throw FormatError("integration extra opaque retention");
            opaque[at]=writer;}
        const auto base=car_base-global_base+slot*car_stride;
        for(unsigned lane:{0x1bd8u,0x24ccu})opaque.erase(base+lane);
        // After airborne tick15 the leveling call stops; its three prior W
        // payloads persist. Matrix products write only3x3 and do not define W.
        const auto mark=[&](unsigned lane,unsigned writer,unsigned retained){
            if(get(actual,base+lane)!=get(previous,base+retained))throw FormatError("whole tick native opaque retention");
            opaque.emplace(base+lane,writer);};
        if(result.progression.heading_opaque)mark(0x24cc,0x41e044,0x24cc);
        if(result.air.orthogonalized)mark(0x1bd8,0x44f9c3,0x1cc8);
        if(result.air.leveled_incremental){mark(0x1c18,0x44f08a,0x1c18);mark(0x1c28,0x44f0a4,0x1c28);mark(0x1c38,0x44f0d6,0x1c38);}
        if(trail_invalid)throw FormatError("whole tick trail diagnostic capacity");
        constexpr std::array<unsigned,4> trail_writers{0x44f08a,0x44f0a4,0x41e6a8,0x41e6be};
        for(unsigned j=0;j<trail_event_count;++j){const auto& event=trail_events[j];const auto found=trail_addresses.find(event.first);
            if(found==trail_addresses.end())throw FormatError("whole tick unknown native trail lane");
            const auto at=found->second;if(get(actual,at)!=get(previous,at))throw FormatError("whole tick trail opaque retention");
            opaque[at]=trail_writers.at(static_cast<unsigned>(event.second));}
        for(unsigned j=0;j<64;++j){const auto at=0x62a078-global_base+j*0xd0;const auto& particle=s.effect_particles.particles[j];
            if(compare_launch_position_provenance){
                opaque.erase(at+0x14);
                if(const auto* source=frames.position_sources[j]){
                    bool bound=false;
                    for(const auto& car:s.vehicles.presentation)for(const auto& center:car.wheel_centers)if(source==&center.w)bound=true;
                    if(!bound||get(actual,at+0x14)!=std::bit_cast<unsigned>(*source))throw FormatError("integration launch position payload copy");
                    opaque.emplace(at+0x14,0x45e151);
                }
            }
            const auto lane=[&](unsigned field,bool undefined){opaque.erase(at+field);if(undefined){
                if(get(actual,at+field)!=get(previous,at+field))throw FormatError("whole tick particle opaque retention");opaque.emplace(at+field,0x45e151);}};
            for(unsigned k=0;k<7;++k)lane(contact::takeoff_oracle::undefined_offsets[k],particle.source_undefined[k]);
            for(unsigned k=0;k<16;++k)lane(0x18+k*4,frames.undefined[j][k]);}
        return result;
    }
};
inline int run_corpus(int argc,char** argv,std::string_view signature,Fixture::Invoke invoke)try {
    if(argc<3||argc>4||(argc==4&&std::string_view(argv[3])!="--trace"))throw FormatError("usage: vehicle_tick_full_oracle corpus retail-root [--trace]");Reader reader(argv[1]);const auto magic=reader.bytes(8);
    if(std::string_view(reinterpret_cast<const char*>(magic.data()),magic.size())!=signature)throw FormatError("whole tick signature");
    const auto cases=reader.word();if(!cases||cases>24)throw FormatError("whole tick case count");
    unsigned calls=0,errors=0,audio_count=0;std::uint64_t exact=0,excluded=0;
    for(unsigned n=0;n<cases;++n){const auto track=reader.text(64);if(track!="track1.PMD")throw FormatError("whole tick initial corpus track domain");
        const auto steps=reader.word(),mode=reader.word(),config=reader.word(),size=reader.word(),count=reader.word();
        if(!steps||steps>384||(mode!=1&&mode!=3&&mode!=4)||config<1||config>5||size<global_size||size>8*1024*1024||count!=19)throw FormatError("whole tick case header");
        std::vector<Region> regions;unsigned offset=0;
        for(unsigned j=0;j<count;++j){Region r;r.kind=reader.word();r.index=reader.word();r.address=reader.word();r.length=reader.word();r.descriptor=reader.word();r.name=reader.text(64);r.offset=offset;
            const auto kind=j==0?0u:j<=12?1u:2u,index=j==0?0u:j<=12?j-1:j-13;
            if(r.kind!=kind||r.index!=index||r.length==0||r.length%4||r.length>size-offset||r.address>std::numeric_limits<unsigned>::max()-r.length)throw FormatError("whole tick region layout");
            if(j==0&&(r.address!=global_base||r.length!=global_size||r.name!="globals"||r.descriptor))throw FormatError("whole tick global region");
            if(kind==1){const auto expected="car"+std::to_string(index/2+1)+(index%2?"sh.PMD":".PMD");if(r.name!=expected||r.descriptor<r.address||r.descriptor>=r.address+r.length)throw FormatError("whole tick model region");}
            for(const auto& previous:regions)if(r.address<previous.address+previous.length&&previous.address<r.address+r.length)throw FormatError("whole tick overlapping source regions");
            offset+=r.length;regions.push_back(std::move(r));}
        if(offset!=size)throw FormatError("whole tick record size");const auto text_count=reader.word();if(text_count!=93)throw FormatError("whole tick text count");std::vector<std::string> texts;for(unsigned j=0;j<text_count;++j)texts.push_back(reader.text(63));
        auto expected=reader.bytes(size);Fixture fixture(argv[2],track,std::move(regions),expected,std::move(texts));
        fixture.trace=argc==4;
        if(fixture.state->clock.game_mode!=mode)throw FormatError("whole tick mode metadata disagrees");
        if(mode==3&&(fixture.state->level.game_type!=3||fixture.state->clock.lap_count!=100))throw FormatError("whole tick challenge domain disagrees");
        for(unsigned step=0;step<steps;++step){const auto slot=reader.word(),tick=reader.word(),patches=reader.word();if(slot>=6||patches>size/4)throw FormatError("whole tick call header");
            unsigned previous=0;for(unsigned j=0;j<patches;++j){const auto at=reader.word(),value=reader.word();if(at%4||at>=size||(j&&at<=previous)||get(expected,at)==value)throw FormatError("whole tick invalid expected patch");put(expected,at,value);previous=at;}
            const auto event_count=reader.word();if(event_count>256)throw FormatError("whole tick event count");std::vector<std::array<unsigned,7>> events(event_count);for(auto& event:events)for(auto& value:event)value=reader.word();
            const auto opaque_count=reader.word();if(opaque_count>4096)throw FormatError("whole tick opaque count");
            std::map<unsigned,unsigned> opaque;
            for(unsigned j=0;j<opaque_count;++j){const auto at=reader.word(),writer=reader.word();
                if(at%4||at>=size||!opaque.emplace(at,writer).second)throw FormatError("whole tick opaque record");}
            fixture.step(slot,tick,invoke);++calls;audio_count+=event_count;
            if(opaque!=fixture.opaque){
                std::cerr<<"opaque provenance case "<<n<<" step "<<step<<" source "<<opaque.size()<<" native "<<fixture.opaque.size()<<'\n';
                for(const auto& [at,writer]:opaque)if(!fixture.opaque.contains(at)||fixture.opaque.at(at)!=writer)
                    std::cerr<<"source-only offset "<<std::hex<<at<<" writer "<<writer<<std::dec<<'\n';
                for(const auto& [at,writer]:fixture.opaque)if(!opaque.contains(at)||opaque.at(at)!=writer)
                    std::cerr<<"native-only offset "<<std::hex<<at<<" writer "<<writer<<std::dec<<'\n';
                unsigned differences=0;
                for(unsigned at=0;at<size;at+=4)if(!opaque.contains(at)&&get(fixture.actual,at)!=get(expected,at)){
                    if(differences<16)std::cerr<<"value offset "<<std::hex<<at<<" actual "<<get(fixture.actual,at)<<" expected "<<get(expected,at)<<std::dec<<'\n';
                    ++differences;}
                std::cerr<<"defined value differences "<<differences<<'\n';
                throw FormatError("whole tick source/native opaque provenance disagrees");}
            excluded+=opaque.size();
            if(fixture.observer.count!=events.size()||!std::equal(events.begin(),events.end(),fixture.observer.events.begin())){++errors;std::cerr<<"whole tick audio trace differs case "<<n<<" step "<<step<<'\n';}
            for(unsigned at=0;at<size;at+=4){if(opaque.contains(at))continue;if(get(fixture.actual,at)==get(expected,at)){++exact;continue;}
                if(errors++<20){unsigned address=0;for(const auto& region:fixture.regions)if(at>=region.offset&&at<region.offset+region.length)address=region.address+at-region.offset;
                    std::cerr<<"whole tick case "<<n<<" step "<<step<<" address "<<std::hex<<address<<" actual "<<get(fixture.actual,at)<<" expected "<<get(expected,at)<<std::dec<<'\n';}}
        }
    }
    if(reader.input.peek()!=std::char_traits<char>::eof())throw FormatError("trailing whole tick corpus data");
    std::cout<<"{\"cases\":"<<cases<<",\"calls\":"<<calls<<",\"exact_words\":"<<exact<<",\"errors\":"<<errors<<",\"audio_events\":"<<audio_count<<",\"excluded_words\":"<<excluded<<",\"allowed_ulp\":0}\n";return errors?1:0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}

}
