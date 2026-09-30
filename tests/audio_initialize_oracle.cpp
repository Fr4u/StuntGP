#include "audio_initialize_state.hpp"
using namespace sgn::test::audio_initialize;
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected initialization corpus and asset root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);
    if(std::string(magic.data(),8)!="SGNAIN01")throw std::runtime_error("audio initialization schema");unsigned count;read(in,count);if(!count||count>2048)throw std::runtime_error("audio initialization count");AssetStore assets(argv[2]);
    unsigned errors=0;std::uint64_t compared=0,events=0,payload_bytes=0,ticks=0;
    for(unsigned n=0;n<count;++n){unsigned steps;read(in,steps);if(!steps||steps>4)throw std::runtime_error("audio initialization step count");Record actual;read(in,actual);
        AudioDriverState state;AudioInitializationState initialization;AudioMaintenanceState maintenance;AudioStreamState stream;LevelAudioState level;LevelOverrideOptions options;contact::player::Controls controls;
        transfer(actual,state,initialization,maintenance,stream,level,options,true);Device device(actual);unsigned split=actual[0x1a88/4];AudioDriver driver{state,device,level,controls,split};
        AudioStartupWorld world{driver,maintenance,stream,initialization,level,options,assets,device,device,actual[0x1a8c/4]};
        for(unsigned step=0;step<steps;++step){unsigned op;read(in,op);if(op>3)throw std::runtime_error("audio initialization operation");Record expected;read(in,expected);unsigned event_count;read(in,event_count);if(event_count>2000)throw std::runtime_error("audio initialization event limit");std::vector<Event> wanted(event_count);for(auto& e:wanted)read(in,e);
            device.expected=wanted;device.cursor=0;
            if(op==0)actual[0x1a90/4]=initialize_audio(world);else if(op==1)shutdown_audio(world);else if(op==2)reconfigure_audio_for_split(world);
            else actual[0x1a90/4]=load_audio_samples(state,initialization.sample_fields,initialization.descriptor_types,initialization.device,device,assets,initialization.spatial_enabled);
            transfer(actual,state,initialization,maintenance,stream,level,options,false);
            for(unsigned j=0;j<actual.size();++j){++compared;if(actual[j]!=expected[j]&&errors++<12)std::cerr<<"case "<<n<<" step "<<step<<" op "<<op<<" offset "<<std::hex<<j*4<<" actual "<<actual[j]<<" expected "<<expected[j]<<std::dec<<'\n';}
            if(device.cursor!=wanted.size())throw std::runtime_error("missing initialization OS calls");events+=device.cursor;++ticks;
            unsigned buffers;read(in,buffers);if(buffers>384||buffers!=device.buffers.size())throw std::runtime_error("initialization created buffer count");
            for(const auto& [expected_id,expected_payload]:device.buffers){unsigned id,size;read(in,id);read(in,size);
                if(id!=expected_id)throw std::runtime_error("initialization buffer identities must be complete, unique and ordered");
                const auto payload=bytes(in,size);if(expected_payload!=payload&&errors++<12)std::cerr<<"buffer payload mismatch "<<n<<'\n';payload_bytes+=payload.size();}
        }
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing initialization corpus");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"calls\":"<<ticks<<",\"compared_words\":"<<compared<<",\"device_events\":"<<events<<",\"payload_bytes\":"<<payload_bytes<<",\"mismatches\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
