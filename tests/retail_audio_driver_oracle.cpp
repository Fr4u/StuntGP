#include "audio_oracle_state.hpp"
int main(int argc,char** argv)try {
    if(argc!=2)throw std::runtime_error("expected audio driver corpus");
    std::ifstream f(argv[1],std::ios::binary);std::array<char,8> magic{};read(f,magic);
    if(std::string_view(magic.data(),8)!="SGNAUD01")throw std::runtime_error("bad audio corpus magic");
    std::uint32_t count{};read(f,count);if(count==0||count>20000)throw std::runtime_error("bad audio case count");
    std::size_t mismatches{},words{},events{};
    for(std::uint32_t n=0;n<count;++n) {
        std::uint32_t op{};read(f,op);if(op>9)throw std::runtime_error("invalid audio operation");
        Record r{},expected{};read(f,r);read(f,expected);std::uint32_t event_count{};read(f,event_count);
        if(event_count>256)throw std::runtime_error("audio event count limit");
        std::vector<Event> expected_trace(event_count);for(auto& e:expected_trace)read(f,e);
        AudioDriverState state;LevelAudioState level;contact::player::Controls controls;Device device(r);
        transfer(r,state,level,controls,true);AudioDriver d{state,device,level,controls,r[8]};
        auto& h=r[9];AudioBufferId b{r[10]};AudioSpatialId s{r[11]};
        const auto slot=r[608],flags=r[609],frequency=r[610],volume=r[611],force=r[613];
        const auto pan=std::bit_cast<std::int32_t>(r[612]);auto* hp=r[614]?&h:nullptr;
        const Vec3 p{real(r[616]),real(r[617]),real(r[618])},v{real(r[619]),real(r[620]),real(r[621])};
        const auto* vp=r[615]?&v:nullptr;
        switch(op){
        case 0:audio_set_range(state,slot);break;
        case 1:audio_stop_slot(d,slot);break;
        case 2:audio_stop_handle(d,h);break;
        case 3:audio_stop_all(d);break;
        case 4:r[12]=audio_duplicate_2d(d,b,slot,hp)?1u:0u;break;
        case 5:r[12]=audio_duplicate_3d(d,b,s,slot,hp)?1u:0u;break;
        case 6:r[12]=audio_listener(d,p);break;
        case 7:audio_start(d,slot,flags,frequency,volume,pan,force,hp);break;
        case 8:audio_start_positioned(d,slot,flags,frequency,volume,p,vp,force,hp);break;
        case 9:audio_update_positioned(d,h,frequency,volume,p,vp);break;
        default:throw std::runtime_error("unreachable audio operation");
        }
        r[10]=id(b);r[11]=id(s);transfer(r,state,level,controls,false);
        for(std::size_t i=0;i<r.size();++i){++words;if(r[i]!=expected[i]){
            if(mismatches<12)std::cerr<<"case="<<n<<" op="<<op<<" word="<<i<<" actual="<<std::hex<<r[i]<<" expected="<<expected[i]<<std::dec<<'\n';++mismatches;}}
        if(device.trace!=expected_trace){if(mismatches<12){std::cerr<<"case="<<n<<" op="<<op<<" event trace differs native="<<device.trace.size()<<" source="<<expected_trace.size()<<'\n';
            for(std::size_t j=0;j<std::min(device.trace.size(),expected_trace.size());++j)if(device.trace[j]!=expected_trace[j]) {
                for(auto x:device.trace[j])std::cerr<<std::hex<<x<<',';std::cerr<<" vs ";for(auto x:expected_trace[j])std::cerr<<x<<',';std::cerr<<std::dec<<'\n';break;}}
            ++mismatches;}
        events+=device.trace.size();
    }
    if(f.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing audio corpus data");
    std::cout<<"cases="<<count<<" words="<<words<<" device_events="<<events<<" mismatches="<<mismatches<<" tolerance_ulp=0\n";
    return mismatches?1:0;
}catch(const std::exception& e){std::cerr<<"audio oracle: "<<e.what()<<'\n';return 2;}
