// Reuse the unchanged, reviewed CPU submission fixture in this separate binary.
// The original test target still owns and runs its own main and route matrix.
#define main prior_core_cpu_main
#include "world_packet_join_cpu.cpp"
#undef main
#include "sgn/world_join_sidecar.hpp"
#include "sgn/displayed_recipe_export.hpp"
#include <fstream>
#include <sstream>
#include <chrono>
#include <limits>

namespace application_test {
using Table=WorldJoinRetention<WorldPacketJoinCapture>;
template<class F> void throws(F&& f,const char* label){bool caught=false;try{f();}catch(const std::exception&){caught=true;}check(caught,label);}
std::string read(const std::filesystem::path& p){std::ifstream in(p,std::ios::binary);check(bool(in),"open witness file");return {std::istreambuf_iterator<char>(in),{}};}
void write(const std::filesystem::path& p,std::string_view text){std::ofstream out(p,std::ios::binary|std::ios::noreplace);out.write(text.data(),static_cast<std::streamsize>(text.size()));out.close();check(bool(out),"write witness file");}
std::string hex(const WorldJoinFingerprint& fp){constexpr char digits[]="0123456789abcdef";std::string out;for(auto b:fp.sha256){out+=digits[b>>4];out+=digits[b&15];}return out;}
bool has(std::string_view text,std::string_view field){return text.find(field)!=std::string_view::npos;}
auto factory(){return []{return std::make_unique<WorldPacketJoinCapture>(true);};}

struct CaptureOptions {unsigned views{1};bool split{},special{},no_owners{},clip{};};
// This runs the actual CPU PolygonDeviceContext and unchanged source submission
// loop. No Session, HWND, renderer, original EXE, texture upload or GPU is created.
ImmutablePolygonSubmission capture(WorldPacketJoinCapture& join,PolygonDeviceContext& device,
    std::uint64_t serial,std::uint64_t tick,CaptureOptions options={}){
    Fixture f;auto owners=f.owners();
    if(options.no_owners)owners.clear();
    if(options.special){
        owners[0].owner.source_name=std::string("A\"\\\n\t\0Z",7);
        owners[1].owner.source_name=std::string("\x80\xff",2);
        const std::uint32_t bits=0x7f800001;std::memcpy(&f.viewport.origin_x,&bits,4);
    }
    check(join.begin(serial,tick,device,owners),"application capture begin");
    auto out=std::make_shared<CapturedPolygonSubmission>();out->device=device.identity();out->source_tick=tick;out->initial=device.snapshot();
    device.begin_capture(tick,6,{{20000,2000000},30000});check(device.begin_scene()==0,"application CPU scene");
    for(unsigned v=0;v<options.views;++v){
        f.viewport.selector=v+1;const auto input=f.input();
        check(no_alloc([&]{return join.preclip(f.frame,v);}),"application preclip without allocation");
        if(options.clip&&v==0){f.clipped[0]=f.headers[1];f.clipped[0][0]=6;f.pools[0][1].header=world_polygon_header_ref(f.clipped[0]);f.frame.clip_headers=std::span(f.clipped).first(1);f.pools[0][3].header={};}
        check(no_alloc([&]{return join.before_submission(f.frame,input,device);}),"application before without allocation");
        submit_world_polygons(f.lists(),input,f.scratch,device);
        check(no_alloc([&]{return join.after_submission(device);}),"application after without allocation");
        if(options.split&&v+1<options.views){out->segments.push_back({device.seal_capture(),{}});device.begin_capture(tick,7+v,{{20000,2000000},30000});}
    }
    check(device.end_scene()==0,"application end scene");out->segments.push_back({device.seal_capture(),{}});out->final=device.snapshot();
    for(const auto& s:out->segments){out->commands+=s.device_frame->polygon_commands->commands.size();out->events+=s.device_frame->events.size();out->payload_bytes+=s.device_frame->polygon_commands->payload_bytes;}
    return out;
}
ImmutablePolygonSubmission commit(Table& table,PolygonDeviceContext& device,std::uint64_t serial,std::uint64_t tick,
    const ImmutablePolygonSubmission& front,CaptureOptions options={}){
    const auto token=table.reserve(serial,tick,front,factory());check(bool(token),"reserve diagnostic cell");
    auto recipe=capture(*table.pending(*token),device,serial,tick,options);
    const auto recipe_count=recipe.use_count(),frame_count=recipe->segments[0].device_frame.use_count();
    table.complete(token,serial,recipe);
    check(table.lookup(recipe).status==WorldJoinLookupStatus::matched,"commit exact nested identity");
    check(table.lookup(recipe).entry->record->result().status==WorldJoinStatus::derived_checked,"committed core checked");
    check(recipe.use_count()==recipe_count&&recipe->segments[0].device_frame.use_count()==frame_count,"table retains no strong recipe or CPU frame");
    return recipe;
}
void options(){++cases;
    const auto a=parse_world_join_selection(L"939",L"2");check(a.valid()&&a.contains(939)&&a.contains(940)&&!a.contains(938)&&!a.contains(941),"explicit serial window");
    check(!WorldJoinSelection{}.contains(0)&&!WorldJoinSelection{0,1}.valid()&&!WorldJoinSelection{1,65}.valid(),"invalid programmatic range stays off");
    check(parse_world_join_selection(L"000939",L"02").first==939,"decimal leading zero is decimal");
    constexpr auto max=(std::numeric_limits<std::uint64_t>::max)();
    check(parse_world_join_selection(L"18446744073709551615",L"1").contains(max),"uint64 maximum single invocation");
    check(parse_world_join_selection(L"18446744073709551552",L"64").contains(max),"maximum valid64 window");
    check(!WorldJoinSelection{max,2}.contains(max),"programmatic range overflow rejected");
    for(const auto* first:{L"",L"0",L"-1",L"+1",L" 1",L"1 ",L"1x",L"1.0",L"\uff11",L"18446744073709551616"})throws([&]{(void)parse_world_join_selection(first,L"1");},"invalid first rejected");
    for(const auto* count:{L"",L"0",L"65",L"-1",L"+1",L" 1",L"1x",L"18446744073709551616"})throws([&]{(void)parse_world_join_selection(L"1",count);},"invalid count rejected");
    throws([]{(void)parse_world_join_selection(L"18446744073709551615",L"2");},"window overflow rejected");
    Table off({});unsigned factories=0;check(!off.reserve(0,0,{},[&]{++factories;return factory()();}),"default invalid table off");check(factories==0,"off does not allocate record");
}
void retention_roles(){++cases;Table table({939,64});PolygonDeviceContext device({});
    auto front=commit(table,device,939,100,{});auto back=commit(table,device,940,100,front);
    check(table.lookup(front).entry->serial==939&&table.lookup(back).entry->serial==940,"same device and paused tick distinguish front939/back940");
    const auto token=table.reserve(941,100,front,factory());check(bool(token),"third cell reserved");
    // The actual source Flip may promote the previously completed back during
    // this invocation. Both pre-capture front and previous back must survive.
    auto displayed=back;auto pending=capture(*table.pending(*token),device,941,100,{2,true});
    check(table.lookup(front).entry->serial==939&&table.lookup(displayed).entry->serial==940,"both prior roles survive pending source flip");
    check(table.lookup(pending).status==WorldJoinLookupStatus::not_retained,"pending is never published before seal completion");
    table.complete(token,941,pending);check(table.lookup(displayed).entry->serial==940,"complete does not select newest back");
    auto newest=commit(table,device,942,100,displayed);
    check(table.lookup(front).status==WorldJoinLookupStatus::not_retained,"only obsolete front evicted");
    check(table.lookup(displayed).entry->serial==940&&table.lookup(pending).entry->serial==941,"actual front and previous back survive reserve");
    check(table.lookup(newest).entry->serial==942&&table.counters().committed==4,"bounded roles commit");
    auto clone=std::make_shared<CapturedPolygonSubmission>(*displayed);back.reset();displayed.reset();
    check(table.lookup(clone).entry->serial==940,"recovery top-level clone survives original expiration");
    auto lookalike=std::make_shared<CapturedPolygonSubmission>(*clone);lookalike->segments[0].device_frame=std::make_shared<CapturedPolygonDeviceFrame>(*clone->segments[0].device_frame);
    check(table.lookup(lookalike).status==WorldJoinLookupStatus::not_retained,"identical bytes with new nested ownership rejected");
    auto changed=std::make_shared<CapturedPolygonSubmission>(*clone);++changed->commands;check(table.lookup(changed).status==WorldJoinLookupStatus::not_retained,"root metadata change rejected");
    check(table.lookup({}).status==WorldJoinLookupStatus::no_displayed,"absent front has no fallback");
    WorldJoinLookupStatus foreign{};WorldJoinSidecarView foreign_view;bool foreign_reserved=true;
    std::thread thread([&]{foreign=table.lookup(clone).status;foreign_view=world_join_sidecar_view(table,clone);foreign_reserved=bool(table.reserve(943,100,clone,factory()));});thread.join();
    check(!foreign_reserved,"foreign reserve ignored");
    check(foreign==WorldJoinLookupStatus::foreign_thread&&foreign_view.lookup==foreign&&!foreign_view.result,"foreign lookup fails before cell access");
}
void failure_isolation(){++cases;Table table({939,8});PolygonDeviceContext device({});
    auto front=commit(table,device,939,100,{});auto back=commit(table,device,940,100,front);
    const auto before=device.snapshot();
    check(!table.reserve(941,100,front,[]()->std::unique_ptr<WorldPacketJoinCapture>{throw std::bad_alloc();}),"factory allocation failure isolated");
    check(table.lookup(front).entry->serial==939&&table.lookup(back).entry->serial==940,"allocation failure preserves both roles");
    check(!table.reserve(941,100,front,[]{return std::unique_ptr<WorldPacketJoinCapture>{};}),"null factory isolated");
    auto token=table.reserve(941,100,front,factory());check(bool(token),"retry after allocation failure");
    check(!table.reserve(942,100,front,factory()),"second pending rejected");
    table.cancel(token);check(!table.pending(*token),"cancel invalidates token");table.cancel(token);
    check(table.counters().aborted==1&&table.counters().allocation_failures==2,"diagnostic failures counted once");
    token=table.reserve(941,100,front,factory());check(bool(token),"reuse canceled cell");table.complete(token,941,{});
    check(table.counters().nonrendered==1&&table.lookup(front).entry->serial==939&&table.lookup(back).entry->serial==940,"nonrendered capture retains both roles");
    // A hole before a retained duplicate cannot bypass serial rejection.
    check(!table.reserve(940,100,{},factory()),"duplicate serial rejected across empty cell");
    token=table.reserve(941,100,front,factory());check(bool(token),"reserve wrong-serial probe");table.complete(token,999,back);
    check(!table.pending(*token)&&table.counters().identity_failures==1,"wrong accepted serial aborts diagnostics");
    token=table.reserve(941,101,front,factory());check(bool(token),"reserve wrong-tick probe");table.complete(token,941,back);
    check(table.counters().identity_failures==2&&table.lookup(front).entry->serial==939,"wrong tick preserves actual front");
    table.complete(token,941,back);check(table.counters().order_failures>=2,"stale completion rejected");
    check(device.snapshot().operation_sequence==before.operation_sequence,"retention failures issue no device/source calls");
    unsigned invoked=0;check(!table.reserve(1000,100,front,[&]{++invoked;return factory()();})&&invoked==0,"outside window does not allocate");
    table.complete({},1000,{});check(table.lookup(back).entry->serial==940,"outside-window null preserves last completed back");
    auto other=std::make_shared<CapturedPolygonSubmission>(*back);other->segments[0].device_frame=std::make_shared<CapturedPolygonDeviceFrame>(*back->segments[0].device_frame);
    table.complete({},1000,other);auto replacement=commit(table,device,941,100,front);
    check(table.lookup(front).entry->serial==939&&table.lookup(replacement).entry->serial==941,"outside-window nonretained back update preserves actual front");
}
void weak_identity(){++cases;auto built=build();
    auto writable=std::make_shared<CapturedPolygonSubmission>(*built.submission);auto bank=std::make_shared<PolygonGpuTextures>();std::weak_ptr<const PolygonGpuTextures> bank_weak=bank;writable->segments[0].textures=bank;bank.reset();
    std::weak_ptr<const CapturedPolygonDeviceFrame> frame_weak=writable->segments[0].device_frame;
    WorldJoinRecipeIdentity identity;check(no_alloc([&]{return identity.bind(writable);}),"identity bind no allocation");
    check(no_alloc([&]{return identity.matches(writable);}),"identity match no allocation");
    writable->segments[0].textures.reset();check(bank_weak.expired(),"identity does not retain texture bank");
    auto clone=std::make_shared<CapturedPolygonSubmission>(*writable);writable.reset();built.submission.reset();
    check(identity.matches(clone),"identity matches clone with identical nested frames");clone.reset();check(frame_weak.expired(),"identity does not retain CPU frame");
    check(!identity.matches({}),"expired identity cannot match null");
    WorldJoinRecipeIdentity invalid;auto empty=std::make_shared<CapturedPolygonSubmission>();check(!invalid.bind(empty),"missing device/segments rejected");
    auto many=build(true,2,true);auto root=std::make_shared<CapturedPolygonSubmission>(*many.submission);root->segments.resize(17,root->segments[0]);check(!invalid.bind(root),"more than16 segments rejected");
    root->segments.resize(1);root->segments[0].device_frame.reset();check(!invalid.bind(root),"null nested frame rejected");
}
void failure_record_and_ambiguity(){++cases;Table table({939,4});PolygonDeviceContext device({});
    auto front=commit(table,device,939,100,{});
    auto token=table.reserve(940,100,front,factory());check(bool(token),"failed core reserve");
    auto recipe=capture(*table.pending(*token),device,940,100);table.pending(*token)->abort();table.complete(token,940,recipe);
    check(table.lookup(recipe).status==WorldJoinLookupStatus::matched&&table.counters().join_failures==1,"failed core keeps exact identity for truthful status");
    std::ostringstream failed;check(!write_world_join_sidecar(failed,{},recipe,world_join_sidecar_view(table,recipe)),"failed core cannot export geometry");
    check(has(failed.str(),"\"geometry\":null")&&has(failed.str(),"\"core_matches_displayed\":false"),"failed core truthful sidecar");
    // Duplicate identity is an invalid capture sequence. Never silently prefer
    // the earlier or later serial when two retained records claim it.
    token=table.reserve(941,100,front,factory());check(bool(token),"duplicate identity reserve");table.complete(token,941,recipe);
    check(table.lookup(recipe).status==WorldJoinLookupStatus::ambiguous,"ambiguous nested identity rejected");
    std::ostringstream ambiguous;check(!write_world_join_sidecar(ambiguous,{},recipe,world_join_sidecar_view(table,recipe)),"ambiguous record no geometry");
    check(has(ambiguous.str(),"\"lookup_status\":\"ambiguous\""),"ambiguous reason written");
}
void sha256(const std::filesystem::path& output){++cases;
    struct Vector {std::string value;const char* digest;};
    const std::array vectors{
        Vector{"","e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        Vector{"abc","ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"},
        Vector{"abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq","248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"},
        Vector{std::string(1000000,'a'),"cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0"}};
    for(const auto& v:vectors){WorldJoinSha256 hash;hash.append({reinterpret_cast<const std::uint8_t*>(v.value.data()),v.value.size()});check(hex(hash.finish())==v.digest&&hash.finish().bytes==v.value.size(),"SHA256 known answer");check(hash.finish()==hash.finish(),"digest finalization repeatable");}
    std::ofstream manifest(output/"hashes.jsonl",std::ios::binary|std::ios::noreplace);check(bool(manifest),"hash manifest create");
    for(const std::size_t n:{0u,1u,7u,55u,56u,57u,63u,64u,65u,119u,120u,127u,128u,129u,65535u,65536u,65537u}){
        std::vector<std::uint8_t> bytes(n);for(std::size_t i=0;i<n;++i)bytes[i]=static_cast<std::uint8_t>((i*37u+11u)&255u);
        WorldJoinSha256 whole,chunked;whole.append(bytes);
        for(std::size_t i=0;i<n;){const auto count=(std::min)(std::size_t{17},n-i);chunked.append(std::span(bytes).subspan(i,count));i+=count;}
        check(whole.finish()==chunked.finish(),"SHA256 chunking/padding identical");
        const auto name="hash-"+std::to_string(n)+".bin";write(output/name,{reinterpret_cast<const char*>(bytes.data()),bytes.size()});
        check(fingerprint_world_join_file(output/name)==whole.finish(),"file and streamed SHA256 match");
        manifest<<"{\"file\":\""<<name<<"\",\"sha256\":\""<<hex(whole.finish())<<"\",\"bytes\":"<<n<<"}\n";
    }
    manifest.close();check(bool(manifest),"hash manifest close");
    WorldJoinSha256 continued;const std::array<std::uint8_t,1> a{'a'},b{'b'};continued.append(a);(void)continued.finish();continued.append(b);
    check(hex(continued.finish())=="fb8e20fc2e4c3f248c60c39bd652f3c1347298bb977b8b4d5903b85055620603","finish does not mutate incremental state");
    throws([&]{(void)fingerprint_world_join_file(output/"absent.bin");},"missing fingerprint file fails");
}
class FailingBuffer final:public std::streambuf {protected:std::streamsize xsputn(const char*,std::streamsize) override{return 0;}int_type overflow(int_type) override{return traits_type::eof();}};
void serializer(const std::filesystem::path& output){++cases;Table table({939,64});PolygonDeviceContext device({});
    auto front=commit(table,device,939,100,{},CaptureOptions{2,true,true,false,true});
    auto back=commit(table,device,940,100,front);const auto front_count=front.use_count(),nested_count=front->segments[0].device_frame.use_count();
    const auto view=world_join_sidecar_view(table,front);
    check(front.use_count()==front_count&&front->segments[0].device_frame.use_count()==nested_count,"sidecar borrow retains no strong recipe/frame");
    const auto recipe_path=output/"front939.bin",sidecar_path=output/"front939.json";
    check(export_displayed_world_join(recipe_path,sidecar_path,front,view),"actual front complete export");
    std::ostringstream existing(std::ios::binary|std::ios::out);write_displayed_recipe(existing,front);
    check(read(recipe_path)==existing.str(),"wrapped existing recipe writer exact byte equality");
    const auto json=read(sidecar_path);
    check(has(json,"\"accepted_serial\":\"939\"")&&!has(json,"\"accepted_serial\":\"940\""),"export selected front939 not newer back940");
    check(has(json,"\"complete\":true")&&has(json,"\"source_lods\":[0,0,0,0]")&&has(json,"multiple_storage_memberships"),"all aliases retained");
    check(has(json,"\"source_name_bytes_hex\":\"80ff\"")&&has(json,"\"source_name_ascii\":null"),"nonASCII source bytes remain exact");
    check(has(json,"\"source_name_bytes_hex\":\"41225c0a09005a\""),"ASCII control and embedded NUL retained");
    check(has(json,"2139095041")&&has(json,"\"controller\":1")&&has(json,"\"segment\":1"),"raw sNaN words and split views retained");
    check(has(json,"\"relation\":\"replaced\"")&&has(json,"\"relation\":\"rejected\""),"clipping outcomes retained");
    check(has(json,"\"sha256\":\""+hex(fingerprint_world_join_file(recipe_path))+"\""),"sidecar bound to actual saved recipe");
    const auto front_bytes=read(recipe_path);throws([&]{(void)export_displayed_world_join(recipe_path,output/"duplicate.json",front,view);},"preexisting recipe refuses overwrite");
    check(read(recipe_path)==front_bytes&&!std::filesystem::exists(output/"duplicate.json"),"exclusive failure preserves old recipe and creates no sidecar");
    write(output/"existing.json","preserve me");throws([&]{(void)export_displayed_world_join(output/"orphan.bin",output/"existing.json",front,view);},"preexisting sidecar refuses overwrite");
    check(read(output/"existing.json")=="preserve me","old sidecar preserved");
    throws([&]{(void)export_displayed_world_join(output/"same.bin",output/"same.bin",front,view);},"identical output paths rejected before create");
    check(!std::filesystem::exists(output/"same.bin"),"no output on invalid paths");
    throws([&]{(void)write_bound_displayed_recipe(output/"absent-dir"/"x.bin",front);},"cannot create recipe fails");
    FailingBuffer fail;std::ostream bad(&fail);throws([&]{(void)write_world_join_sidecar(bad,{},front,view);},"short stream write cannot report success");
    std::ostringstream stale;check(!write_world_join_sidecar(stale,{},back,view),"stale view rejected for newer same-tick front");
    check(has(stale.str(),"\"lookup_status\":\"identity_mismatch\"")&&has(stale.str(),"\"geometry\":null"),"stale view no fallback geometry");write(output/"stale.json",stale.str());
    auto clone=std::make_shared<CapturedPolygonSubmission>(*front);check(export_displayed_world_join(output/"recovery.bin",output/"recovery.json",clone,world_join_sidecar_view(table,clone)),"recovery clone export uses nested identity");
    check(read(output/"recovery.bin")==front_bytes,"recovery clone recipe exact bytes");
    auto unknown=commit(table,device,941,100,front,CaptureOptions{1,false,false,true});
    check(export_displayed_world_join(output/"unknown.bin",output/"unknown.json",unknown,world_join_sidecar_view(table,unknown)),"unknown memberships do not invent model");
    check(has(read(output/"unknown.json"),"\"owners\":[]")&&has(read(output/"unknown.json"),"\"membership_status\":\"unknown\""),"unknown origin explicitly unknown");
    Table none({1,1});check(!export_displayed_world_join(output/"missing.bin",output/"missing.json",back,world_join_sidecar_view(none,back)),"nonretained actual front saved with incomplete result");
    check(has(read(output/"missing.json"),"\"lookup_status\":\"not_retained\"")&&has(read(output/"missing.json"),"\"geometry\":null"),"missing front no newest/contents fallback");
    check(!export_displayed_world_join(output/"null.bin",output/"null.json",{},world_join_sidecar_view(table,{})),"absent front explicit failure");
    check(has(read(output/"null.json"),"\"displayed\":null"),"null front declared");
    // JavaScript cannot exactly represent all source uint64 values as numbers.
    constexpr auto max=(std::numeric_limits<std::uint64_t>::max)();Table high({max,1});PolygonDeviceContext high_device({});
    auto high_recipe=commit(high,high_device,max,max,{});
    check(export_displayed_world_join(output/"uint64.bin",output/"uint64.json",high_recipe,world_join_sidecar_view(high,high_recipe)),"uint64 extreme export");
    check(has(read(output/"uint64.json"),"\"accepted_serial\":\"18446744073709551615\""),"uint64 serial string preserves precision");
    check(front.use_count()==front_count&&front->segments[0].device_frame.use_count()==nested_count+1,"only explicit recovery clone owns extra nested frame");
}
}
int main(int argc,char** argv){try{
    const auto output=argc==2?std::filesystem::path(argv[1]):std::filesystem::current_path()/("world-join-witness-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    check(std::filesystem::create_directory(output),"fresh witness output directory");
    application_test::options();application_test::retention_roles();application_test::failure_isolation();application_test::weak_identity();application_test::failure_record_and_ambiguity();application_test::sha256(output);application_test::serializer(output);
    std::cout<<"{\"passed\":true,\"cases\":"<<cases<<",\"checks\":"<<checks<<",\"gpu_calls\":0,\"source_fixture_only\":true,\"real_race_application_executed\":false}\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
