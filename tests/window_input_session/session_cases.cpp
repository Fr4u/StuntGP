// B1..B8 from Loader's sealed RECEIVING_PLAN; Main execution is recorded in
// reports/contact-work/series6/window-input-integration-execution-v1.
// Actual NEW Sessions/assets/aliases/window guards; no substitute game child.
#include "fixture_com.hpp"
#include "allocation_probe.hpp"
#include "sgn/session_window_input.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/source_input_cold_init.hpp"
#include "sgn/race_session.hpp"
#include "sgn/retail_frontend_state.hpp"
#include <algorithm>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <thread>
#include <utility>

namespace {
using namespace sgn::retail;
using fixture::require;
using Bytes=std::vector<unsigned char>;
constexpr auto platform_thread_error="race world and platform belong to the creating game thread";
constexpr auto alias_error="window input requires the actual world alias of one platform control block";
constexpr auto phase_error="DirectInput acquisition requested outside the initialized source lifetime";
constexpr auto binding_error="window input binding belongs to another raw owner or native window";
constexpr auto session_error="window input and Session must share the same world, control block and HWND";

template<class Error=sgn::FormatError,class F>void rejects(F action,const char* expected){
    try{action();}catch(const Error& error){
        require(std::string_view(error.what())==expected,"wrong rejection type/reason");return;
    }
    throw std::runtime_error(std::string("missing rejection: ")+expected);
}
// Same live object's representation before/after only; padding is not a
// recovered source value and no unrelated constructed objects are compared.
template<class T>void append(Bytes& bytes,const T& value){
    const auto* p=reinterpret_cast<const unsigned char*>(&value);
    bytes.insert(bytes.end(),p,p+sizeof(value));
}
Bytes effects_image(RaceSessionState& s){
    Bytes bytes;auto& a=s.effect_resources.animations;
    for(unsigned i=0;i<16;++i)for(const auto word:a.source_record(i))append(bytes,word);
    append(bytes,a.source_cursor());append(bytes,a.source_flag_577658());
    append(bytes,a.textures().size());append(bytes,a.frames().size());
    for(const auto& item:a.textures())append(bytes,item);
    for(const auto& item:a.frames())append(bytes,item);
    append(bytes,s.effect_resources.colors.borrow_for(s.sprite_packets));
    append(bytes,s.sprite_packets);return bytes;
}
Bytes unchanged_outside_cold(RaceSessionState& s){
    Bytes bytes;const auto& input=s.source_input;const auto& ff=input.force_feedback;
    append(bytes,input.profiles);append(bytes,input.vehicle_profiles);
    append(bytes,input.acquisition.upstream);append(bytes,input.acquisition.source_device_pointer_576b6c);
    append(bytes,input.previous_activity);append(bytes,s.menu_input.previous_pause);
    append(bytes,s.startup.fields_62d480);append(bytes,s.startup.fields_62dcf0);
    append(bytes,ff.selected_data_4d5b24);append(bytes,ff.initialized_576b70);
    append(bytes,ff.direction_576b78);append(bytes,ff.axes_477640);append(bytes,ff.axes_count_477648);
    append(bytes,ff.gain_47764c);append(bytes,ff.deadband_477650);append(bytes,ff.parameter_pointer_kind);
    const auto effects=effects_image(s);bytes.insert(bytes.end(),effects.begin(),effects.end());return bytes;
}
Bytes source_image(RaceSessionState& s){
    auto bytes=unchanged_outside_cold(s);append(bytes,s.source_input);
    append(bytes,s.menu_input);append(bytes,s.frontend_launch.input_bindings);return bytes;
}
void seed(RaceSessionState& s){
    auto& input=s.source_input;
    for(auto& bank:input.profiles)bank.fill(0x31415926u);
    for(auto& profile:input.vehicle_profiles)profile.fill(0x71);
    for(auto& record:input.acquisition.upstream)record.words.fill(0x12345678u);
    input.acquisition.source_device_pointer_576b6c=0x576810;input.previous_activity=0x76543210;
    s.startup.fields_62d480.fill(0x11223344);s.startup.fields_62dcf0.fill(0x55667788);
    s.menu_input.previous_pause=0x24681357;
    for(std::size_t i=0;i<4;++i){
        auto& raw=input.acquisition.physical[i];auto& x=input.physical[i];
        raw.word_8=7;raw.word_0c=6;raw.word_24=5;raw.word_28=4;
        raw.words_from_2c.fill(0xabcdef01);raw.extra_edges={1,2,3,4,5,6};
        x.activity_kind=9;x.profile_index=8;x.feedback_mode=2;
        x.bytes_168_187.fill(0x51);x.bytes_188_1a7.fill(0x52);
        x.bytes_1b6_1b7.fill(0x53);x.bytes_1d8_1db.fill(0x54);
        s.menu_input.raw_words[i]={11,12};s.menu_input.edges[i]={13,14,15,16,17,18,19,20};
        s.frontend_launch.input_bindings[i]={static_cast<std::uint32_t>(i+1),77};
        input.force_feedback.physical_words[i].fill(0x89abcdef);
    }
    auto& ff=input.force_feedback;ff.selected_data_4d5b24=0x62d914;ff.initialized_576b70=1;
    ff.direction_576b78={31,32};ff.axes_477640={4,8};ff.axes_count_477648=2;
    ff.gain_47764c=1234;ff.deadband_477650=42;
    ff.parameter_pointer_kind[3][7]=ForceFeedbackParameterPointer::transient_periodic;
    // Explicit receiving stimulus to an already recovered leaf, not a claim
    // that an automatic runtime animation producer/callsite has been found.
    PhysicalMaterialStorage materials;materials.reset_loading_42c480();
    materials.complete_next(10,20,32);materials.complete_next(11,21,32);
    auto& animations=s.effect_resources.animations;animations.clear_scene_42e510();
    animations.reset_loading_cursor_42c480();animations.recovered_entry_425a70(materials,2,0);
    s.effect_resources.colors.borrow_for(s.sprite_packets).source_undefined[17]=true;
    s.sprite_packets.indices[17]=65000;
}
template<class T>bool zero(const T& values){return std::all_of(values.begin(),values.end(),[](auto v){return v==0;});}
void require_cold_targets(RaceSessionState& s){
    for(std::size_t i=0;i<4;++i){
        auto& r=s.source_input.acquisition.physical[i];const auto& x=s.source_input.physical[i];
        require(s.menu_input.raw_words[i].word_0==0&&s.menu_input.raw_words[i].word_4==0,"cold missed menu raw owner");
        require(r.word_8==0&&r.word_0c==0&&r.word_24==0&&r.word_28==0&&zero(r.words_from_2c),"cold missed acquisition payload");
        require(x.activity_kind==0&&x.profile_index==0&&x.feedback_mode==0,"cold missed physical scalars");
        require(zero(x.bytes_168_187)&&zero(x.bytes_188_1a7)&&zero(x.bytes_1b6_1b7)&&zero(x.bytes_1d8_1db),"cold missed opaque bytes");
        const auto edges=borrow_menu_input_bank(s.menu_input.edges[i],r.extra_edges);
        for(const auto& e:edges.current)require(e.get()==0,"cold missed current edge");
        for(const auto& e:edges.previous)require(e.get()==0,"cold missed previous edge");
        require(zero(s.source_input.force_feedback.physical_words[i]),"cold missed SAME 32-byte FF bank");
        require(s.frontend_launch.input_bindings[i].vehicle==0xffffu&&s.frontend_launch.input_bindings[i].kind==0,"cold binding defaults differ");
    }
}

struct HiddenWindows {
    HINSTANCE instance{::GetModuleHandleW(nullptr)};HWND first{},second{};ATOM atom{};
    static constexpr auto name=L"StuntGPNewSessionWindowInputReceiving";
    HiddenWindows(){
        require(instance!=nullptr,"GetModuleHandleW failed");
        WNDCLASSEXW type{};type.cbSize=static_cast<UINT>(sizeof(type));type.hInstance=instance;
        type.lpfnWndProc=&::DefWindowProcW;type.lpszClassName=name;
        atom=::RegisterClassExW(&type);require(atom!=0,"RegisterClassExW failed");
        first=create();second=create();
        if(!first||!second){cleanup();throw std::runtime_error("CreateWindowExW failed");}
    }
    HWND create(){return ::CreateWindowExW(0,name,L"Session input receiving",WS_OVERLAPPEDWINDOW,
        0,0,160,100,nullptr,nullptr,instance,nullptr);}
    InputWindowHandles handles(HWND window) const{return {reinterpret_cast<std::uintptr_t>(instance),reinterpret_cast<std::uintptr_t>(window)};}
    void destroy_first(){require(::DestroyWindow(first)!=FALSE,"DestroyWindow failed");first=nullptr;}
    void cleanup() noexcept{
        if(second){(void)::DestroyWindow(second);second=nullptr;}
        if(first){(void)::DestroyWindow(first);first=nullptr;}
        if(atom){(void)::UnregisterClassW(name,instance);atom=0;}
    }
    ~HiddenWindows(){cleanup();}
    HiddenWindows(const HiddenWindows&)=delete;
    HiddenWindows& operator=(const HiddenWindows&)=delete;
};
struct Harness {
    HiddenWindows windows;
    fixture::Scenario os; // Outlives all actual input owners and their ledger.
    const InputWindowHandles handles{windows.handles(windows.first)};
    std::unique_ptr<RaceSession> session;
    std::shared_ptr<RaceSessionPlatform> platform;
    std::shared_ptr<RaceSessionState> world;
    std::unique_ptr<SessionWindowInput> input;
    explicit Harness(const std::filesystem::path& assets){
        require(fixture::active==nullptr,"receiving scenarios must be serial");
        fixture::active=&os;fixture::receiving_window=windows.first;(void)os.add(0);
        RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
        launch.window=static_cast<std::uint64_t>(handles.window);
        session=std::make_unique<RaceSession>(assets,launch);
        platform=session->shared_platform();world=session->shared_state();
    }
    void attach(){input=std::make_unique<SessionWindowInput>(*session,handles.instance);}
    void start(){require(input->initialize_4249b0()==1,"actual startup failed in scripted COM case");
        require(input->borrow_initialized().registry.device_count==1,"actual startup registry did not receive the scripted device");}
    void close(){input.reset();session.reset();world.reset();platform.reset();}
    ~Harness(){close();fixture::active=nullptr;fixture::receiving_window=nullptr;}
    Harness(const Harness&)=delete;
    Harness& operator=(const Harness&)=delete;
};
struct NativeIdentity {
    const void* keyboard;const void* startup;const void* registry;
    explicit NativeIdentity(WindowInputBorrow b):keyboard(&b.keyboard),startup(&b.direct_input),registry(&b.registry){
        require(&b.registry==&b.direct_input.registry_for_acquisition(),"borrow supplied a copied registry");
    }
    bool operator==(const NativeIdentity&) const=default;
};

void b1(const std::filesystem::path& assets){
    Harness h(assets);seed(*h.world);const auto before=source_image(*h.world);
    auto* owner=&h.platform->attach_window_input(h.handles.instance,h.handles.window);
    h.attach();
    require(source_image(*h.world)==before,"attach performed implicit source initialization");
    require(h.os.log.calls==std::vector<std::string>{"host.load","host.symbol"},"attach entered factory/source IO");
    rejects([&]{(void)h.input->borrow_initialized();},phase_error);h.start();
    const NativeIdentity identity(h.input->borrow_initialized(*h.session));
    const auto initialized=source_image(*h.world);const auto calls=h.os.log.calls;
    {
        FrontendState frontend(h.world,h.platform);
        SessionWindowInput second(*h.session,h.handles.instance),third(frontend,h.handles);
        for(unsigned i=0;i<16;++i){
            require(&h.platform->attach_window_input(h.handles.instance,h.handles.window)==owner,"attach replaced sole input owner");
            require(NativeIdentity(second.borrow_initialized(*h.session))==identity&&NativeIdentity(third.borrow_initialized())==identity,"native input identity changed");
        }
        require(source_image(*h.world)==initialized&&h.os.log.calls==calls,"reuse reset source state or reentered native IO");
    }
    h.input->shutdown_424930();h.close();h.os.clean();
}
void b2(const std::filesystem::path& assets){
    Harness h(assets);seed(*h.world);const auto before=source_image(*h.world);
    auto foreign=std::make_shared<RaceSessionPlatform>(assets,0.0f);
    auto foreign_world=std::shared_ptr<RaceSessionState>(foreign,&foreign->state());
    seed(*foreign_world);const auto foreign_before=source_image(*foreign_world);
    auto wrong_block=std::shared_ptr<RaceSessionState>(h.world.get(),[](RaceSessionState*){});
    // Never dereference this intentionally invalid alias: the wrapper must
    // reject its address before attachment or source/native effects.
    auto interior=std::shared_ptr<RaceSessionState>(h.platform,reinterpret_cast<RaceSessionState*>(&h.world->source_input));
    rejects([&]{SessionWindowInput bad(h.platform,foreign_world,h.handles);},alias_error);
    rejects([&]{SessionWindowInput bad(foreign,h.world,h.handles);},alias_error);
    rejects([&]{SessionWindowInput bad(h.platform,wrong_block,h.handles);},alias_error);
    rejects([&]{SessionWindowInput bad(h.platform,interior,h.handles);},"window input world does not belong to the supplied platform");
    rejects([&]{SessionWindowInput bad(*h.session,h.handles.instance+1);},"window input requires the actual local top-level HWND and HINSTANCE on its message thread");
    rejects([&]{(void)h.platform->window_input(h.handles.instance,h.handles.window);},"window input has not been attached to this platform");
    require(h.os.log.calls.empty()&&source_image(*h.world)==before&&source_image(*foreign_world)==foreign_before,"bad alias created input or mutated a world");
    // Same actual Session/control block/raw world, different HWND. This
    // isolates the HWND condition rather than failing on a foreign platform.
    const auto other=h.windows.handles(h.windows.second);
    h.input=std::make_unique<SessionWindowInput>(h.platform,h.world,other);
    rejects([&]{h.input->require_session(*h.session);},session_error);
    rejects([&]{(void)h.input->borrow_initialized(*h.session);},session_error);
    rejects([&]{(void)h.platform->attach_window_input(h.handles.instance,h.handles.window);},binding_error);
    rejects([&]{(void)h.platform->attach_window_input(other.instance+1,other.window);},binding_error);
    auto& owner=h.platform->window_input(other.instance,other.window);
    rejects([&]{owner.require_binding(foreign_world->source_input.acquisition,other);},binding_error);
    {
        struct InstanceOverride {
            HWND window;LONG_PTR original;
            explicit InstanceOverride(HWND w):window(w),original(::GetWindowLongPtrW(w,GWLP_HINSTANCE)){
                require(original!=0,"hidden HWND has no HINSTANCE");
                (void)::SetWindowLongPtrW(window,GWLP_HINSTANCE,original+1);
                require(::GetWindowLongPtrW(window,GWLP_HINSTANCE)==original+1,"cannot change receiving HWND instance");
            }
            ~InstanceOverride(){(void)::SetWindowLongPtrW(window,GWLP_HINSTANCE,original);}
        } changed(h.windows.second);
        rejects([&]{(void)h.input->borrow_initialized();},"window input HWND or HINSTANCE is no longer valid for this lifetime");
        rejects([&]{owner.require_binding(h.world->source_input.acquisition,other);},"window input HWND or HINSTANCE is no longer valid for this lifetime");
    }
    require(source_image(*h.world)==before&&source_image(*foreign_world)==foreign_before,"identity rejection reset inputs/effects");
    require(h.os.log.calls==std::vector<std::string>{"host.load","host.symbol"},"identity rejection called the factory");
    interior.reset();wrong_block.reset();foreign_world.reset();foreign.reset();h.close();h.os.clean();
}
void b3(const std::filesystem::path& assets){
    Harness h(assets);h.attach();h.start();seed(*h.world);
    auto frontend=std::make_unique<FrontendState>(h.world,h.platform);
    auto* raw=&h.world->source_input.acquisition;auto* effects=&h.world->effect_resources;
    std::weak_ptr<RaceSessionPlatform> weak=h.platform;
    const NativeIdentity identity(h.input->borrow_initialized());
    const auto before=source_image(*h.world);const auto calls=h.os.log.calls;
    h.session.reset();h.world.reset();h.platform.reset();
    require(!weak.expired()&&&frontend->world().source_input.acquisition==raw&&&frontend->world().effect_resources==effects,"Session destruction moved/destroyed persistent owners");
    {
        SessionWindowInput retained(*frontend,h.handles);
        require(NativeIdentity(retained.borrow_initialized())==identity,"frontend resolved another native owner");
        require(source_image(frontend->world())==before&&h.os.log.calls==calls,"Session release/retention restarted or shut down input");
    }
    frontend.reset();require(!weak.expired(),"input wrapper failed to retain platform alone");
    require(NativeIdentity(h.input->borrow_initialized())==identity,"sole wrapper borrow lost actual registry");
    h.input->shutdown_424930();h.input.reset();require(weak.expired(),"persistent input formed a strong cycle");h.os.clean();
}
void b4(const std::filesystem::path& assets){
    Harness h(assets);h.attach();h.start();seed(*h.world);
    auto frontend=std::make_unique<FrontendState>(h.world,h.platform);
    auto retained=std::make_unique<SessionWindowInput>(*frontend,h.handles);
    const NativeIdentity identity(retained->borrow_initialized());auto* raw=&h.world->source_input.acquisition;
    const auto before=source_image(*h.world);const auto calls=h.os.log.calls;
    frontend.reset();h.input.reset();
    require(&h.platform->state().source_input.acquisition==raw&&NativeIdentity(retained->borrow_initialized())==identity,"frontend destruction replaced borrowed input");
    require(source_image(*h.world)==before&&h.os.log.calls==calls,"frontend destruction entered source IO/reset");
    retained->shutdown_424930();const auto closed_calls=h.os.log.calls;
    {
        SessionWindowInput closed(*h.session,h.handles.instance);
        rejects([&]{(void)closed.borrow_initialized();},phase_error);
        rejects([&]{(void)closed.initialize_4249b0();},"DirectInput source startup already entered");
        rejects([&]{closed.shutdown_424930();},"DirectInput source shutdown requires one entered startup and no previous shutdown");
        require(source_image(*h.world)==before&&h.os.log.calls==closed_calls,"closed rebind reopened/reset native input");
    }
    retained.reset();h.close();h.os.clean();
}
void b5(const std::filesystem::path& assets){
    Harness h(assets);h.attach();h.start();std::weak_ptr<RaceSessionPlatform> weak=h.platform;
    h.session.reset(); // Perform ordinary level teardown before arming the world probe.
    // This is the actual world's last nontrivial member. Watch its allocation
    // being freed, not weak_ptr expiry (which precedes the owner's destructor).
    const auto previous_capacity=h.world->light_vertices.capacity();
    // assign() retains the level's 0x4800-byte allocation. MSVC manually
    // aligns large vectors, so data() is not their operator-delete pointer.
    // Replace that allocation BEFORE arming; still watch the actual member.
    std::vector<std::uint8_t>(257,0xa5).swap(h.world->light_vertices);
    require(h.world->light_vertices.capacity()<4096,"world probe requires an ordinary small allocation");
    std::cout<<"B5 previous_capacity="<<previous_capacity<<" probe_capacity="<<h.world->light_vertices.capacity()<<'\n';
    allocation_probe::arm(h.world->light_vertices.data());
    h.os.log.calls.clear();h.world.reset();h.platform.reset();
    require(!weak.expired(),"last input wrapper did not retain the platform");
    h.input.reset();
    require(weak.expired(),"input owner has a strong reference cycle");
    require(h.os.log.calls==std::vector<std::string>{"e0.release","b0.release","root.release","host.unload"},"residual destruction synthesized shutdown or changed release/unload order");
    std::cout<<"B5 freed="<<allocation_probe::freed.load()<<" freed_after_unload="<<allocation_probe::freed_after_unload.load()<<'\n';
    require(allocation_probe::freed&&allocation_probe::freed_after_unload,"world allocation died before input references/module");
    h.os.clean();
}
void b6(const std::filesystem::path& assets){
    Harness h(assets);seed(*h.world);const auto before=source_image(*h.world);
    h.attach();
    {
        FrontendState frontend(h.world,h.platform);SessionWindowInput alias(frontend,h.handles);
        require(source_image(*h.world)==before,"attachment erased FF/opaque/profile/keyboard/effects sentinels");
        const auto retained=unchanged_outside_cold(*h.world);const auto calls=h.os.log.calls;
        initialize_session_source_physical_input_45ec30(*h.session);
        require_cold_targets(*h.world);
        require(unchanged_outside_cold(*h.world)==retained&&h.os.log.calls==calls,"explicit cold reset reached external owners or native IO");
        // Real frontend alias assignment, then rebind the same platform.
        frontend.world().frontend_launch.input_bindings[2]={5,99};
        frontend.world().source_input.physical[3].bytes_1d8_1db[3]=0x6e;
        frontend.world().source_input.force_feedback.physical_words[1][7]=0x87654321;
        const auto assigned=source_image(frontend.world());SessionWindowInput rebound(frontend,h.handles);
        (void)h.platform->attach_window_input(h.handles.instance,h.handles.window);
        require(source_image(frontend.world())==assigned&&h.os.log.calls==calls,"rebind erased frontend assignments or entered source startup");
        rejects([&]{(void)rebound.borrow_initialized();},phase_error);
    }
    h.close();h.os.clean();
}

std::wstring quote(const std::wstring& value){
    std::wstring result=L"\"";std::size_t slashes=0;
    for(const auto c:value){
        if(c==L'\\'){++slashes;continue;}
        result.append(c==L'\"'?slashes*2+1:slashes,L'\\');slashes=0;result+=c;
    }
    result.append(slashes*2,L'\\');result+=L'\"';return result;
}
constexpr DWORD wrapper_death=0x53574f31,platform_death=0x53574f32;
std::atomic<DWORD> death_thread{};
void expect_death(const std::filesystem::path& assets,const wchar_t* mode,DWORD expected){
    std::wstring executable(32768,L'\0');
    const auto length=::GetModuleFileNameW(nullptr,executable.data(),static_cast<DWORD>(executable.size()));
    require(length!=0&&length<executable.size(),"cannot locate receiving executable");executable.resize(length);
    auto command=quote(executable)+L" "+quote(assets.wstring())+L" "+quote(mode);
    STARTUPINFOW startup{};startup.cb=static_cast<DWORD>(sizeof(startup));PROCESS_INFORMATION child{};
    require(::CreateProcessW(executable.c_str(),command.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,nullptr,&startup,&child)!=FALSE,"death child creation failed");
    (void)::CloseHandle(child.hThread);
    const auto wait=::WaitForSingleObject(child.hProcess,60000);DWORD code{};
    if(wait!=WAIT_OBJECT_0){(void)::TerminateProcess(child.hProcess,98);(void)::WaitForSingleObject(child.hProcess,10000);}
    const auto got=::GetExitCodeProcess(child.hProcess,&code);(void)::CloseHandle(child.hProcess);
    std::wcout<<L"B7 "<<mode<<L" wait="<<wait<<L" exit="<<code<<L" expected="<<expected<<L'\n';
    require(wait==WAIT_OBJECT_0&&got!=FALSE&&code==expected,"wrong-thread destruction did not reach the expected terminate handler");
}
int death_child(const std::filesystem::path& assets,bool wrapper){
    Harness h(assets);h.attach();
    std::set_terminate([]{::ExitProcess(97);});
    if(wrapper){
        // Other references stay on the original thread: only wrapper policy
        // can terminate this branch; platform destruction cannot mask it.
        std::thread worker([owner=std::move(h.input)]() mutable{
            death_thread=::GetCurrentThreadId();
            // UCRT stores the terminate handler in per-thread data.
            std::set_terminate([]{::ExitProcess(::GetCurrentThreadId()==death_thread.load()?wrapper_death:97);});
            owner.reset();
        });worker.join();
    }else{
        h.input.reset();h.session.reset();h.world.reset();
        std::thread worker([owner=std::move(h.platform)]() mutable{
            death_thread=::GetCurrentThreadId();
            std::set_terminate([]{::ExitProcess(::GetCurrentThreadId()==death_thread.load()?platform_death:97);});
            owner.reset();
        });worker.join();
    }
    return 99; // Normal return is a FAILED death case, never an accepted exit.
}
void b7(const std::filesystem::path& assets){
    Harness h(assets);h.attach();h.start();seed(*h.world);
    auto frontend=std::make_unique<FrontendState>(h.world,h.platform);
    const auto before=source_image(*h.world);const auto calls=h.os.log.calls;std::exception_ptr failure;
    std::thread worker([&]{try{
        rejects<std::logic_error>([&]{SessionWindowInput bad(*h.session,h.handles.instance);},platform_thread_error);
        rejects<std::logic_error>([&]{SessionWindowInput bad(*frontend,h.handles);},platform_thread_error);
        rejects<std::logic_error>([&]{SessionWindowInput bad(h.platform,h.world,h.handles);},platform_thread_error);
        rejects<std::logic_error>([&]{(void)h.input->initialize_4249b0();},platform_thread_error);
        rejects<std::logic_error>([&]{h.input->shutdown_424930();},platform_thread_error);
        rejects<std::logic_error>([&]{(void)h.input->borrow_initialized();},platform_thread_error);
        rejects<std::logic_error>([&]{(void)h.input->borrow_initialized(*h.session);},platform_thread_error);
        rejects<std::logic_error>([&]{h.input->require_session(*h.session);},platform_thread_error);
        rejects<std::logic_error>([&]{(void)h.platform->attach_window_input(h.handles.instance,h.handles.window);},platform_thread_error);
        rejects<std::logic_error>([&]{(void)h.platform->window_input(h.handles.instance,h.handles.window);},platform_thread_error);
    }catch(...){failure=std::current_exception();}});worker.join();if(failure)std::rethrow_exception(failure);
    require(source_image(*h.world)==before&&h.os.log.calls==calls,"foreign thread wrote source state or entered native IO");
    expect_death(assets,L"death-wrapper",wrapper_death);expect_death(assets,L"death-platform",platform_death);
    h.input->shutdown_424930();frontend.reset();h.close();h.os.clean();
}
void b8(const std::filesystem::path& assets){
    {
        Harness h(assets);h.attach();h.start();seed(*h.world);const auto before=source_image(*h.world);
        h.os.log.calls.clear();h.input->shutdown_424930();
        require(h.os.log.calls==std::vector<std::string>{"e0.unacquire","e0.release","root.release"},"explicit shutdown did not use actual source order");
        const auto closed=h.os.log.calls;
        rejects([&]{(void)h.input->borrow_initialized();},phase_error);
        require(source_image(*h.world)==before&&h.os.log.calls==closed,"closed borrow mutated the world");
        h.windows.destroy_first();
        rejects([&]{h.input->shutdown_424930();},"window input HWND or HINSTANCE is no longer valid for this lifetime");
        require(source_image(*h.world)==before&&h.os.log.calls==closed,"dead-window call entered source shutdown");
        h.close();require(h.os.log.calls==std::vector<std::string>{"e0.unacquire","e0.release","root.release","b0.release","host.unload"},"normal teardown repeated source shutdown");h.os.clean();
    }
    {
        Harness h(assets);h.attach();h.start();seed(*h.world);const auto before=source_image(*h.world);
        h.windows.destroy_first();h.os.log.calls.clear();
        struct UnwindRelease {
            Harness& owner;
            ~UnwindRelease(){owner.input.reset();owner.session.reset();}
        };
        try{UnwindRelease release{h};throw std::runtime_error("receiving unwind");}
        catch(const std::runtime_error& error){require(std::string_view(error.what())=="receiving unwind","unexpected unwind failure");}
        // Platform aliases still retain actual input; dead HWND cannot force
        // an implicit source shutdown while level/frontend objects disappear.
        require(source_image(*h.world)==before&&h.os.log.calls.empty(),"exceptional wrapper release reset/shut down input");
        h.close();require(h.os.log.calls==std::vector<std::string>{"e0.release","b0.release","root.release","host.unload"},"dead-HWND residual destruction synthesized Unacquire/shutdown");h.os.clean();
    }
}
}

int wmain(int argc,wchar_t** argv)try{
    require(argc==3,"usage: window_input_session_receiving <NEW retail root> <B1..B8|death-wrapper|death-platform>");
    const std::filesystem::path assets(argv[1]);const std::wstring_view name(argv[2]);
    if(name==L"death-wrapper")return death_child(assets,true);
    if(name==L"death-platform")return death_child(assets,false);
    using Case=void(*)(const std::filesystem::path&);
    constexpr std::array<Case,8> cases{b1,b2,b3,b4,b5,b6,b7,b8};
    require(name.size()==2&&name[0]==L'B'&&name[1]>=L'1'&&name[1]<=L'8',"unknown Session case");
    cases[static_cast<std::size_t>(name[1]-L'1')](assets);
    std::cout<<"PASS B"<<static_cast<unsigned>(name[1]-L'0')<<" actual_Session_used=true real_hidden_windows=true DirectInput_boundary=scripted GPU_runs=0 original_execution_cases=0\n";
    return 0;
}catch(const std::exception& error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
