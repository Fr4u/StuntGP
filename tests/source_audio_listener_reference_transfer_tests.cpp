#define main included_a7_regression_main
#include "source_audio_listener_native_configuration_tests.cpp"
#undef main
#include <type_traits>

namespace {
using OwnedListener=Microsoft::WRL::ComPtr<IDirectSound3DListener>;
using Transfer=sgn::SourceAudioListenerReferenceTransfer;
using Script=std::function<HRESULT(IDirectSoundBuffer*,REFIID,void**,Transfer&)>;
thread_local Script* current_script=nullptr;
unsigned assertions{};
void expect(bool value,const char* text){++assertions;require(value,text);}
struct ScriptScope {
    Script* previous;
    explicit ScriptScope(Script& script):previous(current_script){current_script=&script;}
    ~ScriptScope(){current_script=previous;}
};
HRESULT scripted_query(IDirectSoundBuffer* primary,REFIID iid,void** output,Transfer& sink){
    expect(current_script!=nullptr,"missing explicit receiving script");
    return (*current_script)(primary,iid,output,sink);
}
constexpr SourceAudioDefaultDeviceNativeApi ordinary{&create};
constexpr SourceAudioDefaultDeviceNativeApi scripted{&create,&scripted_query};
OwnedListener acquired(Stats& stats){
    auto* value=new Listener(stats);value->AddRef();OwnedListener owned;owned.Attach(value);return owned;
}
static_assert(!std::is_copy_constructible_v<Transfer>&&!std::is_move_constructible_v<Transfer>);

void ordinary_returned_reference(){
    for(const bool equal_initial:{false,true}){
        Stats stats;
        {Rig rig(stats,ordinary);rig.prepare();
            if(equal_initial){stats.prepared_listener=new Listener(stats);rig.listener.value=stats.listener_identity;stats.output=Output::unchanged;}
            auto** address=rig.listener.value.native_address();
            stats.during_query=[&]{
                expect(listener_receiving_fixture::active_transfer==nullptr,"ordinary QI acquired a receiving sink");
                expect(rig.listener.value==stats.listener_identity&&*address==reinterpret_cast<void*>(stats.listener_identity),"ordinary QI callback observed a shadow cell");
                expect(rig.owner->source_listener_identity()==0,"ordinary ownership adopted before successful return");
            };
            const auto outcome=rig.acquire();
            expect(rig.listener.value.native_address()==address&&rig.owner->source_listener_identity()==stats.listener_identity,"ordinary returned ref or permanent cell changed");
            expect(stats.adds==1&&stats.listener_releases==0,"ordinary returned ref was duplicated or released early");
            expect(std::holds_alternative<SourceAudioSamplesBoundary>(continue_source_audio_listener_461aa0(outcome,*rig.owner)),"ordinary QI did not reach A7");
            checked_calls(stats,3);
        }released(stats,1);
    }
}
void ordinary_failure_null_and_exception(){
    for(const auto hr:{E_NOINTERFACE,E_FAIL,S_FALSE,S_OK}){
        Stats stats;stats.query_hr=hr;stats.output=Output::zero;
        {Rig rig(stats,ordinary);rig.prepare();const auto outcome=rig.acquire();
            expect(rig.listener.value==0&&rig.owner->source_listener_identity()==0,"ordinary null output created an owned ref");
            if(hr==S_OK)configuration_error([&]{(void)continue_source_audio_listener_461aa0(outcome,*rig.owner);},0x461aad);
            else{const auto& failure=std::get<SourceAudioListenerAcquisitionFailure>(outcome);expect(failure.query_result==hr,"source exact-zero branch became HRESULT sign test");}
            expect(stats.setter_calls.empty(),"ordinary null output reached a setter");
        }released(stats,0);
    }
    for(const bool before:{true,false}){
        Stats stats;stats.output=Output::zero;stats.throw_before=before;stats.throw_after=!before;
        {Rig rig(stats,ordinary);rig.prepare();const std::uint64_t initial=rig.listener.value;bool caught=false;
            try{(void)rig.acquire();}catch(const Marker&){caught=true;}
            expect(caught&&rig.listener.value==(before?initial:0)&&rig.owner->source_listener_identity()==0,"ordinary exception lost its source prefix");
            reject_at([&]{(void)rig.acquire();},0x462174);
        }released(stats,0);
    }
}
void ordinary_hidden_provider_reference(){
    Stats stats;
    {Rig rig(stats,ordinary);rig.prepare();auto hidden=acquired(stats);stats.prepared_listener=static_cast<Listener*>(hidden.Get());
        const auto outcome=rig.acquire();(void)outcome;
        expect(hidden.Get()!=nullptr&&stats.prepared_listener->references==2&&stats.adds==2&&stats.listener_releases==0,"ordinary QI guessed or consumed the provider's independent ref");
        hidden.Reset();expect(stats.prepared_listener->references==1&&stats.listener_releases==1,"provider cannot release its own independent ref");
    }released(stats,2);
}
void rejected_transfers_keep_provider_ownership(){
    for(const bool same_pointer:{false,true}){
        Stats stats;
        {Rig rig(stats,scripted);rig.prepare();
            Script script=[&](IDirectSoundBuffer* primary,REFIID,void** out,Transfer& sink){
                expect(primary==reinterpret_cast<IDirectSoundBuffer*>(stats.primary_identity)&&out==rig.listener.value.native_address(),"receiving query lost native primary or source alias");
                OwnedListener empty;reject_at([&]{sink.transfer_acquired(empty);},0x461a97);
                expect(!empty&&rig.owner->source_listener_identity()==0,"null rejection consumed ownership state");
                auto first=acquired(stats);auto* const first_pointer=first.Get();*out=first_pointer;
                sink.transfer_acquired(first);
                expect(!first&&rig.owner->source_listener_identity()==reinterpret_cast<std::uintptr_t>(first_pointer)&&stats.adds==1&&stats.listener_releases==0,"accepted transfer did not consume exactly one ref");
                OwnedListener second;
                if(same_pointer){first_pointer->AddRef();second.Attach(first_pointer);}else second=acquired(stats);
                auto* const second_pointer=second.Get();
                reject_at([&]{sink.transfer_acquired(second);},0x461a97);
                expect(second.Get()==second_pointer&&stats.adds==2&&stats.listener_releases==0&&rig.owner->source_listener_identity()==reinterpret_cast<std::uintptr_t>(first_pointer),"repeat rejection consumed or replaced a ref");
                second.Reset();expect(stats.listener_releases==1&&stats.listener_live==1,"provider cleanup lost or doubled a rejected ref");
                return S_OK;
            };ScriptScope scope(script);
            const auto outcome=rig.acquire();expect(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(outcome),"valid transfer after null rejection failed");
            expect(stats.listener_releases==1,"successful return adopted a second ref");
        }released(stats,2);
    }
}
void foreign_transfer_rejected_before_state_access(){
    Stats stats;
    {Rig rig(stats,scripted);rig.prepare();
        Script script=[&](IDirectSoundBuffer*,REFIID,void** out,Transfer& sink){
            auto owned=acquired(stats);auto* const pointer=owned.Get();std::atomic<unsigned> rejected{};
            std::thread worker([&]{try{sink.transfer_acquired(owned);}catch(const SourceAudioDefaultDeviceError& error){if(error.source_site==0x462146)++rejected;}});worker.join();
            expect(rejected==1&&owned.Get()==pointer&&stats.listener_releases==0&&rig.owner->source_listener_identity()==0,"foreign rejection stole the incoming provider ref");
            *out=pointer;sink.transfer_acquired(owned);
            pointer->AddRef();OwnedListener again;again.Attach(pointer);
            std::thread repeated([&]{try{sink.transfer_acquired(again);}catch(const SourceAudioDefaultDeviceError& error){if(error.source_site==0x462146)++rejected;}});repeated.join();
            expect(rejected==2&&again.Get()==pointer&&stats.listener_releases==0,"foreign repeat touched owner state before thread admission");
            again.Reset();return S_OK;
        };ScriptScope scope(script);(void)rig.acquire();
    }released(stats,2);
}
void explicit_success_consistency(){
    for(const unsigned kind:{0u,1u,2u}){
        Stats stats;
        {Rig rig(stats,scripted);rig.prepare();OwnedListener provider;
            Script script=[&](IDirectSoundBuffer*,REFIID,void** out,Transfer& sink){
                expect(out==rig.listener.value.native_address(),"mismatch query uses shadow output");
                if(kind==0){*out=reinterpret_cast<void*>(std::uintptr_t{0x1234abcd});return S_OK;}
                if(kind==2){auto first=acquired(stats);sink.transfer_acquired(first);}
                provider=acquired(stats);*out=provider.Get();return S_OK;
            };ScriptScope scope(script);
            reject_at([&]{(void)rig.acquire();},0x461a97);
            expect(rig.listener.value==(kind==0?std::uint64_t{0x1234abcd}:reinterpret_cast<std::uintptr_t>(provider.Get())),"protocol rejection rewrote source output");
            expect(stats.listener_releases==0&&(kind==0||provider.Get()!=nullptr),"protocol rejection guessed ownership from pointer bits");
            expect((rig.owner->source_listener_identity()!=0)==(kind==2),"protocol rejection lost an accepted provisional ref");
            const SourceAudioListenerAcquisitionOutcome forged=SourceAudioListenerConfigurationBoundary{rig.listener};
            reject_at([&]{(void)continue_source_audio_listener_461aa0(forged,*rig.owner);},0x461aa0);
            provider.Reset();
        }released(stats,kind==0?0:(kind==1?1:2));
    }
}
void receiving_erased_output_and_reentry(){
    for(const auto hr:{S_OK,E_FAIL})for(const bool throwing:{false,true}){
        Stats stats;stats.query_hr=hr;stats.throw_after=throwing;
        {Rig rig(stats);rig.prepare();auto** const address=rig.listener.value.native_address();unsigned refused=0;
            stats.during_query=[&]{
                expect(*address==reinterpret_cast<void*>(stats.listener_identity)&&rig.listener.value==stats.listener_identity,"callback could not observe native publication");
                expect(rig.owner->source_listener_identity()==stats.listener_identity,"explicit transfer was delayed until after callback");
                rig.listener.value=0;expect(*address==nullptr,"callback source write did not reach native output");
                try{(void)rig.acquire();}catch(const SourceAudioDefaultDeviceError&){++refused;}
                try{rig.owner->prepare_host_backend();}catch(const SourceAudioDefaultDeviceError&){++refused;}
                try{(void)rig.owner->release_device(rig.device);}catch(const SourceAudioDefaultDeviceError&){++refused;}
            };
            bool caught=false;
            try{const auto outcome=rig.acquire();
                if(hr==S_OK)configuration_error([&]{(void)continue_source_audio_listener_461aa0(outcome,*rig.owner);},0x461aad);
                else expect(std::get<SourceAudioListenerAcquisitionFailure>(outcome).query_result==hr,"receiving failure changed into success");
            }catch(const Marker&){caught=true;}
            expect(caught==throwing&&refused==3&&rig.listener.value==0&&stats.setter_calls.empty(),"exception/failure/success lost erased source prefix or reentry guard");
            expect(stats.adds==1&&stats.listener_releases==0&&rig.owner->source_listener_identity()==stats.listener_identity&&!rig.owner->host_backend_ready(),"erased output lost or duplicated explicit ref or created modern backend");
            expect(listener_receiving_fixture::active_transfer==nullptr,"receiving transport escaped call scope");
        }released(stats,1);
    }
}
void receiving_equal_pointer_and_no_event(){
    for(const bool owns:{false,true}){
        Stats stats;
        {Rig rig(stats);rig.prepare();
            if(owns){stats.prepared_listener=new Listener(stats);rig.listener.value=stats.listener_identity;stats.output=Output::unchanged;}
            else stats.output=Output::zero;
            const auto outcome=rig.acquire();
            if(owns)expect(rig.owner->source_listener_identity()==stats.listener_identity&&stats.adds==1,"equal initial bits suppressed explicit acquisition");
            else{expect(rig.owner->source_listener_identity()==0&&stats.adds==0,"zero events minted ownership");configuration_error([&]{(void)continue_source_audio_listener_461aa0(outcome,*rig.owner);},0x461aad);}
        }released(stats,owns?1:0);
    }
}
void receiving_fixture_nested_owner_isolation(){
    for(const bool inner_receiving:{false,true})for(const bool inner_throws:{false,true}){
        Stats outer_stats,inner_stats;inner_stats.throw_before=inner_throws;bool nested_completed=false;
        {Rig outer(outer_stats);outer.prepare();
            outer_stats.during_query=[&]{
                auto* const context=listener_receiving_fixture::active_query;
                auto* const sink=listener_receiving_fixture::active_transfer;
                expect(context&&sink,"outer receiving query did not establish a context");
                {Rig inner(inner_stats,inner_receiving?api:ordinary);inner.prepare();bool caught=false;
                    try{nested_completed=std::holds_alternative<SourceAudioListenerConfigurationBoundary>(inner.acquire());}
                    catch(const Marker&){caught=true;}
                    expect(caught==inner_throws&&nested_completed!=inner_throws,"nested ordinary/receiving query stole the outer sink");
                    expect(inner.owner->source_listener_identity()==(inner_throws?0:inner_stats.listener_identity),"nested query reference belongs to the wrong owner");
                }released(inner_stats,inner_throws?0:1);
                expect(listener_receiving_fixture::active_query==context&&listener_receiving_fixture::active_transfer==sink,"nested return/exception did not restore outer context");
                expect(listener_receiving_fixture::begin_query(reinterpret_cast<IDirectSoundBuffer*>(outer_stats.primary_identity))==nullptr,"nested call rearmed an already claimed query context");
                expect(outer.owner->source_listener_identity()==outer_stats.listener_identity&&outer.listener.value==outer_stats.listener_identity,"nested query replaced the outer reference/source cell");
            };
            expect(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(outer.acquire()),"nested query prevented outer continuation");
            expect(listener_receiving_fixture::active_query==nullptr&&listener_receiving_fixture::active_transfer==nullptr,"completed outer query leaked fixture context");
        }released(outer_stats,1);
    }
}
void receiving_fixture_recursive_same_primary_is_ordinary(){
    Stats stats;bool nested=false;
    {Rig rig(stats);rig.prepare();
        stats.during_query=[&]{
            if(nested)return;nested=true;
            auto* const pointer=reinterpret_cast<IDirectSound3DListener*>(stats.listener_identity);
            stats.prepared_listener=static_cast<Listener*>(pointer);stats.output=Output::unchanged;
            void* output=pointer;
            constexpr GUID iid{0x279afa84u,0x4981u,0x11ceu,{0xa5u,0x21u,0x00u,0x20u,0xafu,0x0bu,0xe5u,0x60u}};
            const auto hr=reinterpret_cast<IDirectSoundBuffer*>(stats.primary_identity)->QueryInterface(iid,&output);
            OwnedListener internal;internal.Attach(static_cast<IDirectSound3DListener*>(output));
            expect(hr==S_OK&&internal.Get()==pointer&&stats.adds==2&&stats.listener_releases==0,"recursive ordinary QI consumed the receiving query's sink");
            expect(rig.owner->source_listener_identity()==stats.listener_identity&&rig.listener.value==stats.listener_identity,"recursive QI changed outer ownership/source storage");
            internal.Reset();expect(stats.listener_releases==1,"recursive ordinary caller could not release its own reference");
        };
        expect(std::holds_alternative<SourceAudioListenerConfigurationBoundary>(rig.acquire())&&nested&&stats.query_calls==2,"recursive QI prevented outer source continuation");
    }released(stats,2);
}
}
int main(){try{
    ordinary_returned_reference();ordinary_failure_null_and_exception();ordinary_hidden_provider_reference();
    rejected_transfers_keep_provider_ownership();foreign_transfer_rejected_before_state_access();
    explicit_success_consistency();receiving_erased_output_and_reentry();receiving_equal_pointer_and_no_event();
    receiving_fixture_nested_owner_isolation();receiving_fixture_recursive_same_primary_is_ordinary();
    std::cout<<"native listener output alias/reference transfer: 10 groups, "<<assertions<<" assertions PASS\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}catch(...){std::cerr<<"unexpected exception\n";return 1;}}
