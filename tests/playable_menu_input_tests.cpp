#include "sgn/playable_menu_input.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace sgn::retail;
namespace {
void require(bool okay,const char* message){if(!okay)throw std::runtime_error(message);}
struct Fixture {
    std::uint32_t scene{1};
    MenuKeyboardRawState raw;
    SessionMenuInputState menu;
    RaceInitializationState startup;
    std::uint8_t selector_a{253},selector_b{254};
    std::uint32_t selector_back{0xabcdef12};
    std::array<bool,256> keys{};
    PlayableMenuKeyboardWorld world(){return {scene,raw,menu,startup,{selector_a,selector_b,selector_back}};}
    void bind(){bind_playable_menu_keyboard(world());}
    void publish(){publish_playable_menu_keyboard(world(),keys);}
};
void pause_key_edges(){
    PlayablePauseKey latch;
    std::array<bool,256> keys{};
    require(!latch.consume(1,0,keys),"idle must not request pause");
    keys[27]=true;
    require(latch.consume(1,0,keys),"first Escape enters pause");
    require(!latch.consume(1,0,keys),"held Escape must not repeat entry");
    require(!latch.consume(1,1,keys),"paused Escape belongs to source menu");
    keys[27]=false;(void)latch.consume(1,1,keys);
    keys[27]=true;
    require(!latch.consume(1,1,keys),"paused rising Escape must not bypass menu debounce");
    require(!latch.consume(1,0,keys),"resume while Escape held must not immediately repause");
    keys[27]=false;(void)latch.consume(1,0,keys);keys[27]=true;
    require(latch.consume(1,0,keys),"fresh Escape after resume must work");
    keys[27]=false;(void)latch.consume(1,0,keys);keys[27]=true;
    require(!latch.consume(2,0,keys),"frontend must not enter race pause");
    keys[27]=false;(void)latch.consume(1,0,keys);keys[27]=true;
    require(!latch.consume(1,2,keys),"nonzero raw pause must not enter playable domain");
}
void focus_rearm(){
    PlayablePauseKey latch;std::array<bool,256> keys{};
    latch.release_host_input();keys[27]=true;
    require(!latch.consume(1,0,keys),"held Escape on focus return must be suppressed");
    keys[27]=false;require(!latch.consume(1,0,keys),"observed release only rearms");
    keys[27]=true;require(latch.consume(1,0,keys),"Escape must work after focus rearm");
}
void retained_owners_and_seven_source_edges(){
    Fixture f;
    f.raw.source_device_pointer_576b6c=0xdeadbeef;
    f.raw.upstream[0].words[0]=0x76543210;
    f.raw.physical[0].words_from_2c.fill(0x11223344);
    f.menu.raw_words[0].word_4=0x8899aabb;
    f.menu.previous_pause=0x1234;
    f.startup.fields_62dcf0.fill(0x76543210);
    f.menu.edges[2].up=0x91;
    f.raw.physical[2].extra_edges.current_1ad=0x92;
    auto& edge=f.menu.edges[0];auto& extra=f.raw.physical[0].extra_edges;
    edge={3,90,4,91,5,92,7,93};extra={1,2,6,94,95,96};
    f.bind();
    for(const auto key:{0x25,0x27,0x26,0x28,0x0d,0x1b})f.keys[key]=true;
    f.publish();
    const auto bank=borrow_menu_input_bank(edge,extra);
    for(unsigned i=0;i<7;++i){
        require(bank.current[i].get()==1,"all seven keyboard fields use actual source mapping");
        require(bank.previous[i].get()==i+1,"source seven-byte history must retain nonboolean old bytes");
    }
    f.publish();
    for(unsigned i=0;i<7;++i)require(bank.previous[i].get()==1,"held key must publish previous1");
    f.keys.fill(false);f.publish();
    for(unsigned i=0;i<7;++i){
        require(bank.current[i].get()==0,"keyup must clear current");
        require(bank.previous[i].get()==1,"keyup must retain prior down once");
    }
    f.publish();
    for(unsigned i=0;i<7;++i)require(bank.previous[i].get()==0,"second keyup must clear previous");
    require(f.raw.source_device_pointer_576b6c==0xdeadbeef,"keyboard must not replace source device identity");
    require(f.raw.upstream[0].words[0]==0x76543210,"host binding must not fake source startup");
    require(f.raw.physical[0].words_from_2c[3]==0x11223344,"raw payload must survive");
    require(f.menu.raw_words[0].word_4==0x8899aabb&&f.menu.previous_pause==0x1234,"pause status history must survive");
    require(f.startup.fields_62dcf0[0]==0x76543210,"host must not run source acquisition prefix");
    require(f.menu.edges[2].up==0x91&&f.raw.physical[2].extra_edges.current_1ad==0x92,"inactive physical banks must survive");
}
void key_mapping_and_scene(){
    Fixture f;f.bind();
    f.keys['W']=true;f.keys['S']=true;f.keys['A']=true;f.keys['D']=true;f.publish();
    require(f.menu.edges[0].up==0&&f.menu.edges[0].down==0,"driving WASD must not become invented menu arrows");
    f.keys.fill(false);f.keys[0x1b]=true;f.scene=2;f.publish();
    require(f.menu.edges[0].back==0&&f.raw.physical[0].extra_edges.current_1ad==1,"source scene gate controls Escape back only");
    f.scene=1;f.keys.fill(false);f.keys[13]=true;f.publish();
    require(f.menu.edges[0].activate==1&&f.menu.edges[0].activate_previous==0,"Enter rising source edge");
    const auto bytes=menu_keyboard_bytes(f.startup);
    for(unsigned i=0;i<256;++i)require(bytes[i]==(i==13?0x80:0),"host snapshot must use source high bit and clear released keys");
}
void reject_changed_device_before_write(){
    Fixture f;f.menu.raw_words[3].word_0=1;bool rejected=false;
    try{f.bind();}catch(const sgn::FormatError&){rejected=true;}
    require(rejected&&f.menu.raw_words[0].word_0==0&&f.raw.physical[0].word_0c==0,"multiple-device rejection precedes host binding");
    f.menu.raw_words[3].word_0=0;f.menu.raw_words[0].word_0=1;f.raw.physical[0].word_0c=2;
    rejected=false;try{f.bind();}catch(const sgn::FormatError&){rejected=true;}
    require(rejected&&f.raw.physical[0].word_0c==2,"active joystick must not be silently replaced");
    f.menu.raw_words[0].word_0=0;f.bind();f.raw.physical[0].word_8=1;
    f.startup.fields_62d480.fill(0x42424242);f.menu.edges[0].up=0xa4;
    rejected=false;try{f.publish();}catch(const sgn::FormatError&){rejected=true;}
    require(rejected&&f.startup.fields_62d480[0]==0x42424242&&f.menu.edges[0].up==0xa4,"changed binding must reject before publication");
}
}
int main(){
    try{
        pause_key_edges();focus_rearm();retained_owners_and_seven_source_edges();
        key_mapping_and_scene();reject_changed_device_before_write();
        std::cout<<"PASS: five CPU host-menu groups; no GPU, native window, audio device or retail EXE executed\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
