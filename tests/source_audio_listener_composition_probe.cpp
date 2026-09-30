// Reuse the sealed SDK fake and native A2-A7 rig without editing its bytes.
#define main sealed_a7_regression_main
#include "sealed_a7_fixture.cpp"
#undef main

int main(int argc,char** argv){try{
    require(argc==2,"expected observe, success, failure or exception");
    const std::string_view scenario=argv[1];
    require(scenario=="observe"||scenario=="success"||scenario=="failure"||scenario=="exception","unknown scenario");
    Stats stats;
    bool matched=false;
    {
        Rig rig(stats);rig.prepare();
        if(scenario=="failure")stats.query_hr=E_FAIL;
        if(scenario=="exception")stats.throw_after=true;
        std::uint64_t observed=0;
        stats.during_query=[&]{
            // The sealed fake already wrote *out before this callback. Source
            // PUSH62F11C at461A8C points directly to this retained word.
            observed=rig.listener.value;
            if(scenario!="observe")rig.listener.value=0;
        };
        std::optional<SourceAudioListenerAcquisitionOutcome> acquired;
        bool threw=false;
        try{acquired.emplace(rig.acquire());}catch(const Marker&){threw=true;}
        require(threw==(scenario=="exception"),"unexpected QueryInterface exception policy");
        require(stats.query_calls==1&&stats.adds==1,"unexpected extra QI/reference");
        require(rig.owner->source_listener_identity()==stats.listener_identity,"native output reference not retained");
        if(scenario=="observe"){
            matched=observed==stats.listener_identity;
            std::cout<<"during QI after native output: source slot saw acquired interface="<<matched<<" expected=1\n";
        }else{
            matched=rig.listener.value==0;
            std::cout<<"after QI callback wrote zero: source slot is zero="<<matched<<" expected=1\n";
            if(scenario=="success"){
                require(acquired&&std::holds_alternative<SourceAudioListenerConfigurationBoundary>(*acquired),"successful QI did not reach A7");
                std::uint32_t guard=0;
                try{(void)continue_source_audio_listener_461aa0(*acquired,*rig.owner);}
                catch(const SourceAudioListenerConfigurationError& error){guard=error.source_site;}
                const bool guarded=guard==0x461aadu&&stats.setter_calls.empty();
                std::cout<<"A7 first late guard reached="<<guarded<<" expected=1;native setters="<<stats.setter_calls.size()<<" expected=0\n";
                matched=matched&&guarded;
            }else if(scenario=="failure"){
                require(acquired&&std::holds_alternative<SourceAudioListenerAcquisitionFailure>(*acquired),"failed QI was admitted");
                const auto result=continue_source_audio_listener_461aa0(*acquired,*rig.owner);
                require(std::holds_alternative<SourceAudioListenerAcquisitionFailure>(result)&&stats.setter_calls.empty(),"failure invoked setters");
            }
        }
        require(!rig.owner->host_backend_ready(),"probe prepared Modern");
    }
    released(stats,1); // Even on the demonstrated divergence: exact cleanup.
    std::cout<<"native listener/primary/device cleanup and refs PASS;system audio calls0\n";
    if(matched){std::cout<<"SOURCE_OUTPUT_ALIAS_MATCH\n";return 0;}
    std::cout<<"EXPECTED_RED_A6_OUTPUT_ALIAS_COMPOSITION\n";return 1;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 2;}catch(...){std::cerr<<"unexpected exception\n";return 2;}}
