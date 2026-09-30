#include "sgn/retail_level.hpp"
#include <iostream>
#include <stdexcept>
using namespace sgn::retail;
namespace {
void check(bool good,const char* message){if(!good)throw std::runtime_error(message);}
template<class F> void rejects(F f){bool yes=false;try{f();}catch(const std::domain_error&){yes=true;}check(yes,"missing contract rejection");}
LevelBlock block(std::string_view text){return {text,text.find("-END")};}
}
int main(){try{
    LevelSettings state{};state.game_type=17;state.music_track=23;state.lap_count=8;state.lap_record_ticks=159;
    state.track_texture.fill('X');state.track_texture.back()=0;state.texture_suffix=91;
    apply_level_settings(block("-END\nMUS_TRACK = 0 \n"),state);
    check(state.music_track==23&&state.game_type==17&&state.lap_count==8&&state.lap_record_ticks==159,"absent preserved state overwritten");
    check(state.texture_name().empty()&&state.track_texture[1]=='X'&&state.texture_suffix==91,"missing texture must clear only first byte");
    check(state.ground_level==-2000&&state.rolling_start_offset==60000&&state.camera_mode==1&&state.time_ticks==0&&state.demo_loop_ticks==3600,"default scalar fields differ");
    apply_level_settings(block("TRACKTEXSET = X \nGAMETYPE = timetrial_type_1 \nLAPCOUNT = -1 \nROLSTARTOFFSET = 4294967295 \nR_DEMOLOOP = 0 \n-END\n"),state);
    check(state.texture_suffix==91&&state.game_type==4&&state.lap_count==24&&state.rolling_start_offset==0xffffff9cu&&state.demo_loop_ticks==0xffffffffu,"unsigned conversion or missing short texture semantics changed");
    apply_level_settings(block("GAMETYPE = TIMETRIAL_TYPE_1 \nLAPCOUNT = 4 \n-END\n"),state);
    check(state.time_ticks==240,"original TIME substring collision must remain observable");
    const auto car=parse_level_car_settings(block("-END\n"));check(car[5].config==1&&car[5].ai_skill==100&&car[5].ai_mode==100,"car defaults differ");
    const std::string script="/* FAKE = */\nTRACK1 = \nL_SKY = Sky1 \nL_TRACK = track1 \n-END\n-TRACK2 = \nL_TRACK = track2 \n-END\n";
    const auto names=discover_level_blocks(script);check(names.size()==2&&names[0]=="TRACK1"&&names[1]=="TRACK2","block catalog changed");
    const auto selected=find_level_block(script,"TRACK2");check(selected&&parse_level_asset_bindings(*selected).track_name=="track2","block selection crossed boundary");
    check(!find_level_block(script,"ABSENT"),"unknown block found");
    rejects([&]{find_level_block("TRACK1 = \n","TRACK1");});
    rejects([&]{apply_level_settings(block("TRACKTEXSET ="),state);});
    rejects([&]{parse_level_asset_bindings(block("L_CAR1 = Car16 \n-END\n"),true);});
    rejects([&]{apply_level_settings(block("TRACKTEXSET = %s \n-END\n"),state);});
    rejects([&]{apply_level_settings({std::string_view("X\0Y",3),2},state);});
    std::cout<<"level defaults, preserved words, unsigned arithmetic, key collisions and bounds passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
