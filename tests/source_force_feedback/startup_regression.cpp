// SCRIPTED COM RECEIVING ONLY. No actual device or window is created here.
#include "fixture_com.hpp"
#include "receiving_owner.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include <algorithm>
#include <iostream>

// Exactly seven native API names are replaced AFTER SDK declarations. This
// includes the actual sealed production TU; its C++ body is not reimplemented.
#define LoadLibraryExW FixtureLoadLibraryExW
#define FreeLibrary FixtureFreeLibrary
#define GetProcAddress FixtureGetProcAddress
#define GetCurrentProcessId FixtureGetCurrentProcessId
#define GetCurrentThreadId FixtureGetCurrentThreadId
#define GetWindowThreadProcessId FixtureGetWindowThreadProcessId
#define GetAncestor FixtureGetAncestor
#include SGN_FORCE_FEEDBACK_STARTUP_SOURCE
#undef LoadLibraryExW
#undef FreeLibrary
#undef GetProcAddress
#undef GetCurrentProcessId
#undef GetCurrentThreadId
#undef GetWindowThreadProcessId
#undef GetAncestor

namespace {
using namespace sgn::retail;
using fixture::require;
using Calls=std::vector<std::string>;
using Startup=Win32MenuDirectInputStartup;
unsigned cases{},variants{};
template<class F> void check(const char* name,F action) {
    action(); ++cases; std::cout<<"PASS "<<name<<'\n';
}
std::unique_ptr<Startup> start(fixture::Scenario& s,SourceInputStorage& owner) {
    fixture::active=&s;
    return std::make_unique<Startup>(owner.acquisition,reinterpret_cast<std::uintptr_t>(fixture::instance()),
        reinterpret_cast<std::uintptr_t>(fixture::window()));
}
Calls prefix(bool second=false) {
    Calls result{"host.load","host.symbol","factory500","root.enum101"};
    if (second) result.push_back("root.enum1");
    return result;
}
Calls branch(bool second,unsigned id=0) {
    const auto b="b"+std::to_string(id), e="e"+std::to_string(id);
    if (second) return {"root.create"+std::to_string(id),b+".format",b+".coop6",b+".range0",b+".range4",
        b+".deadzone0",b+".deadzone4",b+".qi",b+".info"};
    return {"root.create"+std::to_string(id),b+".qi",e+".coop5",e+".format",e+".autocenter",e+".effects",
        e+".range0",e+".range4",e+".deadzone0",e+".deadzone4",b+".info"};
}
void append(Calls& target,const Calls& tail) { target.insert(target.end(),tail.begin(),tail.end()); }
void exact(const fixture::Scenario& s,const Calls& expected) {
    if (s.log.calls!=expected) {
        for (const auto& call:s.log.calls) std::cerr<<call<<'\n';
        throw std::runtime_error("COM call order differs from expected source trace");
    }
}
template<class F> void host_error(F action) {
    bool caught=false; try { action(); } catch (const sgn::FormatError&) { caught=true; }
    require(caught,"expected explicit host error, received normal result");
}
struct Keys final : MenuKeyboardAcquisition {
    unsigned calls{};
    void acquire_45ec00(std::span<std::uint8_t,256> bytes) override { ++calls; bytes[13]=0x81; }
};
}

int main() try {
    check("factory-nonzero-and-enumeration-HRESULT-rules",[] {
        for (const auto hr:{S_FALSE,E_FAIL}) {
            fixture::Scenario s; SourceInputStorage owner; s.log.results["factory500"]=hr;
            auto input=start(s,owner); require(input->initialize_4249b0()==0,"nonzero factory accepted");
            require(input->registry_for_acquisition().device_count==0,"factory failure invented devices");
            require(owner.acquisition.upstream[0].words[0]==1 && owner.acquisition.upstream[0].words[1]==1,
                "source keyboard initialization omitted on factory failure");
            input->shutdown_424930(); input.reset(); s.clean(); ++variants;
        }
        for (const auto second:{S_OK,S_FALSE,E_FAIL}) {
            fixture::Scenario s; SourceInputStorage owner; s.log.results["root.enum101"]=E_FAIL;
            s.log.results["root.enum1"]=second;
            auto input=start(s,owner); require(input->initialize_4249b0()==(second==0 ? 1u:0u),"wrong enum return rule");
            exact(s,prefix(true)); input->shutdown_424930(); input.reset(); s.clean(); ++variants;
        }
    });
    check("first-branch-native-adapter-acquisition-and-source-vs-host-release",[] {
        fixture::Scenario s; SourceInputStorage owner; auto& basic=s.add(0); auto& device=*basic.qi_target;
        auto input=start(s,owner);
        basic.on_info=[&]{require(owner.acquisition.upstream[1].words[0]==1 && owner.acquisition.upstream[1].words[1]==0 &&
            input->source_state().cursor_576550==0x572ce8u,"upstream active publication must precede GetDeviceInfo");};
        require(input->initialize_4249b0()==1,"first startup");
        auto expected=prefix(); append(expected,branch(false)); exact(s,expected);
        const auto& registry=input->registry_for_acquisition();
        require(registry.device_count==1 && basic.refs==1 && device.refs==1,"successful callback released a source ref");
        require(owner.acquisition.upstream[1].words[0]==1 && owner.acquisition.upstream[1].words[1]==2 &&
            owner.acquisition.upstream[1].words[2]==2 && input->source_state().flag_576b74==1,"first publication");
        for (unsigned i=0;i<4;++i) require(input->source_state().effect_guids_576558[i]==
            std::array<std::uint32_t,4>{0x13541c27u+i,0x11d08e33u,0xa000d09au,0x356ea0c9u},"source effect GUID initialization");
        const std::int32_t x=1226,y=-4902;
        std::memcpy(device.packet.data(),&x,4); std::memcpy(device.packet.data()+4,&y,4); device.packet[48]=std::byte{0x7f};
        s.log.results["e0.poll"]=E_FAIL; //ignored even through the actual native adapter
        SessionMenuInputState menu; RaceInitializationState keyboard; Keys keys; std::uint32_t pause=7;
        keyboard.fields_62d480.fill(0x12345678u); const auto old=keyboard.fields_62d480;
        acquire_publish_devices_prefix_45f210(owner.acquisition,menu,keyboard,pause,keys,registry);
        const auto views=menu_physical_producer_views(owner.acquisition,menu);
        const std::uint32_t scene=1;
        produce_menu_input_40d860({scene,menu_keyboard_bytes(keyboard),views,receiving::fixture_selectors(owner),
            owner.acquisition.source_device_pointer_576b6c});
        require(keys.calls==1 && pause==0 && keyboard.fields_62dcf0==old,"combined prefix history");
        require(owner.acquisition.upstream[1].words[6]==1250 &&
            owner.acquisition.upstream[1].words[7]==std::bit_cast<std::uint32_t>(std::int32_t{-5000}) &&
            menu.edges[1].activate==1 && menu.edges[1].up==1,"actual native adapter did not reach menu owners");
        append(expected,{"e0.poll","e0.state"}); exact(s,expected);
        const auto retained=owner.acquisition.upstream[1].words;
        s.log.results["e0.state"]=S_FALSE; device.packet.fill(std::byte{0xcc});
        acquire_publish_devices_prefix_45f210(owner.acquisition,menu,keyboard,pause,keys,registry);
        require(owner.acquisition.upstream[1].words==retained,"native adapter failure published poisoned packet");
        append(expected,{"e0.poll","e0.state","e0.acquire"}); exact(s,expected);
        s.log.results["e0.state"]=S_OK; device.packet.fill(std::byte{0});
        acquire_publish_devices_prefix_45f210(owner.acquisition,menu,keyboard,pause,keys,registry);
        require(owner.acquisition.upstream[1].words[18]==0 && owner.acquisition.upstream[1].words[26]==0x7f,
            "native adapter recovery lost last successful button history");
        append(expected,{"e0.poll","e0.state"}); exact(s,expected);
        auto* original_root=input->source_state().root_576b68;
        auto* original_device=input->source_state().records[0].device;
        input->shutdown_424930(); append(expected,{"e0.unacquire","e0.release","root.release"}); exact(s,expected);
        require(basic.refs==1 && device.refs==0 && s.root.refs==0,"source shutdown consumed basic ref");
        require(input->source_state().root_576b68==original_root && input->source_state().records[0].device==original_device &&
            registry.device_count==1 && input->source_state().cursor_576550==0x57328cu,"source shutdown cleared retained identities");
        host_error([&]{(void)input->registry_for_acquisition();});
        input->finalize_host(); expected.push_back("b0.release"); exact(s,expected);
        input->finalize_host(); exact(s,expected); //host-only idempotence
        input.reset(); expected.push_back("host.unload"); exact(s,expected); s.clean();
    });
    check("second-branch-distinct-basic-receiver-and-no-effects",[] {
        fixture::Scenario s; SourceInputStorage owner; auto& basic=s.add(0,true); basic.subtype=3;
        auto input=start(s,owner); require(input->initialize_4249b0()==1,"second startup");
        auto expected=prefix(true); append(expected,branch(true)); exact(s,expected);
        require(input->registry_for_acquisition().device_count==1 && owner.acquisition.upstream[1].words[1]==5 &&
            owner.acquisition.upstream[1].words[2]==0 && input->source_state().flag_576b74==0 && s.log.effect_callbacks==0,
            "second branch borrowed first-branch behavior");
        input->shutdown_424930(); input.reset(); s.clean();
    });
    check("first-branch-all-checked-failure-stages-and-callback-BOOL",[] {
        const std::array<std::pair<const char*,BOOL>,8> rows{{{"root.create0",TRUE},{"b0.qi",FALSE},
            {"e0.coop5",TRUE},{"e0.format",TRUE},{"e0.range0",FALSE},{"e0.range4",FALSE},
            {"e0.deadzone0",FALSE},{"e0.deadzone4",FALSE}}};
        for (const auto& [stage,keep]:rows) for (const auto hr:{S_FALSE,E_FAIL}) {
            fixture::Scenario s; SourceInputStorage owner; (void)s.add(0); s.log.results[stage]=hr;
            auto input=start(s,owner); require(input->initialize_4249b0()==1,"empty second enumeration should succeed");
            auto expected=prefix(); const auto full=branch(false); const auto stop=std::find(full.begin(),full.end(),stage);
            expected.insert(expected.end(),full.begin(),stop+1);
            if (std::string(stage)!="root.create0") expected.push_back("b0.release");
            expected.push_back("root.enum1"); exact(s,expected);
            require(s.root.callback_results==std::vector<BOOL>{keep} && input->registry_for_acquisition().device_count==0 &&
                owner.acquisition.upstream[1].words[0]==0,"first failure callback/status publication");
            input->shutdown_424930(); input.reset(); s.clean(); ++variants;
        }
    });
    check("second-branch-all-checked-failure-stages-and-callback-BOOL",[] {
        const std::array<std::pair<const char*,BOOL>,8> rows{{{"root.create0",TRUE},{"b0.format",TRUE},
            {"b0.coop6",TRUE},{"b0.range0",FALSE},{"b0.range4",FALSE},{"b0.deadzone0",FALSE},
            {"b0.deadzone4",FALSE},{"b0.qi",FALSE}}};
        for (const auto& [stage,keep]:rows) for (const auto hr:{S_FALSE,E_FAIL}) {
            fixture::Scenario s; SourceInputStorage owner; (void)s.add(0,true); s.log.results[stage]=hr;
            auto input=start(s,owner); require(input->initialize_4249b0()==1,"second enum result");
            auto expected=prefix(true); const auto full=branch(true); const auto stop=std::find(full.begin(),full.end(),stage);
            expected.insert(expected.end(),full.begin(),stop+1);
            if (std::string(stage)!="root.create0") expected.push_back("b0.release");
            exact(s,expected);
            require(s.root.callback_results==std::vector<BOOL>{keep} && input->registry_for_acquisition().device_count==0 &&
                input->source_state().flag_576b74==0,"second failure publication");
            input->shutdown_424930(); input.reset(); s.clean(); ++variants;
        }
    });
    check("ignored-autocenter-effects-and-first-enumeration-results",[] {
        fixture::Scenario s; SourceInputStorage owner; (void)s.add(0);
        s.log.results["e0.autocenter"]=S_FALSE; s.log.results["e0.effects"]=E_FAIL; s.log.results["root.enum101"]=E_FAIL;
        auto input=start(s,owner); require(input->initialize_4249b0()==1,"ignored result became fatal");
        auto expected=prefix(); append(expected,branch(false)); exact(s,expected);
        require(s.log.effect_callbacks==2 && input->source_state().flag_576b74==1,"effect callback return/copy branch skipped");
        input->shutdown_424930(); input.reset(); s.clean();
    });
    check("get-device-info-subtype-and-nonzero-result",[] {
        for (const auto subtype:{0u,1u,5u,6u,7u,255u}) for (const auto hr:{S_OK,S_FALSE,E_FAIL}) {
            fixture::Scenario s; SourceInputStorage owner; s.add(0,true).subtype=subtype; s.log.results["b0.info"]=hr;
            auto input=start(s,owner); require(input->initialize_4249b0()==1,"GetDeviceInfo result changed startup result");
            require(owner.acquisition.upstream[1].words[1]==(hr==0 && subtype==6 ? 2u:5u),"wrong subtype byte/default kind");
            input->shutdown_424930(); input.reset(); s.clean(); ++variants;
        }
    });
    check("uncounted-QI-overwrite-retained-until-host-finalizer",[] {
        fixture::Scenario s; SourceInputStorage owner; auto& first=s.add(0); auto& second=s.add(1);
        s.log.results["e0.format"]=E_FAIL;
        auto input=start(s,owner); require(input->initialize_4249b0()==1,"overwrite startup");
        require(input->registry_for_acquisition().device_count==1 && input->source_state().records[0].device==second.qi_target &&
            first.refs==0 && first.qi_target->refs==1,"orphan QI reference released or wrong slot selected");
        input->shutdown_424930(); require(first.qi_target->refs==1,"source shutdown reached uncounted QI");
        input.reset(); s.clean();
    });
    check("equal-basic-and-extended-pointer-still-owns-two-references",[] {
        fixture::Scenario s; SourceInputStorage owner; auto& basic=s.add(0,false,true);
        auto input=start(s,owner); require(input->initialize_4249b0()==1 && basic.refs==2,"QI ref collapsed by equal pointer");
        input->shutdown_424930(); require(basic.refs==1,"source shutdown released both identical references");
        input.reset(); require(std::count(s.log.calls.begin(),s.log.calls.end(),"b0.release")==2,"wrong equal-pointer Release count"); s.clean();
    });
    check("source-shutdown-twelve-fixture-effects-clear-pointers-retain-payload",[] {
        fixture::Scenario s; SourceInputStorage owner; (void)s.add(0); auto input=start(s,owner);
        require(input->initialize_4249b0()==1,"effect fixture startup");
        // Deliberate fixture write into a non-const object's exposed const view.
        // No production effect creation or hardware force feedback is claimed.
        auto& state=const_cast<MenuDirectInputStartupState&>(input->source_state());
        std::array<std::unique_ptr<fixture::Effect>,12> effects;
        for (unsigned i=0;i<12;++i) {
            effects[i]=std::make_unique<fixture::Effect>(s.log,"effect"+std::to_string(i));
            state.records[0].effects[i].effect=effects[i].get(); state.records[0].effects[i].payload.fill(0xface0000u+i);
            effects[i]->on_release=[&,i]{require(state.records[0].effects[i].effect==effects[i].get(),
                "effect pointer cleared before Release returned");};
        }
        const auto begin=s.log.calls.size(); input->shutdown_424930();
        for (unsigned i=0;i<12;++i) {
            require(s.log.calls[begin+i]=="effect"+std::to_string(i)+".release" && effects[i]->refs==0 &&
                state.records[0].effects[i].effect==nullptr,"effect Release order/pointer clear");
            for (const auto word:state.records[0].effects[i].payload) require(word==0xface0000u+i,"effect payload cleared during shutdown");
        }
        require(s.log.calls[begin+12]=="e0.unacquire" && s.log.calls[begin+13]=="e0.release" &&
            s.log.calls[begin+14]=="root.release","device/root order after effects");
        input.reset(); s.clean();
    });
    check("host-finalizer-alone-does-not-fabricate-source-Unacquire",[] {
        fixture::Scenario s; SourceInputStorage owner; (void)s.add(0,true); auto input=start(s,owner);
        require(input->initialize_4249b0()==1,"host finalizer startup"); const auto begin=s.log.calls.size();
        input->finalize_host(); require(Calls(s.log.calls.begin()+static_cast<std::ptrdiff_t>(begin),s.log.calls.end())==
            Calls{"e0.release","b0.release","root.release"},"host finalizer release order or fabricated source call");
        host_error([&]{input->shutdown_424930();}); input.reset(); s.clean();
    });
    check("single-lifetime-guards-and-tenth-eleventh-device-domain",[] {
        for (const unsigned count:{10u,11u}) {
            fixture::Scenario s; SourceInputStorage owner; for (unsigned i=0;i<count;++i) (void)s.add(i);
            owner.acquisition.physical[0].word_8=0xabcddcbau;
            auto input=start(s,owner);
            if (count==10) {
                require(input->initialize_4249b0()==1 && input->registry_for_acquisition().device_count==10,"tenth device rejected");
                require(owner.acquisition.upstream[9].words[0]==1 && owner.acquisition.upstream[9].words[1]==2,"ninth upstream missing");
            } else host_error([&]{(void)input->initialize_4249b0();});
            require(std::find(s.log.calls.begin(),s.log.calls.end(),"b9.info")==s.log.calls.end() &&
                owner.acquisition.physical[0].word_8==0xabcddcbau,"tenth device wrote beyond nine upstream device slots");
            host_error([&]{(void)input->initialize_4249b0();});
            input->shutdown_424930(); const auto after=s.log.calls;
            host_error([&]{input->shutdown_424930();}); require(s.log.calls==after,"repeated shutdown touched COM");
            input.reset(); s.clean(); ++variants;
        }
    });
    check("shutdown-rereads-device-pointer-and-count-after-Unacquire",[] {
        fixture::Scenario s; SourceInputStorage owner; auto& a=s.add(0); auto& b=s.add(1);
        auto input=start(s,owner); require(input->initialize_4249b0()==1,"reread fixture startup");
        auto& state=const_cast<MenuDirectInputStartupState&>(input->source_state());
        auto& registry=const_cast<MenuDirectInputRegistry&>(input->registry_for_acquisition());
        a.qi_target->on_unacquire=[&]{state.records[0].device=b.qi_target; registry.device_count=1;};
        const auto begin=s.log.calls.size(); input->shutdown_424930();
        require(Calls(s.log.calls.begin()+static_cast<std::ptrdiff_t>(begin),s.log.calls.end())==
            Calls{"e0.unacquire","e1.release","root.release"},"source rereads were replaced with stale values");
        require(a.qi_target->refs==1 && b.qi_target->refs==0,"reread receiver mismatch"); input.reset(); s.clean();
    });
    check("callback-host-error-rethrown-after-EnumDevices-not-source-success",[] {
        fixture::Scenario s; SourceInputStorage owner; s.add(0).null_qi_success=true;
        auto input=start(s,owner); host_error([&]{(void)input->initialize_4249b0();});
        require(s.root.callback_results==std::vector<BOOL>{FALSE} &&
            std::find(s.log.calls.begin(),s.log.calls.end(),"root.enum1")==s.log.calls.end(),"host failure became source fallback");
        input->shutdown_424930(); input.reset(); s.clean();
    });
    std::cout<<"cases="<<cases<<" parameter_variants="<<variants
        <<" scripted_COM=1 actual_Win32_calls=0 real_analog_coverage=0\n";
    return 0;
} catch (const std::exception& error) { std::cerr<<"FAIL "<<error.what()<<'\n'; return 1; }
