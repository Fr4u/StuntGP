#include "sgn/model_resources.hpp"
#include "fixtures/source_pmd_request_reset_trace.hpp"
#include <algorithm>
#include <iostream>
#include <thread>
#include <type_traits>

namespace {
using namespace sgn;
using namespace sgn::retail;
static_assert(sizeof(void*)==8,"receiving is the64-bit host contract");
static_assert(!std::is_copy_constructible_v<SourcePmdLoadingState>);
static_assert(!std::is_move_constructible_v<SourcePmdLoadingState>);
static_assert(std::is_copy_constructible_v<SourcePmdLoadingValues>);
static_assert(!std::is_copy_constructible_v<ModelResources>);
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Interrupted {};
struct Trace final:SourceRequestObserver {
    const SourcePmdRequests& actual;
    std::vector<SourceRequestStore> stores;
    std::uint32_t stop{};
    explicit Trace(const SourcePmdRequests& owner,std::uint32_t boundary=0):actual(owner),stop(boundary){}
    void after_store(const SourceRequestStore& event,const SourcePmdRequests& owner) override{
        check(&owner==&actual,"callback did not receive canonical loading-state owner");
        stores.push_back(event);check(event.ordinal==stores.size(),"nonconsecutive source stores");
        if(event.ordinal==stop)throw Interrupted{};
    }
};
template<class F>void rejects(F&& action){
    bool rejected=false;try{action();}catch(const SourceRequestError&){rejected=true;}
    check(rejected,"missing typed admission/lifetime rejection");
}
template<class F>void run_to_boundary(std::uint32_t stop,F&& action){
    if(stop==0)return; //the explicit entry state, before invoking the source prefix
    bool interrupted=false;try{action();}catch(const Interrupted&){interrupted=true;}
    check(interrupted,"did not interrupt after requested persistent source store");
}
LevelBlock block(const std::string& script){return {script,script.size()};}
SourceRequestBoundary track(ModelResources& resources,std::string_view value,SourceRequestObserver* observer=nullptr){
    const std::string script="L_TRACK = "+std::string(value)+" \n-END";
    return resources.produce_source_requests_42c740_prefix(block(script),observer);
}
SourceRequestBoundary sky(ModelResources& resources,std::string_view value,SourceRequestObserver* observer=nullptr){
    const std::string script="L_SKY = "+std::string(value)+" \nL_TRACK = MustNotBeReached \n-END";
    return resources.produce_source_requests_42c740_prefix(block(script),observer);
}
struct Fixture {
    SourceAllocationBank bank;
    ModelResources resources;
    Fixture(const AssetStore& assets,const std::filesystem::path& root):resources(assets,root){bank.initialize_42eab0();}
    const SourcePmdRequests& owner() const{return resources.source_loading_state().requests;}
};
void stale(Fixture& f){
    (void)f.resources.initialize_source_requests_42d070_prefix();
    (void)track(f.resources,"Old");(void)sky(f.resources,"Second");
}
void check_trace(const Trace& trace,const std::vector<SourceRequestStore>& expected,std::uint32_t count){
    check(trace.stores.size()==count,"wrong source store count");
    for(std::size_t i=0;i<count;++i){const auto& a=trace.stores[i];const auto& b=expected[i];
        check(a.site==b.site&&a.target==b.target&&a.bytes==b.bytes&&a.ordinal==i+1,"source persistent-store trace differs");}
}
void initialization(const AssetStore& assets,const std::filesystem::path& root){
    const std::vector<SourceRequestStore> expected{{0x42d10c,0x58c234,4,1},{0x42d116,0x58c540,4,2},{0x42d11c,0x58c808,4,3}};
    for(std::uint32_t cut=0;cut<=3;++cut){Fixture f(assets,root);stale(f);const auto before=f.owner().snapshot();auto wanted=before;
        Trace trace(f.owner(),cut);run_to_boundary(cut,[&]{(void)f.resources.initialize_source_requests_42d070_prefix(&trace);});
        if(cut>=1)wanted.append=before.records[0].name;
        if(cut>=2)wanted.records[0].name={};
        if(cut>=3)wanted.count=0;
        check(f.owner().snapshot()==wanted,"42D070 prefix changed an unreached word or stale role/name");
        check_trace(trace,expected,cut);
    }
}
void producer(const AssetStore& assets,const std::filesystem::path& root,bool is_sky){
    const std::string name=is_sky?"Sky\tName":"TrackA";
    std::vector<SourceRequestStore> expected;
    for(std::size_t i=0;i<name.size();++i)expected.push_back({0x42c61b,0x58c238+static_cast<std::uint32_t>(i),1,0});
    expected.push_back({0x42c62d,0x58c238+static_cast<std::uint32_t>(name.size()),1,0});
    if(is_sky){for(const auto pair:std::array<std::array<std::uint32_t,2>,5>{{{0x42c787,0x58c540},{0x42c78f,0x58c544},{0x42c79f,0x58c808},{0x42c7a5,0x58c234},{0x42c7ab,0x58c548}}})expected.push_back({pair[0],pair[1],4,0});}
    else{for(const auto pair:std::array<std::array<std::uint32_t,2>,6>{{{0x42c838,0x624460},{0x42c842,0x58c540},{0x42c84a,0x58c544},{0x42c85a,0x58c808},{0x42c860,0x58c234},{0x42c866,0x58c548}}})expected.push_back({pair[0],pair[1],4,0});}
    for(std::uint32_t cut=0;cut<=expected.size();++cut){
        Fixture f(assets,root);stale(f);(void)f.resources.initialize_source_requests_42d070_prefix();
        const auto before=f.owner().snapshot();auto wanted=before;Trace trace(f.owner(),cut);
        run_to_boundary(cut,[&]{if(is_sky)(void)sky(f.resources,name,&trace);else(void)track(f.resources,name,&trace);});
        for(std::uint32_t i=0;i<cut;++i){const auto& write=expected[i];
            if(write.site==0x42c61b)wanted.names[write.target-0x58c238]=name[write.target-0x58c238];
            else if(write.site==0x42c62d)wanted.names[name.size()]=0;
            else if(write.site==0x42c838)wanted.track_name=before.append;
            else if(write.site==0x42c787||write.site==0x42c842)wanted.records[0].name=before.append;
            else if(write.site==0x42c78f||write.site==0x42c84a)wanted.records[0].role=is_sky?2u:3u;
            else if(write.site==0x42c79f||write.site==0x42c85a)wanted.count=1;
            else if(write.site==0x42c7a5||write.site==0x42c860)wanted.append=before.append.advanced(static_cast<std::uint32_t>(name.size()+1));
            else wanted.records[1].name={};
        }
        check(f.owner().snapshot()==wanted,"producer prefix lost exact bytes/pointer/type/alias/sentinel order");
        check(f.owner().snapshot().records[1].role==2,"sentinel write cleared retained role");check_trace(trace,expected,cut);
    }
    Fixture f(assets,root);(void)f.resources.initialize_source_requests_42d070_prefix();
    const auto result=is_sky?sky(f.resources,name):track(f.resources,name);
    check(result.next_site==(is_sky?0x42c7b2u:0x42c872u)&&result.required_callee==(is_sky?0x425880u:0u)&&result.reached_return!=is_sky,"hidden source continuation");
    check(f.owner().snapshot().count==1&&f.owner().snapshot().records[0].name.text_snapshot()==name,"producer substituted a copied request vector");
    check(result.script_cursor==(is_sky?8u:10u)+name.size()+1,"missing borrowed script cursor at boundary");
    if(!is_sky)check(f.owner().track_name()==f.owner().snapshot().records[0].name,"624460 has a different name identity");
}
void fill(Fixture& f){
    (void)f.resources.initialize_source_requests_42d070_prefix();
    for(unsigned i=0;i<23;++i){const std::string name(i==22?63:31,static_cast<char>('A'+i));
        if(i%2==0)(void)track(f.resources,name);else(void)sky(f.resources,name);}
    const auto actual=f.owner().snapshot();
    check(actual.count==23&&actual.append&&actual.append.offset()==768,"actual producers did not fill sole name region");
    check(!actual.records[23].name,"last record is not source sentinel");
    for(unsigned i=0;i<23;++i)check(actual.records[i].name&&actual.records[i].role==(i%2==0?3u:2u),"request row lost actual producer identity");
}
void reset(const AssetStore& assets,const std::filesystem::path& root){
    for(std::uint32_t cut=0;cut<=240;++cut){
        Fixture f(assets,root);const auto id=f.resources.load_source(f.bank,"car1");
        const auto model=f.resources.source_at(*f.resources.resolve(id));const auto allocation=model.allocation();
        const auto input=f.bank.bytes(allocation);const Bytes original(input.begin(),input.end());
        fill(f);const auto before=f.owner().snapshot();const auto globals=f.resources.source_loading_state().snapshot();
        check(globals.version==0x12,"witness did not use actual nonzero PMD version");
        auto wanted=before;Trace trace(f.owner(),cut);run_to_boundary(cut,[&]{(void)f.resources.reset_source_requests_42eb00_prefix(&trace);});
        for(std::uint32_t i=0;i<cut;++i){const auto& expected=request_witness::reset_trace[i];
            const auto& actual=trace.stores[i];check(actual.site==expected.site&&actual.target==expected.target&&actual.bytes==4&&actual.ordinal==i+1,"reset differs from PE-derived240-store witness");
            if(i<192)std::fill_n(wanted.names.begin()+static_cast<std::ptrdiff_t>(i*4),4,char{});
            else{const auto at=i-192;if(at%2==0)wanted.records[at/2].name={};else wanted.records[at/2].role=0;}
        }
        check(trace.stores.size()==cut&&f.owner().snapshot()==wanted,"full reset footprint or untouched suffix differs");
        check(f.resources.source_loading_state().snapshot()==globals&&f.resources.size()==1&&f.bank.live_count()==1,"reset changed existing PMD globals/model/bank");
        const auto after=f.bank.bytes(allocation);check(std::equal(original.begin(),original.end(),after.begin(),after.end()),"reset wrote actual PMD payload");
        check(before.track_name==f.owner().track_name(),"content reset expired or changed624460 pointer identity");
        const auto at=before.track_name.offset();check(before.track_name.byte()==wanted.names[at],"existing weak alias did not observe same reset byte region");
    }
}
void lifetime(const AssetStore& assets,const std::filesystem::path& root){
    SourceRequestName dangling;
    {auto resources=std::make_unique<ModelResources>(assets,root);(void)resources->initialize_source_requests_42d070_prefix();
        (void)track(*resources,"Live");dangling=resources->source_loading_state().requests.track_name();
        check(dangling&&dangling.offset()==0&&!(dangling==SourceRequestName{}),"null collapsed into base-offset-zero");
        const auto snapshot=resources->source_loading_state().requests.snapshot();
        const auto base=dangling;const auto past=base.advanced(768);rejects([&]{(void)past.byte();});rejects([&]{(void)base.advanced(0xffffffffu);});
        bool read_rejected=false,write_rejected=false;
        std::thread foreign([&]{try{(void)base.byte();}catch(const SourceRequestError&){read_rejected=true;}
            try{(void)resources->reset_source_requests_42eb00_prefix();}catch(const SourceRequestError&){write_rejected=true;}});foreign.join();
        check(read_rejected&&write_rejected&&resources->source_loading_state().requests.snapshot()==snapshot,"foreign thread touched canonical region");
        const auto boundary=resources->reset_source_requests_42eb00_prefix();
        check(boundary.next_site==0x42eb1b&&!boundary.reached_return&&base.byte()==0,"reset expired owner or activated opaque region");
        resources.reset();rejects([&]{(void)snapshot.track_name.byte();}); //snapshot must not extend owner lifetime
    }
    check(static_cast<bool>(dangling),"dangling pointer value incorrectly became null");rejects([&]{(void)dangling.byte();});
    ModelResources other(assets,root);(void)other.initialize_source_requests_42d070_prefix();(void)track(other,"New");
    check(!(dangling==other.source_loading_state().requests.track_name()),"replacement owner reused old identity");
    rejects([]{(void)SourceRequestName{}.byte();});
}
void bounds(const AssetStore& assets,const std::filesystem::path& root){
    Fixture f(assets,root);const auto zero=f.owner().snapshot();rejects([&]{(void)track(f.resources,"BeforeInit");});check(f.owner().snapshot()==zero,"null append rejection mutated owner");
    const std::string absent="NO_KEY = Value \n-END";check(f.resources.produce_source_requests_42c740_prefix(block(absent)).reached_return,"absent keys cannot take null-owner-safe branch");
    (void)f.resources.initialize_source_requests_42d070_prefix();
    for(const std::string malformed:{"L_TRACK MissingDelimiter", "L_TRACK=NoSpace", "L_TRACK = NoEnding"}){
        const auto before=f.owner().snapshot();rejects([&]{(void)f.resources.produce_source_requests_42c740_prefix(block(malformed));});check(f.owner().snapshot()==before,"malformed token caused partial unsafe write");}
    const std::string edge="L_TRACK = BeyondBoundary \n-END";(void)f.resources.produce_source_requests_42c740_prefix({edge,1});
    check(f.owner().track_name().text_snapshot()=="BeyondBoundary","value parser incorrectly stopped at key-search boundary");
    const std::string nul("L_TRACK = A\0B \n",15);const auto before=f.owner().snapshot();
    rejects([&]{(void)f.resources.produce_source_requests_42c740_prefix(block(nul));});check(f.owner().snapshot()==before,"NUL domain rejection mutated pool");
    Fixture full(assets,root);fill(full);const auto all=full.owner().snapshot();Trace overflow(full.owner());
    rejects([&]{(void)track(full.resources,"Extra",&overflow);});check(overflow.stores.empty()&&full.owner().snapshot()==all,"capacity guard wrote into opaque next region");
    Fixture too_long(assets,root);(void)too_long.resources.initialize_source_requests_42d070_prefix();const auto empty=too_long.owner().snapshot();
    rejects([&]{(void)track(too_long.resources,std::string(768,'x'));});check(too_long.owner().snapshot()==empty,"missing NUL capacity silently truncated name");
}
struct PmdStop final:SourcePmdObserver {
    void observe(const SourcePmdEvent& event,const SourceAllocationBank&,const SourcePmdLoadingState&) override{
        if(event.site==0x42f006)throw Interrupted{};
    }
};
void independent_refs(const AssetStore& assets,const std::filesystem::path& root){
    Fixture f(assets,root);(void)f.resources.load_source(f.bank,"car1");PmdStop stop;
    bool stopped=false;try{(void)f.resources.load_source(f.bank,"car1",&stop);}catch(const Interrupted&){stopped=true;}
    const auto before=f.resources.source_loading_state().snapshot();
    check(stopped&&!(before.file==before.descriptor_file)&&f.bank.live_count()==2,"fixture did not reach actual independent file/descriptor publication");
    (void)f.resources.initialize_source_requests_42d070_prefix();(void)track(f.resources,"Borrowed");(void)f.resources.reset_source_requests_42eb00_prefix();
    check(f.resources.source_loading_state().snapshot()==before&&f.bank.live_count()==2&&f.resources.size()==1,"request prefix merged or released independent PMD references");
}
}
int main(int argc,char** argv){
    try{if(argc!=3)throw std::runtime_error("usage: source_pmd_request_owner_contracts case retail-root");
        const std::string_view name=argv[1];const std::filesystem::path root=argv[2];const AssetStore assets(root);
        if(name=="init_prefix")initialization(assets,root);
        else if(name=="sky_prefix")producer(assets,root,true);
        else if(name=="track_prefix")producer(assets,root,false);
        else if(name=="reset_prefix")reset(assets,root);
        else if(name=="lifetime")lifetime(assets,root);
        else if(name=="bounds")bounds(assets,root);
        else if(name=="independent_refs")independent_refs(assets,root);
        else throw std::runtime_error("unknown receiving case");
        std::cout<<"case="<<name<<" status=PASS full42D070=0 full42F090=0 full42EB00=0\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
