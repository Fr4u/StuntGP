#include "sgn/source_audio_enumeration.hpp"
#include "sgn/source_audio_enumeration_native.hpp"
#include <array>
#include <cstring>
#include <exception>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace sgn::retail;
using Guid=std::array<std::uint32_t,4>;
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Event {
    std::optional<Guid> guid;
    std::string description;
    bool operator==(const Event&)const=default;
};
struct Capture final:AudioEnumerationReceiver {
    std::vector<Event> events;
    bool more=true;
    bool visit(const std::optional<Guid>& guid,std::string_view text)override{
        events.push_back({guid,std::string(text)});return more;
    }
};
//Authored import seam only. Production has no mutable global entry/context.
struct Script {
    std::vector<Event> events;
    HRESULT result=S_OK;
    std::vector<BOOL> returns;
    bool ignore_stop=false;
    bool null_description=false;
    unsigned entries{};
};
thread_local Script* active_script=nullptr;
struct ScriptScope {
    Script* previous;
    explicit ScriptScope(Script& script):previous(active_script){active_script=&script;}
    ~ScriptScope(){active_script=previous;}
};
HRESULT WINAPI scripted_entry(LPDSENUMCALLBACKA callback,LPVOID context){
    auto& script=*active_script;++script.entries;
    for(const auto& event:script.events){
        GUID guid{};if(event.guid)std::memcpy(&guid,event.guid->data(),sizeof(guid));
        const auto accepted=callback(event.guid?&guid:nullptr,
            script.null_description?nullptr:event.description.c_str(),nullptr,context);
        script.returns.push_back(accepted);
        if(!accepted&&!script.ignore_stop)break;
    }
    return script.result;
}
struct Marker {int value;};
struct ThrowingCapture final:AudioEnumerationReceiver {
    unsigned visits{};
    bool visit(const std::optional<Guid>&,std::string_view)override{++visits;throw Marker{91};}
};
void abi_transcript_and_results(){
    Script s;const char bytes[]={static_cast<char>(0x81),static_cast<char>(0xe9),'z',0};
    s.events={{std::nullopt,bytes},{Guid{0x11223344,0x55667788,0x99aabbcc,0xddeeff00},"secondary"},{std::nullopt,"last"}};
    ScriptScope scope(s);Win32SourceAudioEnumeration bridge(&scripted_entry);
    for(const HRESULT result:{S_OK,S_FALSE,E_FAIL}){
        s.result=result;s.returns.clear();Capture capture;
        require(bridge.enumerate(capture)==static_cast<AudioResult>(result),"bridge changed actual HRESULT");
        require(capture.events==s.events&&s.returns==std::vector<BOOL>({TRUE,TRUE,TRUE}),"ABI GUID/ANSI/null transcript or callback return changed");
    }
    require(s.entries==3,"entry called more than once per enumeration");
}
void abi_stop_and_exception(){
    Script s;s.events={{std::nullopt,"one"},{Guid{1,2,3,4},"two"}};
    ScriptScope scope(s);Win32SourceAudioEnumeration bridge(&scripted_entry);
    Capture capture;capture.more=false;
    require(bridge.enumerate(capture)==0&&capture.events.size()==1&&s.returns==std::vector<BOOL>({FALSE}),"FALSE ignored or converted to failure");
    s.ignore_stop=true;s.returns.clear();ThrowingCapture throwing;
    bool caught=false;try{(void)bridge.enumerate(throwing);}catch(const Marker& e){caught=e.value==91;}
    require(caught&&throwing.visits==1&&s.returns==std::vector<BOOL>({FALSE,FALSE}),"exception escaped ABI or later callback reentered receiver");
    //A subsequent invocation owns a fresh stack context, with no stopped/error
    //state retained by the preceding invocation.
    s.ignore_stop=false;s.returns.clear();Capture fresh;
    require(bridge.enumerate(fresh)==0&&fresh.events==s.events,"callback context leaked across calls");
}
void abi_source_prefix_and_null(){
    Script s;s.events={{Guid{11,22,33,44},"prefix"}};
    ScriptScope scope(s);Win32SourceAudioEnumeration bridge(&scripted_entry);
    AudioEnumerationState state;state.records.fill(0xcc);AudioListenerId listener{99};
    s.result=E_FAIL;
    const auto failed=enumerate_source_audio_stage_46210e(state,listener,bridge);
    require(std::get<SourceAudioEnumerationFailure>(failed).result==static_cast<AudioResult>(E_FAIL),"native nonzero result lost");
    require(state.cursor_word==11&&listener.value==22&&state.count==33&&state.adjacent_word==44&&state.records[0]=='p',"native failure lost source callback prefix");
    s.result=S_OK;s.null_description=true;s.returns.clear();s.events[0].guid=Guid{111,222,333,444};
    const auto records=state.records;bool caught=false;
    try{(void)enumerate_source_audio_stage_46210e(state,listener,bridge);}
    catch(const SourceAudioEnumerationError& e){caught=e.source_site==0x462062;}
    require(caught&&s.returns==std::vector<BOOL>({FALSE}),"null callback description not stopped at scan");
    require(state.cursor_word==111&&listener.value==222&&state.count==333&&state.adjacent_word==444&&state.records==records,"ANSI bridge scanned before publishing source GUID");
    s.null_description=false;s.events[0].guid=std::nullopt;s.events[0].description="ok";
    const auto success=enumerate_source_audio_stage_46210e(state,listener,bridge);
    require(std::holds_alternative<SourceAudioDefaultDeviceBoundary>(success),"native success missing typed continuation");
}
void abi_null_entry(){
    bool caught=false;try{Win32SourceAudioEnumeration absent(nullptr);}
    catch(const SourceAudioEnumerationError& e){caught=e.source_site==0x46210e;}
    require(caught,"null import entry accepted");
}
//Independent direct SDK observer for receiving on the actual machine. It does
//not call the adapter or its receiver, and retains no SDK-owned pointer.
struct DirectCapture {
    std::vector<Event> events;
    std::exception_ptr error;
    bool more=true;
};
BOOL CALLBACK direct_observer(LPGUID guid,LPCSTR description,LPCSTR module,LPVOID context) noexcept {
    (void)module;auto& out=*static_cast<DirectCapture*>(context);
    try{
        if(!description)throw std::runtime_error("SDK returned null description");
        Event event;if(guid){event.guid.emplace();
            const auto* bytes=reinterpret_cast<const unsigned char*>(guid);
            for(std::size_t i=0;i<4;++i){const auto j=i*4;
                (*event.guid)[i]=std::uint32_t(bytes[j])|(std::uint32_t(bytes[j+1])<<8)|
                    (std::uint32_t(bytes[j+2])<<16)|(std::uint32_t(bytes[j+3])<<24);}}
        for(const char* p=description;*p;++p)event.description.push_back(*p);
        out.events.push_back(std::move(event));return out.more?TRUE:FALSE;
    }catch(...){out.error=std::current_exception();return FALSE;}
}
struct DirectResult {HRESULT result;std::vector<Event> events;};
DirectResult direct(bool more){
    DirectCapture capture;capture.more=more;
    const auto result=::DirectSoundEnumerateA(&direct_observer,&capture);
    if(capture.error)std::rethrow_exception(capture.error);
    return {result,std::move(capture.events)};
}
bool actual_os(bool more){
    //One bracket only. Hardware changes are an explicit receiving failure, not
    //silently retried until a desired transcript appears.
    const auto before=direct(more);Capture capture;capture.more=more;
    Win32SourceAudioEnumeration bridge;const auto actual=bridge.enumerate(capture);
    const auto after=direct(more);
    require(before.result==after.result&&before.events==after.events,"OS inventory changed during bracket: receiving inconclusive, rerun only after diagnosis");
    require(actual==static_cast<AudioResult>(before.result)&&capture.events==before.events,"actual DirectSoundEnumerateA differs from independent SDK observer");
    if(!more)require(capture.events.size()<=1,"native FALSE did not stop enumeration");
    std::cout<<"actual OS enumeration more="<<more<<" HRESULT="<<actual<<" callbacks="<<capture.events.size()<<'\n';
    return !capture.events.empty();
}
}
int main(int argc,char** argv){try{
    require(argc==2,"expected --abi or --os");const std::string mode=argv[1];
    if(mode=="--abi"){
        abi_transcript_and_results();abi_stop_and_exception();abi_source_prefix_and_null();abi_null_entry();
        std::cout<<"source audio enumeration: 4 native ABI receiving groups PASS\n";
    }else if(mode=="--os"){
        const bool full=actual_os(true);const bool stopped=actual_os(false);
        if(!full||!stopped){std::cout<<"NO CALLBACK COVERAGE: actual HRESULTs matched, callback receiving SKIPPED\n";return 77;}
        std::cout<<"source audio enumeration: 2 actual OS transcript brackets PASS\n";
    }else throw std::runtime_error("unknown receiving mode");
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
