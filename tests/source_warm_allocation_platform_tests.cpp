#include "sgn/race_session.hpp"
#include "sgn/source_pmd_model.hpp"
#include "source_warm_allocation_witness.hpp"
#include <algorithm>
#include <atomic>
#include <iostream>
#include <thread>

using namespace sgn;
using namespace sgn::retail;
namespace {
unsigned checks{};
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);++checks;}
template<class F>void expired(F call){
    bool rejected=false;try{call();}catch(const SourceAllocationError&){rejected=true;}
    check(rejected,"expired source allocation remained accessible");
}
void verify(const std::filesystem::path& root){
    // Actual deferred platform: constructor does not create a native backend.
    RaceSessionPlatform platform(SourceColdConstructionOnly{},root,0.0f);
    auto& bank=platform.source_allocations();auto& world=platform.state();
    check(!bank.initialized()&&bank.live_count()==0,"source bank was implicitly initialized");
    bool boot_rejected=false;
    try{platform.release_scene_allocations_42eaa0();}
    catch(const SourceAllocationError& error){boot_rejected=error.source_site==0x42eab0;}
    check(boot_rejected&&!bank.initialized()&&bank.live_count()==0,"missing source boot must reject without initialization");
    bank.initialize_42eab0();
    const auto ordinary=bank.allocate_42e8d0(17,0),persistent=bank.allocate_42e8d0(29,1);
    std::fill(bank.bytes(persistent).begin(),bank.bytes(persistent).end(),std::uint8_t{0x5b});
    auto& fonts=world.ui.fonts;
    const auto font_id0=fonts.allocate_source_scratch(0,bank,nullptr);
    const auto font_id1=fonts.allocate_source_scratch(1,bank,nullptr);
    const auto font0=fonts.source_scratch_allocation(0,bank),font1=fonts.source_scratch_allocation(1,bank);
    check(bank.bytes(font0).size()==1024&&bank.bytes(font1).size()==1024,"actual font source producers missing");
    SourcePmdFileSystem files(root);SourcePmdLoadingState loading;
    auto model=load_source_pmd_42efb0(bank,loading,files,"car1");
    const auto view=model.view();const auto field=view.part(view.source_lod_index(0),0).flags();
    const auto model_ref=view.allocation();const auto source_flags=field.value();
    check(bank.live_count()==5&&loading.file==model_ref,"PMD/font producers did not use the same actual bank");
    const auto before=bank.source_words();
    // Receiving scene request only. The allocation child must not consume it.
    world.scene_mode=1;world.options.selection_phase=2;world.normal_finish.menu_requested=1;
    world.pause.physical_input.token=3;world.frame_pause=1;
    std::atomic<bool> thread_rejected{false};
    std::thread foreign([&]{try{platform.release_scene_allocations_42eaa0();}catch(const std::exception&){thread_rejected=true;}});
    foreign.join();
    check(thread_rejected&&bank.live_count()==5&&field.value()==source_flags,"foreign thread changed actual allocation owners");
    RaceSessionPlatform other(SourceColdConstructionOnly{},root,0.0f);
    auto& other_bank=other.source_allocations();other_bank.initialize_42eab0();
    const auto unrelated=other_bank.allocate_42e8d0(13,0);other_bank.bytes(unrelated)[0]=0x91;
    platform.release_scene_allocations_42eaa0(); // The production adapter under test.
    const auto after=bank.source_words();
    check(&bank==&platform.source_allocations()&&bank.live_count()==1,"warm release replaced the bank or retained ordinary allocations");
    check(after.active==0&&after.ordinary_bytes==0&&after.persistent_bytes==65,"source scope/category accounting");
    check(after.next_ordinal==before.next_ordinal&&after.peak_bytes==before.peak_bytes&&
        after.release_base==before.release_base&&after.release_persistent==0,"release injected begin/reset/force-persistent semantics");
    expired([&]{(void)bank.bytes(ordinary);});expired([&]{(void)bank.bytes(model_ref);});
    expired([&]{(void)model.view();});expired([&]{(void)view.part_count();});expired([&]{(void)field.value();});
    expired([&]{(void)bank.bytes(font0);});expired([&]{(void)bank.bytes(font1);});
    check(fonts.scratch_allocations()==2&&fonts.source_word(0,4)==font_id0&&fonts.source_word(1,4)==font_id1,
        "source font descriptors were eagerly cleared instead of leaving expired identities");
    check(loading.file==model_ref,"warm release rewrote the retained source PMD descriptor");
    check(std::all_of(bank.bytes(persistent).begin(),bank.bytes(persistent).end(),[](auto b){return b==0x5b;}),"persistent category payload changed");
    check(other_bank.live_count()==1&&other_bank.bytes(unrelated)[0]==0x91,"another platform bank was released");
    check(world.scene_mode==1&&world.options.selection_phase==2&&world.normal_finish.menu_requested==1&&
        world.frame_pause==1&&world.pause.physical_input.token==3,"allocation child crossed scene/pause/input boundary");
    platform.release_scene_allocations_42eaa0();
    check(bank.live_count()==1&&bank.source_words().active==0&&bank.source_words().next_ordinal==before.next_ordinal,
        "inactive source release force-freed persistent rows or reset ordinals");
    bank.begin_42ea80();const auto replacement=bank.allocate_42e8d0(17,0);
    check(replacement!=ordinary,"new allocation reused an expired host generation");
    expired([&]{(void)bank.bytes(ordinary);});expired([&]{(void)field.value();});
    platform.release_scene_allocations_42eaa0();expired([&]{(void)bank.bytes(replacement);});
    const auto host=platform.host_readiness(),other_host=other.host_readiness();
    check(!host.assets&&!host.audio_device&&!host.audio_stream&&!host.window_input&&
        !other_host.assets&&!other_host.audio_device&&!other_host.audio_stream&&!other_host.window_input,
        "CPU receiving constructed a native platform backend");
    RaceSessionPlatform receiving(SourceColdConstructionOnly{},root,0.0f);
    const warm_release_receiving::AllocationWitness witness(receiving);
    receiving.release_scene_allocations_42eaa0();witness.released();
}
}
int main(int argc,char** argv)try{
    check(argc==2,"expected NEW retail data root");verify(argv[1]);
    std::cout<<"PASS "<<checks<<" actual platform/bank/PMD/font allocation checks; no native backend created\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
