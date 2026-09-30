// PREPARED, NOT EXECUTED. All game children are actual integrated sources.
// Only native DirectInput COM, seven window/module APIs and keyboard IO are scripted.
#include "fixture_com.hpp"
#include "sgn/source_force_feedback.hpp"
#include "sgn/source_input_dispatch.hpp"
#include "sgn/menu_directinput_startup.hpp"
#include "sgn/race_session.hpp"
#include "sgn/race_session_pause.hpp"
#include <algorithm>
#include <cfenv>
#include <cmath>
#include <iostream>
#include <limits>
#include <utility>

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
unsigned cases{};
template<class F>void check(const char* name,F action){action();++cases;std::cout<<"PASS "<<name<<'\n';}
template<class F>void host_error(F action){bool caught=false;try{action();}catch(const sgn::FormatError&){caught=true;}
    require(caught,"expected a reached host-domain error");}
std::uint32_t bits(float v){return std::bit_cast<std::uint32_t>(v);}

#include "receiving_session_fixture.hpp"

struct Created {
    unsigned record,slot;std::array<std::uint32_t,4> guid;
    fixture::EffectParameters parameters;
};
struct FeedbackFixture:Fixture {
    fixture::Scenario os;
    std::unique_ptr<Win32MenuDirectInputStartup> startup;
    std::vector<Created> created;
    HRESULT create_result{S_OK};bool retain_failure_output{};
    fixture::Effect* repeat_receiver{};
    std::function<void(unsigned,unsigned,LPCDIEFFECT,LPDIRECTINPUTEFFECT*)> before_create,after_create;
    explicit FeedbackFixture(RaceSession& owner,unsigned devices=1):Fixture(owner){
        fixture::active=&os;
        for(unsigned i=0;i<devices;++i)(void)os.add(i);
        startup=std::make_unique<Win32MenuDirectInputStartup>(input.acquisition,
            reinterpret_cast<std::uintptr_t>(fixture::instance()),reinterpret_cast<std::uintptr_t>(fixture::window()));
        require(startup->initialize_4249b0()==1,"actual source startup failed in fixture");
        for(unsigned record_index=0;record_index<devices;++record_index){
            auto& receiver=*os.root.devices[record_index]->qi_target;
            receiver.on_create_effect=[&,record_index](REFGUID guid,LPCDIEFFECT p,LPDIRECTINPUTEFFECT* output,LPUNKNOWN outer){
                require(!outer,"source CreateEffect outer is not null");
                auto& bank=startup->borrow_force_feedback_state(input.acquisition).records;
                unsigned slot=12;
                for(unsigned i=0;i<12;++i)if(output==&bank[record_index].effects[i].effect)slot=i;
                require(slot<12,"CreateEffect output must be the actual typed source slot");
                std::array<std::uint32_t,4> words{};std::memcpy(words.data(),&guid,16);
                created.push_back({record_index,slot,words,fixture::EffectParameters(p)});
                const auto label="fx"+std::to_string(record_index)+"."+std::to_string(slot)+"."+std::to_string(created.size());
                (void)os.log.call("create"+std::to_string(record_index)+"."+std::to_string(slot));
                if(before_create)before_create(record_index,slot,p,output);
                if(SUCCEEDED(create_result)){
                    auto& returned_effect=repeat_receiver?*repeat_receiver:os.add_effect(label);
                    ++returned_effect.refs;*output=&returned_effect;
                }else if(!retain_failure_output)*output=nullptr;
                if(after_create)after_create(record_index,slot,p,output);
                return create_result;
            };
        }
        s.clock.phase=2;s.scene_mode=0;s.frame_pause=0;
        s.current_slot=4;s.frontend_launch.input_bindings[1].vehicle=4;
        input.physical[1].feedback_mode=2;
        input.force_feedback.selected_data_4d5b24=0x62d738u+0x1dcu;
        s.random.state=1;
    }
    auto world(){return SourceForceFeedbackWorld{session,input,*startup};}
    auto& words(unsigned physical=1){return input.force_feedback.physical_words[physical];}
    fixture::Effect& effect(unsigned slot,unsigned record=0){
        auto* p=startup->source_state().records[record].effects[slot].effect;
        require(p!=nullptr,"fixture requires an actually created effect");
        return *static_cast<fixture::Effect*>(p);
    }
    void prepare(){auto w=world();force_feedback_4240e0(w,1);}
    void finish(){startup->shutdown_424930();startup.reset();os.clean();}
};
struct Keys final:MenuKeyboardAcquisition {
    unsigned calls{};std::array<std::uint8_t,256> next{};
    void acquire_45ec00(std::span<std::uint8_t,256> out)override{++calls;std::copy(next.begin(),next.end(),out.begin());}
};
}

int main(int argc,char** argv)try{
    require(argc==2,"supply NEW retail data root");
    require(std::fegetround()==FE_TONEAREST,"receiving requires round nearest");
    RaceSessionLaunch launch;launch.mode=SessionMode::normal_race;launch.master_gain=0;
    RaceSession session(argv[1],launch);
    check("source-cold-words-and-no-mode-zero-or-one-write",[&]{
        const SourceInputStorage image;const auto& ff=image.force_feedback; //actual untouched constructor output
        require(ff.selected_data_4d5b24==0&&ff.initialized_576b70==0&&ff.axes_count_477648==1&&
            ff.direction_576b78==std::array<std::uint32_t,2>{0,0}&&
            ff.axes_477640==std::array<std::uint32_t,2>{0,4}&&ff.gain_47764c==10000&&ff.deadband_477650==100,"PE data seeds");
        for(const auto& record:ff.physical_words)for(auto word:record)require(word==0,"PE physical zeros");
        FeedbackFixture f(session);auto w=f.world();f.input.force_feedback.selected_data_4d5b24=0xdeadbeef;f.s.current_slot=5;
        const auto before=f.input.acquisition.source_device_pointer_576b6c;
        for(auto mode:{0u,1u}){f.input.physical[1].feedback_mode=mode;force_feedback_410a10(w,1);}
        require(f.s.current_slot==5&&f.input.force_feedback.selected_data_4d5b24==0xdeadbeef&&
            f.input.acquisition.source_device_pointer_576b6c==before&&f.created.empty(),"skipped FF mode changed source owners");
        f.finish();
    });
    check("create-order-source-descriptors-GUIDs-and-record-zero-starts",[&]{
        FeedbackFixture f(session);auto w=f.world();f.s.scene_mode=2;
        f.before_create=[&](unsigned r,unsigned slot,LPCDIEFFECT,LPDIRECTINPUTEFFECT*){
            require(r==0&&f.input.force_feedback.initialized_576b70==1&&f.s.current_slot==4&&
                f.input.acquisition.source_device_pointer_576b6c==0x62d788u&&
                f.input.force_feedback.selected_data_4d5b24==0x62d914u,"source publications must precede COM");
            const auto& raw=f.startup->source_state().records[r].effects[slot].payload;
            require(raw[16]==52&&raw[17]==18&&raw[24]==0x477640&&raw[25]==0x576b78,"source descriptor fields");
        };
        const auto begin=f.os.log.calls.size();force_feedback_410a10(w,1);
        require(f.created.size()==4,"four actual creations");
        const std::array<unsigned,4> slots{0,4,5,6};
        const std::array<std::uint32_t,4> guids{0x13541c27,0x13541c23,0x13541c22,0x13541c24};
        for(unsigned i=0;i<4;++i){const auto& c=f.created[i];
            require(c.record==0&&c.slot==slots[i]&&c.guid==std::array<std::uint32_t,4>{guids[i],0x11d08e33,0xa000d09a,0x356ea0c9},"source GUID order");
            require(c.parameters.header==std::array<DWORD,9>{sizeof(DIEFFECT_DX5),18,i==2?100000u:0xffffffffu,
                0,10000,0xffffffffu,0,1,i==0?24u:16u},"source-to-native header");
            require(c.parameters.axes==std::vector<DWORD>{0}&&c.parameters.directions==std::vector<LONG>{0},"pointer relocation values");
        }
        require(f.created[0].parameters.type==std::vector<DWORD>{0,10000,10000,10000,10000,100},"condition payload");
        require(f.created[1].parameters.type==std::vector<DWORD>{10000,0,0,100000},"periodic default payload");
        const Calls expected{"create0.0","create0.4","create0.5","create0.6","fx0.0.1.start","fx0.4.2.start",
            "fx0.5.3.start","fx0.6.4.start","fx0.4.2.set","fx0.6.4.set"};
        require(Calls(f.os.log.calls.begin()+static_cast<std::ptrdiff_t>(begin),f.os.log.calls.end())==expected,"actual source call order");
        require(f.words()[6]==0&&f.words()[3]==0,"quiet path zeros");f.finish();
    });
    check("global-once-failure-retains-flag-and-exact-HRESULT-result",[&]{
        for(auto hr:{S_FALSE,E_FAIL}){
            FeedbackFixture f(session);f.create_result=hr;auto w=f.world();f.prepare();
            require(f.created.size()==4&&f.input.force_feedback.initialized_576b70==1,"global once on COM failure");
            force_feedback_4240e0(w,2);require(f.created.size()==4,"source once flag retried creation");
            const std::array<std::uint32_t,6> condition{0,1,2,3,4,5};
            require(!force_feedback_423d30(w,0,0,condition),"CreateEffect success must be exactly zero");
            require(force_feedback_423e40(w,0)==1,"423E40 ignores child HRESULT");f.finish();
        }
    });
    check("periodic-start-rereads-global-cursor-and-initializer-starts-record-zero",[&]{
        FeedbackFixture f(session,2);auto w=f.world();auto& bank=f.startup->borrow_force_feedback_state(f.input.acquisition);
        force_feedback_423ff0(w,0);auto& prior=f.effect(4);
        bank.cursor_576550=0x572ce8u+2u*0x5a4u; //different empty record before COM
        f.before_create=[&](unsigned,unsigned slot,LPCDIEFFECT,LPDIRECTINPUTEFFECT*){
            if(slot==4)bank.cursor_576550=0x572ce8u+2u*0x5a4u;
        };
        f.after_create=[&](unsigned,unsigned slot,LPCDIEFFECT,LPDIRECTINPUTEFFECT*){
            if(slot==4)bank.cursor_576550=0x572ce8u; //external return changes the actual source cursor
        };
        const auto begin=f.os.log.calls.size();force_feedback_4240e0(w,2);
        require(f.created[1].record==1&&f.created[2].record==1,"physical two selects native record one");
        const auto count=std::count(f.os.log.calls.begin()+static_cast<std::ptrdiff_t>(begin),f.os.log.calls.end(),prior.label+".start");
        require(count==2,"cursor record Start and first-record Start must both reach prior effect");
        require(std::count(f.os.log.calls.begin(),f.os.log.calls.end(),f.effect(4,1).label+".start")==0,"new periodic was incorrectly substituted for cursor");
        f.finish();
    });
    check("physical-zero-error-keeps-preceding-source-publications",[&]{
        FeedbackFixture f(session);auto w=f.world();f.input.physical[0].feedback_mode=2;
        f.s.frontend_launch.input_bindings[0].vehicle=3;
        host_error([&]{force_feedback_410a10(w,0);});
        require(f.input.force_feedback.initialized_576b70==1&&f.s.current_slot==3&&
            f.input.force_feedback.selected_data_4d5b24==0x62d738&&
            f.input.acquisition.source_device_pointer_576b6c==0x62d5ac&&f.created.empty(),"invalid source address was clamped or writes rolled back");
        f.finish();
    });
    check("condition-math-same-selected-vehicle-and-real-set-parameters",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();auto& p=f.s.vehicles.physics[4];
        p.basis_x={1,0,0};p.acceleration={0.5f,0,0};p.wheels[2].load=2;p.wheels[2].reference_load=4;p.wheels[2].surface_grip=0.5f;
        force_feedback_410780(w,1);
        require(f.words()[0]==bits(3500)&&f.effect(0).parameters.back().type==std::vector<DWORD>{0,3500,3500,10000,10000,100}&&
            f.effect(0).set_flags.back()==0x20000100u,"selected vehicle arithmetic/condition output");
        p.acceleration.x=2;force_feedback_410780(w,1);require(f.words()[0]==bits(10000),"upper condition clamp");
        p.acceleration.x=0;p.wheels[2].load=-2;force_feedback_410780(w,1);
        require(f.words()[0]==bits(-500)&&f.effect(0).parameters.back().type[1]==0xfffffe0cu,"source has no lower clamp");f.finish();
    });
    check("full-normal-410A10-runs-four-real-leaves-in-order",[&]{
        FeedbackFixture f(session);auto w=f.world();auto& p=f.s.vehicles.physics[4];
        p.basis_x={1,0,0};p.acceleration={0.5f,0,0};
        p.wheels[2].load=2;p.wheels[2].reference_load=4;p.wheels[2].surface_grip=0.5f;
        for(auto& wheel:f.s.vehicles.contact[4].wheels)wheel.type=9;
        f.s.vehicles.contact[4].wheels[2].type=1;p.speed=10;p.rpm=2048;
        f.s.vehicles.parameters[4].rpm_limit=8000;f.s.vehicles.presentation[4].idle_hold=0;
        f.s.vehicles.contact[4].direction_magnitude=0.1f;
        force_feedback_410a10(w,1);
        require(f.words()[0]==bits(3500)&&f.words()[6]==bits(500)&&f.words()[7]==bits(50000)&&
            f.words()[3]==bits(6.25f)&&f.words()[1]==0&&f.s.random.state==0x29e2c0,"normal composition state");
        require(f.effect(5).parameters.back().type==std::vector<DWORD>{500,0,498,100000},"impact equality threshold");
        require(Calls(f.os.log.calls.end()-4,f.os.log.calls.end())==
            Calls{"fx0.0.1.set","fx0.4.2.set","fx0.5.3.set","fx0.6.4.set"},"normal source child order");
        f.finish();
    });
    check("each-410A10-quiet-gate-only-runs-engine-and-surface-clears",[&]{
        for(unsigned gate=0;gate<3;++gate){
            FeedbackFixture f(session);auto w=f.world();
            if(gate==0)f.s.scene_mode=2;else if(gate==1)f.s.frame_pause=1;else f.s.clock.phase=7;
            f.words()[0]=123;f.words()[1]=456;f.words()[6]=bits(100);f.words()[3]=bits(200);
            force_feedback_410a10(w,1);
            require(f.words()[0]==123&&f.words()[1]==456&&f.words()[6]==0&&f.words()[3]==0&&f.s.random.state==1,
                "quiet source gates touched active behavior");
            require(f.effect(0).parameters.empty()&&f.effect(5).parameters.empty()&&
                f.effect(4).parameters.size()==1&&f.effect(6).parameters.size()==1,"quiet source children");f.finish();
        }
    });
    check("engine-period-stored-rounding-caps-idle-divisor-and-zero-range",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();auto& p=f.s.vehicles.physics[4];
        f.s.vehicles.parameters[4].rpm_limit=8000;p.rpm=2048;f.s.vehicles.presentation[4].idle_hold=0;
        force_feedback_410800(w,1);
        require(f.words()[7]==bits(50000)&&f.words()[6]==bits(500)&&
            f.effect(4).parameters.back().type==std::vector<DWORD>{500,0,0,50000}&&f.effect(4).set_flags.back()==0x100,"engine output");
        f.s.vehicles.parameters[4].rpm_limit=20000;force_feedback_410800(w,1);require(f.words()[7]==bits(30000),"period low cap");
        f.s.vehicles.parameters[4].rpm_limit=0;force_feedback_410800(w,1);require(f.words()[7]==bits(75000),"masked divide period high cap");
        f.s.vehicles.presentation[4].idle_hold=0xffffffffu;force_feedback_410800(w,1);require(f.words()[6]==bits(10000),"source wrapping idle divisor and masked divide");
        f.s.vehicles.presentation[4].idle_hold=0xfffffffeu;force_feedback_410800(w,1);require(f.words()[6]==bits(-500),"idle signed divisor");
        p.rpm=4096;force_feedback_410800(w,1);require(f.words()[6]==0,"rpm equality selects zero magnitude");f.finish();
    });
    check("impact-consumption-shared-RNG-and-post-child-clear",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();auto& impact=f.s.vehicles.contact[4].direction_magnitude;
        impact=0.05f;force_feedback_4108c0(w,1);
        require(impact==0&&f.words()[1]==bits(0.05f)&&f.s.random.state==1&&f.effect(5).parameters.empty(),"subthreshold retains payload but consumes contact");
        impact=3;f.effect(5).on_set=[&](LPCDIEFFECT,DWORD){
            require(impact==0&&f.words()[1]==bits(10000)&&f.s.random.state==0x29e2c0,"source consumption/RNG before COM");};
        force_feedback_4108c0(w,1);
        require(f.effect(5).parameters.back().type==std::vector<DWORD>{10000,0,498,150000}&&
            f.effect(5).set_flags.back()==0x20000100&&f.words()[1]==0,"impact magnitude/phase/period or trailing clear");f.finish();
    });
    check("absent-impact-effect-does-not-advance-RNG",[&]{
        FeedbackFixture f(session);f.create_result=E_FAIL;f.prepare();auto w=f.world();
        f.s.vehicles.contact[4].direction_magnitude=1;force_feedback_4108c0(w,1);
        require(f.s.random.state==1&&f.words()[1]==0&&f.s.vehicles.contact[4].direction_magnitude==0,"missing effect advanced RNG");f.finish();
    });
    check("surface-wheel-types-reference-load-speed-and-quiet-leaves",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();auto& p=f.s.vehicles.physics[4];
        for(unsigned i=0;i<4;++i){p.wheels[i].load=1;p.wheels[i].reference_load=i<2?2.0f:0.0f;
            f.s.vehicles.contact[4].wheels[i].type=i==0?2:(i==1?1:9);}
        p.speed=-10;force_feedback_410950(w,1);
        require(f.words()[3]==bits(31.25f)&&f.words()[4]==bits(100000)&&
            f.effect(6).parameters.back().type==std::vector<DWORD>{31,0,0,100000},"surface source data and operation order");
        f.words()[0]=123;f.words()[1]=456;f.words()[6]=bits(900);force_feedback_4109e0(w,1);
        require(f.words()[0]==123&&f.words()[1]==456&&f.words()[3]==0&&f.words()[6]==0,"quiet branch erased unrelated payload");f.finish();
    });
    check("conversion-low-DWORD-and-masked-invalid",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();
        for(const auto row:std::array<std::pair<float,std::uint32_t>,6>{{{-1.75f,0xffffffffu},{4294967296.0f,0u},
            {2147483648.0f,0x80000000u},{-2147483648.0f,0x80000000u},
            {std::numeric_limits<float>::infinity(),0u},{std::numeric_limits<float>::quiet_NaN(),0u}}}){
            f.words()[6]=bits(row.first);f.words()[7]=bits(100000);force_feedback_424210(w,1);
            require(f.effect(4).parameters.back().type[0]==row.second,"FISTP64 low DWORD conversion");
        }f.finish();
    });
    check("unordered-impact-preserves-bitcopy-consumes-source-and-does-not-randomize",[&]{
        FeedbackFixture f(session);f.prepare();auto w=f.world();constexpr std::uint32_t nan=0x7fc12345u;
        f.s.vehicles.contact[4].direction_magnitude=std::bit_cast<float>(nan);force_feedback_4108c0(w,1);
        require(f.words()[1]==nan&&f.s.vehicles.contact[4].direction_magnitude==0&&f.s.random.state==1,
            "unordered source comparison branch differs");f.finish();
    });
    check("effect-ownership-orphans-equal-pointers-and-failure-retained-output",[&]{
        FeedbackFixture f(session);auto w=f.world();force_feedback_423ff0(w,0);auto* old=&f.effect(4);
        force_feedback_423ff0(w,0);auto* newer=&f.effect(4);require(old!=newer&&old->refs==1,"overwritten reference prematurely released");
        f.repeat_receiver=newer;f.create_result=S_FALSE;force_feedback_423ff0(w,0);require(newer->refs==2,"equal returned references collapsed");
        f.create_result=E_FAIL;f.retain_failure_output=true;force_feedback_423ff0(w,0);require(&f.effect(4)==newer&&newer->refs==2,"failed call retained output mishandled");
        f.startup->shutdown_424930();require(newer->refs==1&&old->refs==1,"source shutdown should consume only current slot reference");
        f.startup->finalize_host();require(old->refs==0&&newer->refs==0,"host finalizer lost orphan or duplicate reference");
        f.startup.reset();f.os.clean();
    });
    check("same-owner-raw-and-native-effect-guards-before-external-call",[&]{
        FeedbackFixture f(session);f.prepare();SourceInputStorage foreign;auto& e=f.effect(4);
        fixture::Effect unowned(f.os.log,"unowned");const auto before=f.os.log.calls.size();
        host_error([&]{(void)f.startup->borrow_force_feedback_state(foreign.acquisition);});
        host_error([&]{(void)f.startup->start_force_feedback(foreign.acquisition,e,1,0);});
        host_error([&]{(void)f.startup->start_force_feedback(f.input.acquisition,unowned,1,0);});
        require(f.os.log.calls.size()==before,"foreign reference reached external COM");
        f.startup->shutdown_424930();host_error([&]{(void)f.startup->start_force_feedback(f.input.acquisition,e,1,0);});
        f.startup->finalize_host();host_error([&]{(void)f.startup->borrow_force_feedback_state(f.input.acquisition);});
        f.startup.reset();f.os.clean();
    });
    check("source-shutdown-release-before-pointer-clear-and-payload-retained",[&]{
        FeedbackFixture f(session);f.prepare();auto& bank=f.startup->borrow_force_feedback_state(f.input.acquisition);
        for(unsigned slot:{0u,4u,5u,6u}){
            auto* p=&f.effect(slot);p->on_release=[&,slot,p]{require(bank.records[0].effects[slot].effect==p,"source pointer cleared before Release");};
        }
        const auto payload=bank.records[0].effects[0].payload;f.startup->shutdown_424930();
        for(unsigned slot:{0u,4u,5u,6u})require(bank.records[0].effects[slot].effect==nullptr,"shutdown effect pointer retained");
        require(bank.records[0].effects[0].payload==payload,"shutdown modified descriptor payload");
        f.startup.reset();f.os.clean();
    });
    check("real-dispatcher-calls-feedback-once-then-next-controller",[&]{
        FeedbackFixture f(session);f.s.scene_mode=2;auto feedback=f.world();
        f.activate(1,4,0);f.activate(2,2,0);f.input.physical[1].feedback_mode=2;f.input.physical[2].feedback_mode=0;
        f.s.vehicles.physics[4].input.throttle=91;f.s.vehicles.physics[2].input.throttle=92;
        const auto begin=f.os.log.calls.size();
        f.with_world([&](SourceInputWorld& w){const auto result=dispatch_source_input_40dad0(w,&feedback);
            require(result.status==SourceInputStatus::completed,"real feedback did not close reached child");});
        require(f.created.size()==4&&f.s.current_slot==2&&f.s.vehicles.restart[4].previous_input.throttle==91&&
            f.s.vehicles.restart[2].previous_input.throttle==92,"dispatcher retried or skipped next controller");
        require(Calls(f.os.log.calls.begin()+static_cast<std::ptrdiff_t>(begin),f.os.log.calls.end())==
            Calls{"create0.0","create0.4","create0.5","create0.6","fx0.0.1.start","fx0.4.2.start",
                "fx0.5.3.start","fx0.6.4.start","fx0.4.2.set","fx0.6.4.set"},
            "dispatcher repeated or omitted a feedback call; once flag cannot hide duplicate SetParameters");
        f.finish();
    });
    check("unbound-dispatch-retains-required-result-and-paused-dispatch-never-enters-feedback",[&]{
        FeedbackFixture f(session);f.activate(1,4,0);f.input.physical[1].feedback_mode=2;
        f.with_world([&](SourceInputWorld& w){const auto r=dispatch_source_input_40dad0(w);
            require(r.status==SourceInputStatus::force_feedback_410a10_required&&r.physical_slot==1,"missing real boundary silently skipped");});
        f.s.frame_pause=1;auto feedback=f.world();
        f.with_world([&](SourceInputWorld& w){require(dispatch_source_input_40dad0(w,&feedback).status==SourceInputStatus::completed,"paused real menu child");});
        require(f.created.empty(),"paused branch reached force feedback");f.finish();
    });
    check("actual-Session-acquisition-dispatch-overload-and-foreign-input-rejected",[&]{
        FeedbackFixture f(session);Keys keys;f.s.scene_mode=2;
        f.s.frontend_launch.input_bindings[1]={4,2};
        const auto result=acquire_dispatch_session_input(session,keys,*f.startup);
        require(result.status==SourceInputStatus::completed&&keys.calls==1&&f.created.size()==4&&
            f.input.physical[1].feedback_mode==2,"actual acquisition wrapper omitted FF or used another registry");
        SourceInputStorage foreign;const auto before=keys.calls;
        host_error([&]{(void)acquire_dispatch_session_input(session,foreign,keys,*f.startup);});
        require(keys.calls==before,"foreign input reached acquisition");f.finish();
    });
    std::cout<<"force_feedback_receiving_cases="<<cases<<" physical_effects=0 original_execution_cases=0\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
