#include "audio_oracle_state.hpp"
#include "sgn/retail_audio_maintenance.hpp"
namespace {
using Frame=std::array<std::uint32_t,704>;
void transfer_extra(Frame& r,AudioMaintenanceState& s,LevelAudioState& level,CameraPresentationState& p,bool loading) {
    auto u=[&](std::uint32_t& x,unsigned i){if(loading)x=r[i];else r[i]=x;};
    auto si=[&](std::int32_t& x,unsigned i){if(loading)x=std::bit_cast<std::int32_t>(r[i]);else r[i]=std::bit_cast<std::uint32_t>(x);};
    auto v=[&](CameraVector& x,unsigned i){
        if(loading)x={real(r[i]),real(r[i+1]),real(r[i+2]),real(r[i+3])};
        else {r[i]=bits(x.x);r[i+1]=bits(x.y);r[i+2]=bits(x.z);r[i+3]=bits(x.w);}};
    if(loading){s.listener={r[640]};s.music.buffer={r[641]};}else {r[640]=id(s.listener);r[641]=id(s.music.buffer.borrowed());}
    auto& m=s.music;u(m.requested_volume,642);u(m.gain,643);u(m.gain_step,644);si(m.pan,645);si(m.cached_volume,646);si(m.cached_pan,647);
    u(level.output_a,648);u(level.output_b_copy,649);v(p.up_axis,650);v(p.forward_axis,654);
}
}
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("expected audio maintenance corpus");
    std::ifstream f(argv[1],std::ios::binary);std::array<char,8> magic{};read(f,magic);
    if(std::string_view(magic.data(),8)!="SGNAMT01")throw std::runtime_error("bad audio maintenance magic");
    std::uint32_t count{};read(f,count);if(count==0||count>20000)throw std::runtime_error("bad audio maintenance count");
    std::size_t mismatches{},words{},events{};
    for(std::uint32_t n=0;n<count;++n) {
        std::uint32_t op{};read(f,op);if(op>6)throw std::runtime_error("invalid audio maintenance operation");
        Frame r{},expected{};read(f,r);read(f,expected);std::uint32_t event_count{};read(f,event_count);
        if(event_count>512)throw std::runtime_error("audio maintenance event count limit");
        std::vector<Event> expected_trace(event_count);for(auto& e:expected_trace)read(f,e);
        Record base{};std::copy_n(r.begin(),640,base.begin());Device device(base);
        AudioDriverState driver_state;LevelAudioState level;contact::player::Controls controls;transfer(base,driver_state,level,controls,true);
        AudioMaintenanceState state;CameraPresentationState presentation;transfer_extra(r,state,level,presentation,true);
        AudioDriver d{driver_state,device,level,controls,base[8]};
        switch(op){
        case 0:audio_recycle_duplicates(d);break;case 1:audio_recount_playing(d);break;
        case 2:audio_update_listener(d,state.listener,controls.controllers[0],presentation);break;
        case 3:audio_apply_music_levels(d,state.music,std::bit_cast<std::int32_t>(r[658]),r[659]);break;
        case 4:audio_advance_music_gain(d,state.music);break;case 5:audio_advance_sound_gain(level);break;
        case 6:audio_maintain({d,level,state,presentation});break;
        default:throw std::runtime_error("unreachable audio maintenance operation");}
        transfer(base,driver_state,level,controls,false);std::copy(base.begin(),base.end(),r.begin());transfer_extra(r,state,level,presentation,false);
        for(std::size_t i=0;i<r.size();++i){++words;if(r[i]!=expected[i]) {
            if(mismatches<15)std::cerr<<"case="<<n<<" op="<<op<<" word="<<i<<" actual="<<std::hex<<r[i]<<" expected="<<expected[i]<<std::dec<<'\n';++mismatches;}}
        if(device.trace!=expected_trace) {if(mismatches<15){std::cerr<<"case="<<n<<" op="<<op<<" trace native="<<device.trace.size()<<" source="<<expected_trace.size()<<'\n';
            for(std::size_t j=0;j<std::min(device.trace.size(),expected_trace.size());++j)if(device.trace[j]!=expected_trace[j]) {
                for(auto x:device.trace[j])std::cerr<<std::hex<<x<<',';std::cerr<<" vs ";for(auto x:expected_trace[j])std::cerr<<x<<',';std::cerr<<std::dec<<'\n';break;}}
            ++mismatches;}
        events+=device.trace.size();
    }
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing audio maintenance data");
    std::cout<<"cases="<<count<<" words="<<words<<" device_events="<<events<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"audio maintenance oracle: "<<e.what()<<'\n';return 2;}
