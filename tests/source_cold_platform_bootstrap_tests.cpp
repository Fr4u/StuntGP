#include "sgn/race_session.hpp"
#include "sgn/race_session_music.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <thread>
#include <type_traits>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{};
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);++checks;}
template<class Error=FormatError,class F> void rejects(F&& call,const char* why){
    bool rejected=false;
    try{call();}catch(const Error&){rejected=true;}
    check(rejected,why);
}
struct PeImage {
    std::vector<std::uint8_t> data;
    std::uint32_t base{},sections{},section_count{};
    explicit PeImage(const std::filesystem::path& path){
        std::ifstream input(path,std::ios::binary);
        if(!input)throw std::runtime_error("cannot open pinned PE receiving input");
        data.assign(std::istreambuf_iterator<char>(input),{});
        check(word(0)%65536==0x5a4d,"PE MZ signature");
        const auto nt=word(0x3c);
        check(word(nt)==0x4550,"PE signature");
        check((word(nt+4)&0xffff)==0x14c,"PE machine");
        const auto opt=nt+24;
        check((word(opt)&0xffff)==0x10b,"PE32 optional header");
        base=word(opt+28);check(base==0x400000,"PE preferred base");
        section_count=word(nt+4)>>16;sections=opt+(word(nt+20)&0xffff);
    }
    std::uint32_t word(std::size_t at) const{
        if(at>data.size()||data.size()-at<4)throw std::runtime_error("PE word outside file");
        return std::uint32_t(data[at])|(std::uint32_t(data[at+1])<<8)|
            (std::uint32_t(data[at+2])<<16)|(std::uint32_t(data[at+3])<<24);
    }
    std::uint8_t at_va(std::uint32_t va) const{
        if(va<base)throw std::runtime_error("PE VA before image");
        const auto rva=va-base;
        for(std::uint32_t i=0;i<section_count;++i){
            const auto s=std::size_t(sections)+40u*i;
            const auto virtual_size=word(s+8),start=word(s+12),raw_size=word(s+16);
            if(rva>=start&&rva-start<std::max(virtual_size,raw_size)){
                const auto delta=rva-start;
                if(delta>=raw_size)return 0;
                const auto at=std::size_t(word(s+20))+delta;
                if(at>=data.size())throw std::runtime_error("PE section outside file");
                return data[at];
            }
        }
        throw std::runtime_error("PE VA outside sections");
    }
    std::uint32_t at_word(std::uint32_t va) const{
        return std::uint32_t(at_va(va))|(std::uint32_t(at_va(va+1))<<8)|
            (std::uint32_t(at_va(va+2))<<16)|(std::uint32_t(at_va(va+3))<<24);
    }
};
// Explicit field projection, not memcpy of an x86 image into native objects.
constexpr std::array<std::uint32_t,35> completion_addresses{
    0x482668,0x48266c,0x482670,0x482674,0x482678,0x48267c,0x482680,
    0x482688,0x48268c,0x482690,0x482694, //482684 has no WorldGravity field
    0x4746c0,0x4747b8,0x472c74,0x4754a0,0x4897a0,0x4897a4,0x4897a8,
    0x4897b8,0x4897bc,0x4897c0,0x4897c4,0x4897c8,0x4897cc,0x4897d0,
    0x4897d4,0x4897d8,0x4897dc,0x4897e0,0x4897e4,0x4897e8,0x4897ec,0x4897f0,0x4897f4,
    0x60d74c
};
std::array<std::uint32_t,35> completion_values(const RaceSessionState& s){
    const auto f=[](float value){return std::bit_cast<std::uint32_t>(value);};
    const auto& g=s.gravity;const auto& a=s.audio_driver;const auto& p=a.spatial_template;
    return {
        f(g.factor),f(g.inverse_factor),f(g.magnitude),f(g.inverse_magnitude),
        f(g.vector.x),f(g.vector.y),f(g.vector.z),f(g.unit.x),f(g.unit.y),f(g.unit.z),f(g.unit.w),
        s.vehicle_audio.controller_gain,f(s.vehicle_audio.motion_gain),
        std::bit_cast<std::uint32_t>(s.load_selection.configuration_id),s.contact_cast.mode,
        a.range,a.range_squared,f(a.inverse_range),p.size,
        f(p.position.x),f(p.position.y),f(p.position.z),f(p.velocity.x),f(p.velocity.y),f(p.velocity.z),
        p.inside_cone_angle,p.outside_cone_angle,f(p.cone_orientation.x),f(p.cone_orientation.y),f(p.cone_orientation.z),
        std::bit_cast<std::uint32_t>(p.outside_cone_volume),f(p.minimum_distance),f(p.maximum_distance),p.mode,
        s.options.texture
    };
}
void check_completion_pe(const RaceSessionState& s,const PeImage& pe){
    const auto values=completion_values(s);
    for(std::size_t i=0;i<values.size();++i){
        if(values[i]!=pe.at_word(completion_addresses[i]))
            std::cerr<<"PE field VA="<<std::hex<<completion_addresses[i]<<" actual="<<values[i]
                     <<" expected="<<pe.at_word(completion_addresses[i])<<std::dec<<'\n';
        check(values[i]==pe.at_word(completion_addresses[i]),"completed PE field mismatch");
    }
    check(pe.at_word(0x482684)==0,"captured gravity gap changed; do not invent its owner");
    check(s.audio_driver.spatial_template.size==64,"DS3DBUFFER source size must be literal64");
    check(std::bit_cast<std::uint32_t>(s.audio_driver.inverse_range)==0x390bcf65u,"PE inverse range bits changed");
}
void write_later_completion_state(RaceSessionState& s){
    // Receiving fixture writes establish distinct values in every new owner.
    // The two actual existing source leaves below then overwrite their own
    // fields. A borrow/attachment must preserve the resulting state exactly.
    s.gravity={7,8,9,10,{11,12,13},{14,15,16,17}};
    s.vehicle_audio={63,0.375f};s.load_selection.configuration_id=-19;s.contact_cast.mode=3;
    s.audio_driver.range=43;s.audio_driver.range_squared=87;s.audio_driver.inverse_range=3;
    s.audio_driver.spatial_template={32,{1,2,3},{4,5,6},45,67,{7,8,9},-81,123,456,2};
    s.options.texture=19;
    initialize_world_gravity(s.gravity); //actual441F90, no host/native backend
    audio_set_range(s.audio_driver,3210); //actual460D20, no host/native backend
    check(std::bit_cast<std::uint32_t>(s.gravity.factor)==std::bit_cast<std::uint32_t>(7.0f),"441F90 changed factor");
    check(std::bit_cast<std::uint32_t>(s.gravity.unit.w)==std::bit_cast<std::uint32_t>(17.0f),"441F90 changed unit.W");
    check(s.audio_driver.range==3210&&s.audio_driver.range_squared==10304100,"460D20 did not write its range owners");
}

std::array<std::uint32_t*,8> selected(RaceSessionState& s){
    return {&s.random.state,&s.audio_level.scale,&s.audio_initialization.requested_rate,
        &s.audio_level.output_b,&s.audio_level.output_b_copy,
        &s.audio_initialization.spatial_capability,&s.audio_initialization.spatial_enabled,
        &s.audio_driver.spatial_mode};
}
void empty_backends(RaceSessionPlatform& p){
    const auto r=p.host_readiness();
    check(!r.assets&&!r.audio_device&&!r.audio_stream&&!r.window_input,"construction/borrow activated a host backend");
}
}
int main(int argc,char** argv)try{
    if(argc!=2)throw std::runtime_error("expected pinned NEW PE path; file is read, never executed");
    static_assert(std::is_aggregate_v<RaceSessionState>,"do not break designated fixture initialization");
    static_assert(sizeof(WorldGravity)==44); //11 fields; excludes PE482684 gap
    static_assert(sizeof(AudioSpatialParameters)==64); //separate from source size member
    static_assert(!std::is_copy_constructible_v<RaceSessionPlatform>);
    static_assert(!std::is_move_constructible_v<RaceSession>);
    const PeImage pe(argv[1]);
    const auto missing=std::filesystem::path(argv[1]).parent_path()/"__missing_source_cold_bootstrap_assets__";
    check(!std::filesystem::exists(missing),"receiving requires its deterministic missing assets path to be absent");
    auto owner=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},missing,0.0f);
    auto& s=owner->state();
    check(owner->source_construction_only_entry(),"wrong platform entry classification");
    check(owner->retail_root()==missing,"construction rewrote root identity");
    check(!owner->session_attachment_claimed(),"platform construction attached a session");
    empty_backends(*owner);
    const std::array<std::uint32_t,8> addresses{0x4959c0,0x489790,0x489794,0x489798,0x48979c,0x4897ac,0x4897b0,0x4897b4};
    const auto words=selected(s);
    for(std::size_t i=0;i<words.size();++i)check(*words[i]==pe.at_word(addresses[i]),"selected PE owner mismatch");
    check_completion_pe(s,pe);
    // Physical descriptors contain pairs in PE. The modern order projection
    // must still be zero, before the reached loader constructs descriptors.
    for(std::uint32_t va=0x60f350;va<0x60f698;va+=4)check(pe.at_word(va)==0,"descriptor PE range ceased to be zero");
    for(const auto order:s.game_text.descriptor_order)check(order==0,"445D10 ran during construction");
    for(const auto offset:s.game_text.offsets)check(!offset,"text handles preloaded");
    check(s.game_text.remaining_bytes==pe.at_word(0x4835dc),"text budget PE mismatch");
    check(s.game_text.language==pe.at_word(0x612944),"text language PE mismatch");
    check(s.menu.requested_language==1,"existing requested-language initial value changed");
    rejects([&]{(void)owner->assets();},"absent AssetStore returned successfully");
    rejects([&]{(void)owner->audio_device();},"absent device returned successfully");
    rejects([&]{(void)owner->audio_stream_platform();},"absent stream returned successfully");
    rejects([&]{(void)owner->window_input(0,0);},"absent input owner returned successfully");
    for(std::size_t i=0;i<words.size();++i)*words[i]=0x91000000u+static_cast<std::uint32_t>(i);
    s.game_text.descriptor_order[0]=71;s.game_text.remaining_bytes=0x123;s.game_text.arena[19]='Q';
    s.scene_mode=0x31415926;s.gravity.factor=7;s.options.texture=19;
    write_later_completion_state(s);
    const auto after_source_writes=completion_values(s);
    auto retained=std::shared_ptr<RaceSessionState>(owner,&s);
    {
        FrontendState frontend(retained,owner);
        check(&frontend.world()==&s,"frontend copied world before session attachment");
        auto session=std::make_unique<RaceSession>(SourceColdAttachmentOnly{},owner);
        check(session->readiness()==RaceSessionReadiness::source_attachment_only,"attachment falsely published scene readiness");
        check(owner->session_attachment_claimed(),"session claim was not persisted on platform");
        check(session->shared_platform().get()==owner.get()&&&session->state()==&s,"attachment created another owner");
        check(!session->shared_state().owner_before(owner)&&!owner.owner_before(session->shared_state()),"world alias has another control block");
        check(&frontend.world()==&session->state(),"frontend/session identity diverged");
        check(completion_values(s)==after_source_writes,"attachment reset later source PE-owner writes");
        for(std::size_t i=0;i<words.size();++i)check(*words[i]==0x91000000u+i,"attachment reset a modified PE owner");
        empty_backends(*owner);
        rejects([&]{(void)session->assets();},"cold assets getter succeeded");
        rejects([&]{(void)session->models();},"cold model getter succeeded");
        rejects([&]{(void)session->scene_resources();},"cold scene getter succeeded");
        rejects([&]{(void)session->splines();},"cold spline getter succeeded");
        rejects([&]{(void)session->track_geometry();},"cold geometry getter succeeded");
        rejects([&]{(void)session->audio_device();},"cold audio getter succeeded");
        rejects([&]{(void)session->audio_stream_platform();},"cold stream getter succeeded");
        rejects([&]{(void)session->track_pmd();},"cold PMD getter succeeded");
        rejects([&]{(void)session->track_normal_view();},"cold normals getter succeeded");
        rejects([&]{(void)session->raw_texture_labels();},"cold texture getter succeeded");
        rejects([&]{(void)session->level_name();},"cold level getter returned an invented default");
        rejects([&]{(void)session->text(0);},"cold text getter succeeded");
        rejects([&]{(void)session->window();},"cold window getter returned an invented default");
        rejects([&]{(void)session->audio_view();},"cold audio view succeeded");
        rejects([&]{(void)session->reset_parameters();},"cold reset parameters succeeded");
        rejects([&]{(void)session->selected_vehicles();},"cold selected vehicles succeeded");
        rejects([&]{close_session_music(*session);},"cold cleanup swallowed backend admission failure");
        rejects([&]{RaceSession duplicate(SourceColdAttachmentOnly{},owner);},"simultaneous session attachment allowed");
        check(completion_values(s)==after_source_writes,"rejected attachment reset completed PE owners");
        std::atomic<unsigned> foreign_rejections{};
        std::thread foreign([&]{
            try{RaceSession other(SourceColdAttachmentOnly{},owner);}catch(const std::logic_error&){++foreign_rejections;}
            try{(void)owner->host_readiness();}catch(const std::logic_error&){++foreign_rejections;}
            try{(void)owner->assets();}catch(const std::logic_error&){++foreign_rejections;}
            try{owner->prepare_host_assets();}catch(const std::logic_error&){++foreign_rejections;}
            try{owner->prepare_host_audio();}catch(const std::logic_error&){++foreign_rejections;}
        });foreign.join();
        check(foreign_rejections==5,"foreign thread admission did not reject before host effects");
    }
    check(s.scene_mode==0x31415926&&s.gravity.factor==7&&s.options.texture==19,"source-only destruction mutated unrelated state");
    check(completion_values(s)==after_source_writes,"session destruction reset later source PE-owner writes");
    check(s.game_text.remaining_bytes==0x123&&s.game_text.arena[19]=='Q'&&s.game_text.descriptor_order[0]==71,"attach/destruction ran language reset");
    rejects([&]{RaceSession duplicate(SourceColdAttachmentOnly{},owner);},"session claim was reset by destruction");
    for(unsigned attempt=0;attempt<2;++attempt){
        rejects<std::exception>([&]{owner->prepare_host_assets();},"nonexistent assets silently prepared");
        empty_backends(*owner);
        check(completion_values(s)==after_source_writes,"failed host preparation reset completed PE owners");
        for(std::size_t i=0;i<words.size();++i)check(*words[i]==0x91000000u+i,"failed host preparation reset source owners");
    }
    rejects([&]{RaceSession absent(SourceColdAttachmentOnly{},{});},"null platform accepted");
    rejects<std::invalid_argument>([&]{RaceSessionPlatform invalid(SourceColdConstructionOnly{},missing,std::numeric_limits<float>::quiet_NaN());},"NaN host gain accepted");
    std::weak_ptr<RaceSessionPlatform> weak=owner;
    owner.reset();check(!weak.expired(),"world alias failed to retain actual platform");
    check(retained->game_text.arena[19]=='Q',"retained world alias invalid after session destruction");
    retained.reset();check(weak.expired(),"platform ownership cycle");
    // A genuinely new platform gets its own once initialization; reattaching
    // the prior platform did not. No native backend is prepared in this test.
    auto fresh=std::make_shared<RaceSessionPlatform>(SourceColdConstructionOnly{},missing,0.0f);
    const auto fresh_words=selected(fresh->state());
    for(std::size_t i=0;i<fresh_words.size();++i)check(*fresh_words[i]==pe.at_word(addresses[i]),"new platform missed once PE initialization");
    empty_backends(*fresh);
    check_completion_pe(fresh->state(),pe);
    std::cout<<"construction_attachment_receiving checks="<<checks<<" native_backends_prepared=0 scene_ready=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
