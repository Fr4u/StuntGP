#include "sgn/retail_audio_catalog_startup.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
using Record=std::array<unsigned,128>;using Event=std::array<unsigned,20>;
template<class T>void read(std::ifstream& in,T& value){if(!in.read(reinterpret_cast<char*>(&value),sizeof(value)))throw std::runtime_error("short creation corpus");}
Bytes read_bytes(std::ifstream& in,unsigned n){if(n>4*1024*1024)throw std::runtime_error("audio payload limit");Bytes out(n);if(!in.read(reinterpret_cast<char*>(out.data()),n))throw std::runtime_error("short creation payload");return out;}
std::array<unsigned,7> format_words(const AudioWaveFormat& f){return {f.tag,f.channels,f.samples_per_second,f.bytes_per_second,f.block_align,f.bits_per_sample,f.extra_bytes};}
AudioWaveFormat format(const Record& r){return {static_cast<std::uint16_t>(r[64]),static_cast<std::uint16_t>(r[65]),r[66],r[67],static_cast<std::uint16_t>(r[68]),static_cast<std::uint16_t>(r[69]),static_cast<std::uint16_t>(r[70])};}
struct Device final:AudioCreationDevice {
    const Record& r;std::vector<Event> events;Bytes buffer;
    explicit Device(const Record& input):r(input){if(input[32]>4*1024*1024)throw std::runtime_error("audio buffer limit");buffer.assign(input[32],0xcd);}
    AudioResult call(unsigned op,unsigned object,std::initializer_list<unsigned> params){Event e{};e[0]=op;e[1]=object;e[2]=r[80+op];e[3]=static_cast<unsigned>(params.size());std::copy(params.begin(),params.end(),e.begin()+4);events.push_back(e);return std::bit_cast<AudioResult>(e[2]);}
    AudioResult create_buffer(AudioDeviceId d,const AudioBufferRequest& request,AudioBufferId& out)override{
        auto hr=call(0,static_cast<unsigned>(d.value),{request.size,request.flags,request.bytes,request.reserved,request.format?1u:0u});
        if(request.format){auto f=format_words(*request.format);if(f[0]!=1)throw std::runtime_error("creation corpus expects PCM format");std::copy_n(f.begin(),6,events.back().begin()+9);events.back()[3]+=6;}
        if(hr==0||r[88])out.value=r[89];return hr;
    }
    AudioResult query_spatial_buffer(AudioBufferId b,AudioSpatialId& out)override{auto hr=call(1,static_cast<unsigned>(b.value),{r[90]});if(hr==0||r[88])out.value=r[90];return hr;}
    AudioResult lock_buffer(AudioBufferId b,unsigned offset,unsigned count,AudioBufferMemory& memory,unsigned flags)override{
        auto hr=call(2,static_cast<unsigned>(b.value),{offset,count,flags});if(hr==0||r[88]){memory.first=buffer;memory.second={};}return hr;
    }
    AudioResult unlock_buffer(AudioBufferId b,std::span<std::uint8_t> first,std::span<std::uint8_t> second)override{
        if(first.data()!=buffer.data())throw std::runtime_error("unlock wrong region");return call(3,static_cast<unsigned>(b.value),{static_cast<unsigned>(first.size()),static_cast<unsigned>(second.size())});
    }
    AudioResult capabilities(AudioDeviceId d,std::span<unsigned,24> caps)override{auto hr=call(4,static_cast<unsigned>(d.value),{caps[0]});if(hr==0||r[88])std::copy_n(r.begin()+40,24,caps.begin());return hr;}
    AudioResult primary_format(AudioPrimaryId p,AudioWaveFormat& f,unsigned count,unsigned& written)override{
        auto hr=call(5,static_cast<unsigned>(p.value),{count});if(hr==0||r[88]){f=format(r);written=r[71];}return hr;
    }
    AudioResult set_primary_format(AudioPrimaryId p,const AudioWaveFormat& f)override{auto hr=call(6,static_cast<unsigned>(p.value),{});auto values=format_words(f);std::copy(values.begin(),values.end(),events.back().begin()+4);events.back()[3]=7;return hr;}
};
}
int main(int argc,char** argv){try{
    if(argc!=3)throw std::runtime_error("expected audio creation corpus and asset root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);
    if(std::string(magic.data(),8)!="SGNACR01")throw std::runtime_error("audio creation schema");unsigned count;read(in,count);if(!count||count>10000)throw std::runtime_error("audio creation count");AssetStore assets(argv[2]);
    std::uint64_t words=0,event_count=0,payload_bytes=0;unsigned errors=0;
    for(unsigned n=0;n<count;++n){unsigned op,name_size;read(in,op);read(in,name_size);if(op>3||name_size>127)throw std::runtime_error("audio creation operation/name");
        const auto name_bytes=read_bytes(in,name_size);const std::string name(name_bytes.begin(),name_bytes.end());if(name.find('\0')!=std::string::npos)throw std::runtime_error("embedded audio name terminator");
        Record r,expected;read(in,r);read(in,expected);unsigned events;read(in,events);if(events>16)throw std::runtime_error("audio creation event limit");std::vector<Event> wanted(events);for(auto& e:wanted)read(in,e);
        unsigned bytes;read(in,bytes);auto wanted_payload=read_bytes(in,bytes);Device device(r);
        AudioSample sample{{r[0]},{r[1]},r[2],r[5]};AudioSampleLoadFields fields{r[3],r[4],r[6],r[7]};AudioPrimaryId primary{r[8]};unsigned capability=r[9],enabled=r[10];bool result;
        if(op==0||op==1)result=create_audio_sample_buffer(sample,fields,{r[12]},device,op==0,r[13]);
        else if(op==2)result=initialize_audio_primary({r[12]},primary,device,r[11],capability,enabled);
        else result=load_audio_sample(sample,fields,{r[12]},device,assets,name,r[10],r[13]);
        r[0]=static_cast<unsigned>(sample.buffer.value);r[1]=static_cast<unsigned>(sample.spatial.value);r[2]=sample.type;r[3]=fields.ready;r[4]=fields.bytes;r[5]=sample.frequency;r[6]=fields.bits;r[7]=fields.channels;
        r[8]=static_cast<unsigned>(primary.value);r[9]=capability;r[10]=enabled;r[14]=result;
        for(unsigned j=0;j<r.size();++j){++words;if(r[j]!=expected[j]&&errors++<16)std::cerr<<"case "<<n<<" op "<<op<<" word "<<j<<" actual "<<std::hex<<r[j]<<" expected "<<expected[j]<<std::dec<<'\n';}
        if(device.events!=wanted&&errors++<16){std::cerr<<"creation event mismatch case "<<n<<'\n';for(unsigned e=0;e<device.events.size()&&e<wanted.size();++e)for(unsigned j=0;j<20;++j)if(device.events[e][j]!=wanted[e][j])std::cerr<<"event "<<e<<" word "<<j<<" actual "<<std::hex<<device.events[e][j]<<" expected "<<wanted[e][j]<<std::dec<<'\n';}
        if(device.buffer!=wanted_payload&&errors++<16)std::cerr<<"creation payload mismatch case "<<n<<'\n';
        event_count+=device.events.size();payload_bytes+=device.buffer.size();
    }
    if(in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("trailing creation corpus");
    std::cout<<"{\"passed\":"<<(errors?"false":"true")<<",\"cases\":"<<count<<",\"compared_words\":"<<words<<",\"device_events\":"<<event_count<<",\"payload_bytes\":"<<payload_bytes<<",\"mismatches\":"<<errors<<",\"allowed_ulp\":0}\n";return errors?1:0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 2;}}
