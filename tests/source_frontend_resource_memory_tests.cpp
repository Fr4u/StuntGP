#include "sgn/source_frontend_resource_memory.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

using namespace sgn::retail;
namespace {
using Word=std::uint32_t;
using Bytes=std::vector<std::uint8_t>;
using Kind=SourceFrontendResourceEventKind;
std::size_t assertions{};
void require(bool value,const char* message){++assertions;if(!value)throw std::runtime_error(message);}
template<class Error=SourceFrontendResourceError,class F>void rejects(F&& operation,Word site){
    bool caught=false;try{operation();}catch(const Error& error){caught=true;require(error.source_site==site,"wrong failure frontier");}
    require(caught,"expected rejection did not happen");
}
Bytes bytes(std::string_view text){return {text.begin(),text.end()};}
void root(SourceBootConfigState& config,std::string_view text){
    require(text.size()<config.base_path.size(),"fixture root bounds");
    std::copy(text.begin(),text.end(),config.base_path.begin());config.base_path[text.size()]=0;
}
struct Files final:SourceBootFilePlatform {
    std::map<std::string,Bytes> contents;
    std::map<std::string,Word> sizes,returns;
    std::map<std::string,std::size_t> limits;
    std::vector<std::string> calls;
    std::vector<Word> allocations;
    bool allocation_fails{};
    std::string throw_read;
    std::function<void(std::string_view)> before_read;
    Word size_loose_44af60(std::string_view name) override{
        const std::string key(name);calls.push_back("S:"+key);
        if(sizes.contains(key))return sizes.at(key);
        return contents.contains(key)?static_cast<Word>(contents.at(key).size()):0;
    }
    Word read_loose_44afa0(std::string_view name,Word requested,std::span<std::uint8_t> output,std::size_t& copied) override{
        const std::string key(name);calls.push_back("R:"+key+":"+std::to_string(requested));copied=0;
        if(before_read)before_read(name);
        if(contents.contains(key)){
            const auto& payload=contents.at(key);
            auto count=std::min(static_cast<std::size_t>(requested),payload.size());
            if(limits.contains(key))count=std::min(count,limits.at(key));
            for(std::size_t i=0;i<count;++i){
                if(i>=output.size())throw SourceBootConfigError(0x44afccu,"fixture requested inaccessible output");
                output[i]=payload[i];copied=i+1;
            }
        }
        if(key==throw_read)throw SourceBootConfigError(0x44afccu,"fixture read threw after actual stores");
        return returns.contains(key)?returns.at(key):static_cast<Word>(copied);
    }
    SourceBootRawAllocation allocate_wad_raw(Word count) override{
        calls.push_back("M:"+std::to_string(count));allocations.push_back(count);
        if(allocation_fails)return {};
        auto* value=static_cast<std::uint8_t*>(std::malloc(count));
        if(!value)throw std::bad_alloc();
        std::memset(value,0xcd,count); //Fixture poison, never source initialization.
        return SourceBootRawAllocation(value);
    }
};
struct Events {
    std::array<SourceFrontendResourceEvent,128> values{};
    std::size_t count{};
    SourceFrontendResourceMemory* owner{};
    bool try_reentry{},reentry_rejected{},reentry_wrong{};
    static void observe(void* context,const SourceFrontendResourceEvent& event) noexcept{
        auto& self=*static_cast<Events*>(context);
        if(self.count==self.values.size())std::terminate();
        self.values[self.count++]=event;
        if(self.try_reentry&&event.kind==Kind::allocated){
            try{self.owner->dispose_440c20();self.reentry_wrong=true;}
            catch(const SourceFrontendResourceError& e){self.reentry_rejected=e.source_site==0x440c20u;}
            catch(...){self.reentry_wrong=true;}
        }
    }
    SourceFrontendResourceObserver view(){return {this,&observe};}
    std::size_t number(Kind kind) const{
        return static_cast<std::size_t>(std::count_if(values.begin(),values.begin()+static_cast<std::ptrdiff_t>(count),
            [=](const auto& value){return value.kind==kind;}));
    }
};
struct Fixture {
    SourceBootConfigState config;
    Word current{};
    Events events;
    Files files;
    SourceFrontendResourceMemory memory;
    const std::string path="X:\\Root\\Custom\\catalog.csv";
    Fixture():memory(config,current,events.view()){
        root(config,"X:\\Root");events.owner=&memory;files.contents[path]=bytes("ABCDEF");
    }
    SourceFrontendResourceRef load(){return memory.load_43c7f0(files,"Custom\\catalog.csv");}
};
void success_reload_and_current_disposal(){
    Fixture f;f.config.display_width=991;f.config.files.searched_name[250]='T';
    const auto a=f.load();
    require(f.current==0&&f.memory.live_allocations()==1,"loader must not publish");
    const auto ai=f.memory.inspect(a);require(ai.payload_bytes==6&&ai.allocation_bytes==7&&ai.initialized_prefix==6,"exact allocation and read extent");
    require(ai.terminator_written&&ai.loader_returned&&ai.read_return==6u,"complete load facts");
    require(f.memory.complete_input(a,1).bytes.size()==7&&f.memory.read_byte(a,6,2)==0,"source final NUL");
    f.memory.publish_440b26(a);
    const auto b=f.load();const auto bi=f.memory.inspect(b);
    require(f.current==a.identity()&&f.memory.current_reference()==a,"second loader overwrote source word");
    require(a.identity()!=b.identity()&&ai.native_address!=bi.native_address,"successful reload reused live identity/allocation");
    f.memory.publish_440b26(b);
    require(f.memory.live_allocations()==2&&f.memory.read_byte(a,0,3)=='A',"reload released predecessor");
    const auto event_at=f.events.count;f.memory.dispose_440c20();
    require(f.current==0&&f.memory.live_allocations()==1,"source free must select current only");
    require(f.events.count==event_at+3,"source disposal events");
    const auto& before=f.events.values[event_at];const auto& after=f.events.values[event_at+1];const auto& clear=f.events.values[event_at+2];
    require(before.kind==Kind::before_source_free&&after.kind==Kind::after_source_free&&clear.kind==Kind::source_null,"free/null ordering");
    require(before.source_identity==b.identity()&&after.source_identity==b.identity()&&clear.source_identity==0,"source null published before raw free");
    require(before.resource.native_address==bi.native_address&&after.resource.native_address==bi.native_address,"wrong raw pointer freed");
    rejects([&]{(void)f.memory.read_byte(b,0,0x440d38u);},0x440d38u);
    require(f.memory.read_byte(a,5,4)=='F',"previous raw input changed after current free");
    const auto c=f.load();require(c.identity()>b.identity(),"identity recycled after free");
    f.memory.publish_440b26(c);f.memory.publish_440b26({});
    require(f.current==0&&f.memory.live_allocations()==2,"null publication freed predecessor");
    const auto unchanged=f.events.count;f.memory.dispose_440c20();require(f.events.count==unchanged,"null source disposer was not a no-op");
    require(f.config.display_width==991&&f.config.files.searched_name[250]=='T',"unrelated source words changed");
    require(f.files.calls==std::vector<std::string>{"S:"+f.path,"M:7","R:"+f.path+":6","S:"+f.path,"M:7","R:"+f.path+":6","S:"+f.path,"M:7","R:"+f.path+":6"},"loader call path/order/count");
}
void signed_size_and_allocator_frontiers(){
    for(const Word n:{0u,0xffffffffu,0x80000000u}){
        Fixture f;const auto old=f.load();f.memory.publish_440b26(old);f.files.calls.clear();f.files.allocations.clear();f.files.sizes[f.path]=n;
        const auto empty=f.load();
        require(!empty&&f.memory.last_attempt().size_return==n&&f.memory.last_attempt().loader_returned,"signed size gate");
        require(f.files.allocations.empty()&&f.files.calls==std::vector<std::string>{"S:"+f.path},"signed nonpositive size reached allocate/read");
        require(f.current==old.identity()&&f.memory.live_allocations()==1,"null load changed current allocation");
        f.memory.publish_440b26(empty);require(f.current==0&&f.memory.live_allocations()==1,"null source publication");
    }
    {Fixture f;f.files.sizes[f.path]=0x7fffffffu;f.files.allocation_fails=true;
     rejects([&]{(void)f.load();},0x43c83bu);
     require(f.files.allocations==std::vector<Word>{0x80000000u},"signed positive maximum n+1");
     require(f.current==0&&!f.memory.last_attempt().resource&&!f.memory.last_attempt().loader_returned&&f.memory.live_allocations()==0,"malloc failure minted success");}
    {Fixture f;const auto old=f.load();f.memory.publish_440b26(old);f.files.allocation_fails=true;
     rejects([&]{(void)f.load();},0x43c83bu);
     require(f.current==old.identity()&&f.memory.read_byte(old,0,1)=='A',"malloc failure rolled back prior publication");}
}
void short_read_and_actual_read_checks(){
    Fixture f;f.files.limits[f.path]=3;f.files.returns[f.path]=0xabcdu;
    const auto r=f.load();f.memory.publish_440b26(r);const auto info=f.memory.inspect(r);
    require(info.initialized_prefix==3&&info.read_return==0xabcdu&&info.payload_bytes==6&&info.terminator_written,"read EAX was mistaken for initialization");
    require(f.memory.read_byte(r,2,1)=='C'&&f.memory.read_byte(r,6,2)==0,"initialized prefix or separate NUL unavailable");
    rejects([&]{(void)f.memory.read_byte(r,3,0x43c696u);},0x43c696u);
    rejects([&]{(void)f.memory.read_byte(r,7,0x440d38u);},0x440d38u);
    rejects([&]{(void)f.memory.complete_input(r,0x440b46u);},0x440b46u);
    const auto* physical=reinterpret_cast<const std::uint8_t*>(info.native_address); //Safe only because fixture explicitly poisoned every byte.
    require(physical[3]==0xcd&&physical[4]==0xcd&&physical[5]==0xcd&&physical[6]==0,"short gap was zeroed or terminator followed read return");
    require(f.memory.initialized_prefix(r).size()==3&&f.current==r.identity(),"short result rejected before source publication");
    f.files.contents[f.path]=Bytes{'Q',0,'Z','D','E','F'};
    const auto embedded=f.load();f.memory.publish_440b26(embedded);
    Word at=0;while(f.memory.read_byte(embedded,at,0x43c696u)!=0)++at;
    require(at==1,"an initialized embedded NUL cannot require reading the later gap");
    f.files.limits[f.path]=0;f.files.returns[f.path]=0;
    const auto none=f.load();require(f.memory.read_byte(none,6,1)==0,"normal zero-byte read still reaches NUL[n]");
    rejects([&]{(void)f.memory.read_byte(none,0,0x43c696u);},0x43c696u);
}
void read_exception_retains_ownership_and_stores(){
    Fixture f;const auto prior=f.load();f.memory.publish_440b26(prior);
    f.files.limits[f.path]=3;f.files.throw_read=f.path;
    f.files.before_read=[&](std::string_view){f.config.files.searched_name[0]='W';f.config.display_height=31337;};
    rejects<SourceBootConfigError>([&]{(void)f.load();},0x44afccu);
    const auto attempt=f.memory.last_attempt();const auto info=f.memory.inspect(attempt.resource);
    require(attempt.resource&&attempt.size_return==6u&&!attempt.loader_returned,"partial failure attempt facts");
    require(info.initialized_prefix==3&&!info.terminator_written&&!info.loader_returned&&!info.read_return,"exception fabricated normal read/NUL");
    require(f.current==prior.identity()&&f.memory.live_allocations()==2,"read exception freed allocation or published incomplete load");
    require(f.memory.read_byte(attempt.resource,2,1)=='C',"actual prefix lost across exception");
    rejects([&]{f.memory.publish_440b26(attempt.resource);},0x440b26u);
    rejects([&]{(void)f.memory.read_byte(attempt.resource,6,0x440d38u);},0x440d38u);
    require(f.config.files.searched_name[0]=='W'&&f.config.display_height==31337,"earlier source writes rolled back");
    require(reinterpret_cast<const std::uint8_t*>(info.native_address)[6]==0xcd,"exception wrote source NUL");
    f.memory.dispose_440c20();require(f.memory.live_allocations()==1,"source disposer freed unpublished failed allocation");
}
void put(Bytes& data,std::size_t at,Word word){for(unsigned n=0;n<4;++n)data.at(at+n)=static_cast<std::uint8_t>(word>>(n*8u));}
//Independent DIR fixture: metadata is before payload so a short WAD prefix can
//complete lookup and then reject the first whole DWORD read44B354.
Bytes archive(std::string_view name,std::string_view payload,Word& payload_offset){
    constexpr Word table=12,node=4100;
    payload_offset=table+node+12+static_cast<Word>(name.size())+1;
    Bytes result(static_cast<std::size_t>(payload_offset)+payload.size());
    put(result,0,0x1a524944);put(result,4,static_cast<Word>(result.size()));put(result,8,table);
    Word hash=0;for(const unsigned char c:name)hash=((((hash&511u)<<1u)|(hash>>9u))+c)&1023u;
    put(result,table+4+hash*4,node);put(result,table+node+4,payload_offset);put(result,table+node+8,static_cast<Word>(payload.size()));
    std::copy(name.begin(),name.end(),result.begin()+table+node+12);
    std::copy(payload.begin(),payload.end(),result.begin()+payload_offset);return result;
}
void wad_return_copy_and_partial_exception(){
    {Fixture f;Word offset{};const std::string wad="X:\\Root\\wads\\setup.wad",path="X:\\Root\\Setup\\languagePC.csv";
     f.files.contents[wad]=archive("setup\\languagepc.csv","PAYLOAD",offset);
     const auto r=f.memory.load_43c7f0(f.files,"Setup\\languagePC.csv");
     require(f.memory.inspect(r).payload_bytes==7&&f.memory.inspect(r).read_return==7u&&f.memory.read_byte(r,0,1)=='P',"real existing WAD loader route");
     require(f.files.allocations==std::vector<Word>{static_cast<Word>(f.files.contents[wad].size()),8},"WAD and catalog must be distinct raw allocations");
     auto* const cached=f.config.files.archive.get();
     std::array<std::uint8_t,8> old_out,new_out;old_out.fill(0xee);new_out.fill(0xee);std::size_t copied=99;
     const auto old_eax=source_boot_file_read_44b260(f.config,f.files,path,3,old_out);
     const auto new_eax=source_boot_file_read_44b260(f.config,f.files,path,3,new_out,copied);
     require(old_eax==7&&new_eax==old_eax&&old_out==new_out&&copied==3&&new_out[3]==0xee,"observation changed old WAD EAX/copy semantics");
     copied=99;require(source_boot_file_read_44b260(f.config,f.files,path,0,new_out,copied)==7&&copied==0,"zero request observation/EAX");
     f.memory.publish_440b26(r);f.memory.dispose_440c20();
     require(f.config.files.archive.get()==cached&&f.config.files.readable_prefix==f.files.contents[wad].size(),"catalog disposer released shared WAD cache");}
    {Fixture f;Word offset{};const std::string wad="X:\\Root\\wads\\setup.wad";
     f.files.contents[wad]=archive("setup\\languagepc.csv","PAYLOAD",offset);f.files.limits[wad]=static_cast<std::size_t>(offset)+3;
     rejects<SourceBootConfigError>([&]{(void)f.memory.load_43c7f0(f.files,"Setup\\languagePC.csv");},0x44b354u);
     const auto ref=f.memory.last_attempt().resource;const auto info=f.memory.inspect(ref);
     require(info.initialized_prefix==0&&!info.terminator_written&&!info.read_return&&
         reinterpret_cast<const std::uint8_t*>(info.native_address)[0]==0xcd,"incomplete first WAD DWORD must not write destination bytes");
     require(f.config.files.archive&&f.config.files.readable_prefix==static_cast<std::size_t>(offset)+3&&std::string(f.config.files.searched_name.data())=="setup\\languagepc.csv","partial WAD exception rolled back cache/scratch");
     rejects([&]{(void)f.memory.read_byte(ref,0,0x43c696u);},0x43c696u);}
}
void wad_cache_replacement_and_short_catalog(){
    Fixture f;Word offset{};const std::string setup="X:\\Root\\wads\\setup.wad",misc="X:\\Root\\wads\\misc.wad";
    f.files.contents[setup]=archive("setup\\languagepc.csv","ORIGINAL",offset);
    const auto first=f.memory.load_43c7f0(f.files,"Setup\\languagePC.csv");f.memory.publish_440b26(first);
    f.files.contents[misc]=archive("graphics24\\misc\\thing","OTHER",offset);
    const auto second=f.memory.load_43c7f0(f.files,"Graphics24\\Misc\\thing");
    require(std::string(f.config.files.archive_name.data())=="misc.wad"&&f.memory.read_byte(first,0,1)=='O'&&f.memory.read_byte(second,0,1)=='O',"catalog bytes incorrectly borrowed WAD cache");
    require(f.memory.complete_input(first,1).bytes[7]=='L'&&f.memory.inspect(second).payload_bytes==5,"cache replacement corrupted independent catalog");
    // Source size can be loose while read selects a longest-prefix archive.
    const std::string loose="X:\\Root\\Setup\\Extra\\languagePC.csv";
    f.files.contents[setup]=archive("setup\\extra\\languagepc.csv","XYZ",offset);f.files.sizes[loose]=8;
    const auto short_ref=f.memory.load_43c7f0(f.files,"Setup\\Extra\\languagePC.csv");const auto info=f.memory.inspect(short_ref);
    require(info.payload_bytes==8&&info.initialized_prefix==3&&info.read_return==3u&&f.memory.read_byte(short_ref,8,1)==0,"size/read route mismatch lost n or source EAX");
    rejects([&]{(void)f.memory.read_byte(short_ref,3,0x43c696u);},0x43c696u);
}
void roots_paths_and_preflight_boundaries(){
    Fixture f;const auto first=f.load();f.memory.publish_440b26(first);
    root(f.config,"Y:\\Changed\\");const std::string path="Y:\\Changed\\\\Custom\\catalog.csv";f.files.contents[path]=bytes("NEW");
    const auto second=f.load();require(f.memory.read_byte(second,0,1)=='N'&&f.memory.inspect(second).payload_bytes==3,"cached or normalized old root");
    require(f.files.calls[f.files.calls.size()-3]=="S:"+path,"literal root plus one backslash spelling");
    const auto calls=f.files.calls.size();
    rejects([&]{(void)f.memory.load_43c7f0(f.files,std::string_view("A\0B",3));},0x43c80cu);
    rejects([&]{(void)f.memory.load_43c7f0(f.files,std::string(255,'x'));},0x43c80cu);
    require(f.files.calls.size()==calls&&f.current==first.identity(),"invalid local path caused I/O/current mutation");
    f.config.base_path.fill('X');rejects([&]{(void)f.load();},0x43c80cu);
    Fixture nested;rejects<SourceBootConfigError>([&]{(void)nested.memory.load_43c7f0(nested.files,std::string(120,'z'));},0x44b063u);
    require(nested.files.calls.empty()&&nested.memory.live_allocations()==0,"existing file splitter boundary bypassed");
}
void owner_identity_null_stale_and_foreign_thread(){
    static_assert(!std::is_copy_constructible_v<SourceFrontendResourceMemory> && !std::is_move_constructible_v<SourceFrontendResourceMemory>);
    Fixture a,b;const auto ar=a.load(),br=b.load();
    require(ar.identity()==br.identity()&&!(ar==br),"cross-owner numeric identity alias");
    rejects([&]{a.memory.publish_440b26(br);},0x440b26u);
    rejects([&]{(void)a.memory.read_byte(br,0,0x43c696u);},0x43c696u);
    rejects([&]{(void)a.memory.read_byte({},0,0x43c696u);},0x43c696u);
    require(!a.memory.current_reference()&&a.memory.initialized_prefix({}).empty(),"null source reference observation");
    a.current=900;require(a.memory.current_reference().identity()==900,"current_reference dereferenced stale source word too early");
    rejects([&]{(void)a.memory.read_byte(a.memory.current_reference(),0,0x440d38u);},0x440d38u);
    rejects([&]{a.memory.dispose_440c20();},0x440c2au);require(a.current==900,"stale disposer cleared source word on failure");a.current=0;
    SourceFrontendResourceRef expired;
    {Fixture temporary;expired=temporary.load();}
    rejects([&]{a.memory.publish_440b26(expired);},0x440b26u);
    std::atomic<unsigned> rejection_count{};
    std::thread other([&]{
        try{(void)a.load();}catch(const SourceFrontendResourceError&){++rejection_count;}
        try{a.memory.publish_440b26(ar);}catch(const SourceFrontendResourceError&){++rejection_count;}
        try{a.memory.dispose_440c20();}catch(const SourceFrontendResourceError&){++rejection_count;}
        try{(void)a.memory.read_byte(ar,0,1);}catch(const SourceFrontendResourceError&){++rejection_count;}
    });other.join();
    require(rejection_count==4&&a.current==0&&a.memory.live_allocations()==1,"foreign thread reached source state");
    a.events.try_reentry=true;const auto next=a.load();
    require(a.events.reentry_rejected&&!a.events.reentry_wrong&&a.memory.inspect(next).loader_returned,"diagnostic reentry mutated owner");
    SourceBootConfigState nonnull_config;Word cell=123;
    rejects([&]{SourceFrontendResourceMemory invalid(nonnull_config,cell);},0x440b26u);require(cell==123,"invalid initial binding changed source word");
}
void host_cleanup_is_separate(){
    SourceBootConfigState config;root(config,"X:\\Root");Word cell{};Events events;Files files;
    const std::string path="X:\\Root\\Custom\\catalog.csv";files.contents[path]=bytes("ABCDEF");
    Word last_identity{};std::uintptr_t current_address{};SourceFrontendResourceRef expired;
    {
        SourceFrontendResourceMemory owner(config,cell,events.view());events.owner=&owner;
        const auto first=owner.load_43c7f0(files,"Custom\\catalog.csv");owner.publish_440b26(first);
        const auto second=owner.load_43c7f0(files,"Custom\\catalog.csv");owner.publish_440b26(second);
        expired=second;last_identity=second.identity();current_address=owner.inspect(second).native_address;
        files.limits[path]=2;files.throw_read=path;
        rejects<SourceBootConfigError>([&]{(void)owner.load_43c7f0(files,"Custom\\catalog.csv");},0x44afccu);
        require(owner.live_allocations()==3,"host cleanup fixture retains current overwritten and failed allocations");
    }
    events.owner=nullptr;
    require(cell==last_identity&&expired.identity()==last_identity,"host final cleanup pretended source440C20");
    require(events.number(Kind::before_host_free)==3&&events.number(Kind::after_host_free)==3&&events.number(Kind::source_null)==0&&events.number(Kind::before_source_free)==0,"missing/double host frees or source disposal mixed into host cleanup");
    bool current_freed=false;
    for(std::size_t i=0;i<events.count;++i)if(events.values[i].kind==Kind::before_host_free){
        require(events.values[i].source_identity==last_identity,"cleanup changed source identity");
        require(i+1<events.count&&events.values[i+1].kind==Kind::after_host_free&&events.values[i+1].resource.identity==events.values[i].resource.identity,"host free pair/order");
        if(events.values[i].resource.native_address==current_address)current_freed=true;
    }
    require(current_freed,"current raw allocation missed final host cleanup");
}
}
int main(int argc,char** argv){
    if(argc==2&&std::string_view(argv[1])=="--foreign-destroy"){
        SourceBootConfigState config;Word cell{};auto* memory=new SourceFrontendResourceMemory(config,cell);
        //MSVC terminate handler is thread-local: install on the worker that
        //violates ownership, not only on the creating thread.
        std::thread worker([&]{std::set_terminate([]{std::_Exit(86);});delete memory;});worker.join();return 7;
    }
    try{
        success_reload_and_current_disposal();signed_size_and_allocator_frontiers();short_read_and_actual_read_checks();
        read_exception_retains_ownership_and_stores();wad_return_copy_and_partial_exception();wad_cache_replacement_and_short_catalog();
        roots_paths_and_preflight_boundaries();owner_identity_null_stale_and_foreign_thread();host_cleanup_is_separate();
        std::cout<<"PASS source frontend resource memory: "<<assertions<<" assertions, 9 receiving groups\n";return 0;
    }catch(const std::exception& error){std::cerr<<"FAIL after "<<assertions<<" assertions: "<<error.what()<<'\n';return 1;}
}
