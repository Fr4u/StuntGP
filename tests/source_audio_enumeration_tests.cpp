#include "sgn/source_audio_enumeration.hpp"
#include <algorithm>
#include <array>
#include <functional>
#include <iostream>
#include <string>
#include <utility>

namespace {
using namespace sgn::retail;
using Guid=std::array<std::uint32_t,4>;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Platform final:AudioEnumerationPlatform {
    std::function<AudioResult(AudioEnumerationReceiver&)> action;
    explicit Platform(std::function<AudioResult(AudioEnumerationReceiver&)> f):action(std::move(f)){}
    AudioResult enumerate(AudioEnumerationReceiver& receiver)override{return action(receiver);}
};
AudioEnumerationState seeded(){
    AudioEnumerationState s;s.cursor_word=AudioEnumerationState::source_origin;s.count=77;s.adjacent_word=88;s.records.fill(0xcc);return s;
}
bool equal(const AudioEnumerationState& a,const AudioEnumerationState& b){
    return a.cursor_word==b.cursor_word&&a.count==b.count&&a.adjacent_word==b.adjacent_word&&a.records==b.records;
}
template<class F>void error_at(std::uint32_t site,F&& f){
    try{f();}catch(const SourceAudioEnumerationError& error){require(error.source_site==site,"wrong source error site");return;}
    throw std::runtime_error("expected source error missing");
}
void null_guid_ansi_tail(){
    auto s=seeded();AudioListenerId l{0x11223344};const auto before=s;
    const char name[]={static_cast<char>(0x81),static_cast<char>(0xe9),'a','b','c','\0'};
    require(record_audio_enumeration_ansi(s,l,std::nullopt,name),"callback must continue");
    require(s.cursor_word==before.cursor_word&&s.count==77&&s.adjacent_word==88&&l.value==0x11223344,"null GUID changed context words");
    require(s.records[0]==0x81&&s.records[1]==0xe9&&s.records[5]==0&&s.records[6]==0xcc,"ANSI bytes/NUL/tail");
    record_audio_enumeration_ansi(s,l,std::nullopt,"x");
    require(s.records[0]=='x'&&s.records[1]==0&&s.records[2]=='a'&&s.records[4]=='c',"short callback cleared retained tail");
    record_audio_enumeration(s,l,std::nullopt,std::string_view("q\0discard",9));
    require(s.records[0]=='q'&&s.records[1]==0&&s.records[2]=='a',"embedded NUL did not terminate copy");
    record_audio_enumeration(s,l,std::nullopt,{});
    require(s.records[0]==0&&s.records[2]=='a',"empty implicit terminator");
}
void guid_write_order_and_overwrite(){
    auto s=seeded();AudioListenerId l{0xffffffffffffffffull};
    const Guid a{0xfedcba98,0x87654321,0x12345678,0x99aabbcc};
    record_audio_enumeration_ansi(s,l,a,"first");
    require(s.cursor_word==a[0]&&l.value==a[1]&&s.count==a[2]&&s.adjacent_word==a[3],"four context DWORDs");
    record_audio_enumeration_ansi(s,l,std::nullopt,"second");
    require(s.cursor_word==a[0]&&l.value==a[1]&&s.count==a[2]&&s.adjacent_word==a[3],"null GUID erased preceding context");
    const Guid b{1,2,3,4};record_audio_enumeration_ansi(s,l,b,"z");
    require(s.cursor_word==1&&l.value==2&&s.count==3&&s.adjacent_word==4,"callback did not overwrite same context");
    require(s.records[0]=='z'&&s.records[1]==0&&s.records[2]=='c'&&s.records[6]==0,"record did not overwrite same destination");
}
void signed_count(){
    //Literal signed-division cases include truncation toward zero, DWORD wrap,
    //and the signed extrema. Expected words are independent of host floats.
    constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,10> cases{{
        {0,0},{143,0},{144,1},{288,2},{0xffffffff,0},{0xffffff71,0},
        {0xffffff70,0xffffffff},{0xfffffee0,0xfffffffe},
        {0x7fffffff,14913080},{0x80000000,0xff1c71c8}}};
    for(const auto& [delta,expected]:cases){
        auto s=seeded();AudioListenerId l{321};const auto cursor=AudioEnumerationState::source_origin+delta;
        Platform p([&](AudioEnumerationReceiver& r){r.visit_ansi(Guid{cursor,654,0x77777777,987},"device");return AudioResult{0};});
        require(enumerate_audio_devices_46210e(s,l,p)==0,"zero HRESULT changed");
        require(s.count==expected&&s.cursor_word==cursor&&l.value==654&&s.adjacent_word==987,"signed source count/context mismatch");
    }
}
void nonzero_results_retain_prefix(){
    for(const AudioResult hr:{AudioResult{1},AudioResult{-1},AudioResult{-2147467259}}){
        auto s=seeded();AudioListenerId l{321};
        Platform p([&](AudioEnumerationReceiver& r){r.visit_ansi(Guid{0x111,0x222,0x333,0x444},"prefix");return hr;});
        const auto outcome=enumerate_source_audio_stage_46210e(s,l,p);
        require(std::holds_alternative<SourceAudioEnumerationFailure>(outcome),"nonzero HRESULT accepted");
        require(std::get<SourceAudioEnumerationFailure>(outcome).result==hr,"HRESULT changed");
        require(s.cursor_word==0x111&&l.value==0x222&&s.count==0x333&&s.adjacent_word==0x444&&s.records[0]=='p',"failure lost callback prefix or published count");
    }
    auto s=seeded();const auto before=s;AudioListenerId l{98};
    Platform p([](AudioEnumerationReceiver&){return AudioResult{1};});
    require(enumerate_audio_devices_46210e(s,l,p)==1&&equal(s,before)&&l.value==98,"empty failure reset owner");
}
struct CallbackFailure {int token;};
void exception_retains_prefix(){
    auto s=seeded();AudioListenerId l{321};
    Platform p([](AudioEnumerationReceiver& r)->AudioResult{r.visit_ansi(Guid{1,2,3,4},"kept");throw CallbackFailure{73};});
    bool caught=false;try{(void)enumerate_source_audio_stage_46210e(s,l,p);}catch(const CallbackFailure& e){caught=e.token==73;}
    require(caught&&s.cursor_word==1&&l.value==2&&s.count==3&&s.adjacent_word==4&&s.records[0]=='k',"exception became success or lost prefix");
}
void null_description_retains_guid(){
    auto s=seeded();AudioListenerId l{321};const auto bytes=s.records;
    error_at(0x462062,[&]{record_audio_enumeration_ansi(s,l,Guid{11,22,33,44},nullptr);});
    require(s.cursor_word==11&&l.value==22&&s.count==33&&s.adjacent_word==44&&s.records==bytes,"GUID must precede invalid description scan");
    const auto before=s;
    error_at(0x462062,[&]{record_audio_enumeration_ansi(s,l,std::nullopt,nullptr);});
    require(equal(s,before)&&l.value==22,"null GUID/description mutated state");
}
void bounded_extent(){
    auto s=seeded();AudioListenerId l{321};
    record_audio_enumeration_ansi(s,l,std::nullopt,std::string(1151,'v').c_str());
    require(s.records.front()=='v'&&s.records[1150]=='v'&&s.records[1151]==0,"largest supported terminated record");
    for(const std::size_t length:{1152u,1153u,1154u,1155u,1200u}){
        s=seeded();const auto site=length<1155?0x46207du:0x462071u;
        const std::string name(length,'p');
        error_at(site,[&]{record_audio_enumeration_ansi(s,l,Guid{11,22,33,44},name.c_str());});
        require(std::all_of(s.records.begin(),s.records.end(),[](auto b){return b=='p';}),"overflow lost supported description prefix");
        require(s.cursor_word==11&&l.value==22&&s.count==33&&s.adjacent_word==44,"overflow lost GUID prefix");
    }
}
void continuation_boundary(){
    auto s=seeded();AudioListenerId l{321};
    Platform p([](AudioEnumerationReceiver& r){r.visit_ansi(std::nullopt,"default");return AudioResult{0};});
    const auto outcome=enumerate_source_audio_stage_46210e(s,l,p);
    require(std::holds_alternative<SourceAudioDefaultDeviceBoundary>(outcome)&&s.count==0&&l.value==321,"successful enum did not publish boundary/count");
    const auto before=s;
    error_at(0x462146,[&]{std::get<SourceAudioDefaultDeviceBoundary>(outcome).resume();});
    require(equal(s,before)&&l.value==321,"rejected continuation changed existing owners");
    //The generic preview helper has no typed cold-continuation rejection.
    require(enumerate_audio_devices_46210e(s,l,p)==0,"generic helper cannot return success");
}
}
int main(){try{
    null_guid_ansi_tail();guid_write_order_and_overwrite();signed_count();nonzero_results_retain_prefix();
    exception_retains_prefix();null_description_retains_guid();bounded_extent();continuation_boundary();
    std::cout<<"source audio enumeration: 8 core receiving groups PASS\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
