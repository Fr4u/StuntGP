#pragma once
// Test setup and address views derived from pinned NEW test code, not game children.
struct Fixture {
    RaceSession& session;RaceSessionState& s;SourceInputStorage& input;SessionPauseInvocation pause;
    explicit Fixture(RaceSession& owner):session(owner),s(owner.state()),input(s.source_input),pause(s,owner.audio_device()){
        // Reset test payloads in the SAME owner; never an application reset.
        SourceInputStorage initial;
        // Fixture cold state for this case, never an application reinitialization.
        auto& ff=input.force_feedback;
        ff.physical_words={};ff.selected_data_4d5b24=0;ff.initialized_576b70=0;
        ff.direction_576b78={};ff.axes_477640={0,4};ff.axes_count_477648=1;
        ff.gain_47764c=10000;ff.deadband_477650=100;ff.parameter_pointer_kind={};
        input.acquisition.upstream=initial.acquisition.upstream;
        input.acquisition.physical=initial.acquisition.physical;
        input.acquisition.source_device_pointer_576b6c=initial.acquisition.source_device_pointer_576b6c;
        input.physical=initial.physical;input.profiles=initial.profiles;
        input.vehicle_profiles=initial.vehicle_profiles;input.previous_activity=initial.previous_activity;
        // Explicit test seeds, never a proposed replacement for source startup.
        s.scene_mode=0;s.frame_pause=0;s.startup.field_60f2c0=0;s.current_slot=5;
        s.menu_input.previous_pause=0;s.menu_input.raw_words={};s.menu_input.edges={};
        s.startup.fields_62d480.fill(0);s.startup.fields_62dcf0.fill(0);
        s.normal_finish.confirm_pressed=0;s.normal_finish.confirm_consumed=0;s.pause.raw_activity=0;
        s.pause.physical_input.token=0;
        for(auto& b:s.frontend_launch.input_bindings)b={0xffffu,0};
        for(unsigned i=0;i<6;++i){s.vehicles.physics[i].input={};s.vehicles.restart[i].previous_input={};
            s.vehicles.physics[i].previous_shift=0;s.vehicles.physics[i].previous_stunt=0;
            s.vehicles.startup[i].field_2280=1;}
    }
    auto bank(unsigned i){return borrow_menu_input_bank(s.menu_input.edges[i],input.acquisition.physical[i].extra_edges);}
    std::span<std::uint8_t,13> profile(unsigned bank_index,unsigned index=0){
        return std::span<std::uint8_t,13>{reinterpret_cast<unsigned char*>(input.profiles[bank_index].data())+13*index,13};
    }
    void activate(unsigned physical,unsigned vehicle,unsigned kind){
        s.menu_input.raw_words[physical]={1,1};s.frontend_launch.input_bindings[physical]={vehicle,99};
        input.acquisition.physical[physical].word_0c=kind;
        input.acquisition.physical[physical].words_from_2c[2]=5000;
    }
    void button(unsigned physical,unsigned index,std::uint8_t value,bool previous=false){
        auto& raw=input.acquisition.physical[physical];const auto offset=(previous?0x88u:0x68u)+index;
        if(offset<0x168u)reinterpret_cast<unsigned char*>(raw.words_from_2c.data())[offset-0x2c]=value;
        else input.physical[physical].bytes_168_187[offset-0x168]=value;
    }
    SourcePhysicalInputView physical(unsigned i){
        auto& r=input.acquisition.physical[i];auto& x=input.physical[i];
        return {s.menu_input.raw_words[i].word_0,r.word_8,r.word_0c,x.activity_kind,x.profile_index,
            s.frontend_launch.input_bindings[i],x.feedback_mode,r.word_24,r.word_28,r,x.bytes_168_187,bank(i)};
    }
    SourceVehicleInputView vehicle(unsigned i){
        auto& p=s.vehicles.physics[i];auto& old=s.vehicles.restart[i].previous_input;
        return {s.vehicles.startup[i].field_2280,p.input,{old.throttle,old.brake,old.left,old.right,
            p.previous_shift,p.previous_stunt,old.rear_lock,old.steering_boost,old.turbo,old.auxiliary},
            input.vehicle_profiles[i],s.vehicles.race[i].control_kind,s.vehicles.parameters[i].steering_sensitivity};
    }
    template<class F> void with_world(F action){
        auto p=[&]<std::size_t... I>(std::index_sequence<I...>){return std::array<SourcePhysicalInputView,4>{physical(static_cast<unsigned>(I))...};}(std::make_index_sequence<4>{});
        auto v=[&]<std::size_t... I>(std::index_sequence<I...>){return std::array<SourceVehicleInputView,6>{vehicle(static_cast<unsigned>(I))...};}(std::make_index_sequence<6>{});
        auto m=menu_physical_producer_views(input.acquisition,s.menu_input);
        const auto keys=menu_keyboard_bytes(s.startup);
        const std::span<const std::uint8_t,256> old{reinterpret_cast<const unsigned char*>(s.startup.fields_62dcf0.data()),256};
        SourceInputWorld w{s.frame_pause,keys,old,p,v,input.profiles,s.current_slot,input.acquisition.source_device_pointer_576b6c,
            s.normal_finish.confirm_pressed,s.normal_finish.confirm_consumed,s.pause.raw_activity,input.previous_activity,
            {s.scene_mode,pause.lifecycle()},{s.scene_mode,keys,m,source_menu_selectors(input.profiles),input.acquisition.source_device_pointer_576b6c}};
        action(w);
    }
};

