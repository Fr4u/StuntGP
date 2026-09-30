#pragma once
#include "borrowed_fixture.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <cstring>

namespace pause_menu_test {
namespace input {
inline void require(bool condition,const char* message){if(!condition)throw sgn::FormatError(message);}
inline std::uint32_t number(std::ifstream& stream){
    std::uint32_t value{};require(bool(stream.read(reinterpret_cast<char*>(&value),4)),"truncated pause input stream");return value;
}
struct Patch {std::uint32_t address,width,value;};
inline Patch patch(std::ifstream& stream){
    Patch value{number(stream),number(stream),number(stream)};
    require(value.width==1||value.width==4,"pause patch byte/word width");
    require(value.width==4||value.value<=255,"pause byte patch range");return value;
}
enum class Boundary:std::uint32_t {draw,audio,upload};
struct Effect {Boundary boundary;std::uint32_t event;std::vector<Patch> patches;};
struct Step {std::uint32_t operation;std::array<std::uint32_t,2> args;std::vector<Patch> patches;std::vector<Effect> effects;};
struct Group {std::uint32_t ordinal,width;bool separate_current,native_guard_only;std::vector<Step> steps;};
inline std::vector<Patch> patches(std::ifstream& stream){
    const auto count=number(stream);require(count<=1024,"pause input patch count");std::vector<Patch> result;
    for(unsigned i=0;i<count;++i)result.push_back(patch(stream));return result;
}
inline Step step(std::ifstream& stream){
    Step value{number(stream),{number(stream),number(stream)},{},{}};
    constexpr std::array<unsigned,12> entries{0x4473f0,0x447100,0x446c20,0x446b60,0x446af0,0x446b20,
        0x446a90,0x446ac0,0x446a70,0x446bb0,0x446b40,0x445350};
    require(std::find(entries.begin(),entries.end(),value.operation)!=entries.end(),"pause input full source entry");
    value.patches=patches(stream);const auto count=number(stream);require(count<=32,"pause effect count");
    for(unsigned i=0;i<count;++i){const auto boundary=number(stream),event=number(stream);
        require(boundary<=2&&event<4096,"pause effect boundary/index");
        value.effects.push_back({static_cast<Boundary>(boundary),event,patches(stream)});}
    return value;
}
inline std::vector<Group> read(const char* filename){
    std::ifstream stream(filename,std::ios::binary);char magic[8]{};
    require(bool(stream.read(magic,8))&&std::memcmp(magic,"SGNPMNU1",8)==0,"pause input signature");
    const auto count=number(stream);require(count>0&&count<=4096,"pause input group count");std::vector<Group> groups;
    for(unsigned i=0;i<count;++i){const auto ordinal=number(stream),width=number(stream),publication=number(stream),domain=number(stream),steps=number(stream);
        require(ordinal==i&&width>0&&width<=65535&&publication<=1&&domain<=1&&steps>0&&steps<=64,"pause input group fields");
        Group group{i,width,publication!=0,domain!=0,{}};
        for(unsigned n=0;n<steps;++n)group.steps.push_back(step(stream));groups.push_back(std::move(group));}
    require(stream.peek()==std::char_traits<char>::eof(),"trailing pause input bytes");return groups;
}
}

// Registry of actual mutable INPUT owners. No cast removes constness from a
// gameplay view. Signed row coordinates use memcpy of their object bytes so
// every uint32 fixture word retains its exact source bit representation.
class InputLocations {
    struct Location {void* data;std::uint32_t width;r::AudioMusicBufferValue* music{};};
    std::map<std::uint32_t,Location> locations_;
    template<class T>void bind(std::uint32_t address,T& value){
        static_assert(sizeof(T)==1||sizeof(T)==4);
        if(!locations_.emplace(address,Location{&value,sizeof(T)}).second)
            throw sgn::FormatError("duplicate pause input source owner");
    }
public:
    InputLocations(MenuOwners& menu){
        const std::array<std::uint32_t*,5> globals{&menu.physical.token,&menu.release,&menu.selection,&menu.highlight,&menu.ready};
        for(unsigned i=0;i<globals.size();++i)bind(0x61b744+i*4,*globals[i]);
        bind(0x4835e0,menu.language);
        for(unsigned i=0;i<8;++i){auto& row=menu.rows[i];const auto address=0x485120+i*24;
            bind(address,row.enabled);bind(address+4,row.text_id);bind(address+8,row.x);bind(address+12,row.y);
            bind(address+16,row.callback);bind(address+20,row.extra_text_id);}
        for(unsigned i=0;i<4;++i){auto& e=menu.edges[i];const auto address=0x62d580+i*0x1dc;
            bind(address+0x1aa,e.up);bind(address+0x1b1,e.up_previous);
            bind(address+0x1ab,e.down);bind(address+0x1b2,e.down_previous);
            bind(address+0x1ac,e.activate);bind(address+0x1b3,e.activate_previous);
            bind(address+0x1ae,e.back);bind(address+0x1b5,e.back_previous);}
    }
    void word(std::uint32_t source_address,std::uint32_t& actual_owner){bind(source_address,actual_owner);}
    void representation_word(std::uint32_t source_address,void* actual_mutable_owner){
        require_owner(actual_mutable_owner);
        if(!locations_.emplace(source_address,Location{actual_mutable_owner,4}).second)
            throw sgn::FormatError("duplicate pause input source owner");
    }
    void music_word(std::uint32_t address,r::AudioMusicBufferValue& value){
        if(!locations_.emplace(address,Location{nullptr,4,&value}).second)
            throw sgn::FormatError("duplicate pause input source owner");
    }
    static void require_owner(void* owner){if(!owner)throw sgn::FormatError("null pause input owner");}
    void apply(input::Patch patch){
        const auto found=locations_.find(patch.address);
        if(found==locations_.end()||found->second.width!=patch.width)
            throw sgn::FormatError("pause input patch does not name a registered live owner");
        if(patch.address==0x61b744&&patch.value>4)
            throw sgn::FormatError("pause input physical identity codec");
        if(patch.width==1&&patch.value>255)throw sgn::FormatError("pause input byte value");
        if(found->second.music)found->second.music->source_low_word(patch.value);
        else std::memcpy(found->second.data,&patch.value,patch.width);
    }
    void apply(std::span<const input::Patch> patches){for(auto patch:patches)apply(patch);}
    std::vector<std::uint32_t> capture()const{
        std::vector<std::uint32_t> words;words.reserve(locations_.size()*3);
        for(const auto& [address,location]:locations_){std::uint32_t value{};if(location.music)value=static_cast<std::uint32_t>(*location.music);else std::memcpy(&value,location.data,location.width);
            words.insert(words.end(),{address,location.width,value});}
        return words;
    }
};

class InputEffects {
    InputLocations& locations_;
    const input::Step& step_;
    std::vector<bool> applied_;
    std::array<std::uint32_t,3> calls_{};
public:
    InputEffects(InputLocations& locations,const input::Step& step):locations_(locations),step_(step),applied_(step.effects.size()){}
    void boundary(input::Boundary boundary){
        const auto kind=static_cast<unsigned>(boundary);input::require(kind<3,"pause boundary family");const auto event=calls_[kind]++;
        for(unsigned i=0;i<step_.effects.size();++i){const auto& effect=step_.effects[i];
            if(effect.boundary==boundary&&effect.event==event){locations_.apply(effect.patches);applied_[i]=true;}}
    }
    void require_all_applied()const{
        input::require(std::all_of(applied_.begin(),applied_.end(),[](bool value){return value;}),"pause required external effect was not reached");
    }
};

// Every selected entry calls the actual candidate; no callback-result service.
inline void invoke(r::PauseMenuWorld& world,const input::Step& step){
    switch(step.operation){
    case 0x4473f0:r::submit_pause_menu_4473f0(world);return;
    case 0x447100:r::dispatch_pause_menu_input_447100(world);return;
    case 0x446c20:r::submit_pause_menu_rows_446c20(world,std::bit_cast<float>(step.args[0]),std::bit_cast<float>(step.args[1]));return;
    case 0x446b60:r::update_pause_camera_label_446b60(world);return;
    case 0x446af0:r::restart_pause_menu_446af0(world);return;
    case 0x446b20:r::exit_pause_menu_446b20(world);return;
    case 0x446a90:r::increment_pause_audio_b_446a90(world);return;
    case 0x446ac0:r::increment_pause_audio_a_446ac0(world);return;
    case 0x446a70:r::increment_pause_detail_446a70(world);return;
    case 0x446bb0:r::cycle_pause_camera_446bb0(world);return;
    case 0x446b40:r::resume_pause_menu_446b40(world);return;
    case 0x445350:r::set_pause_exit_marker_445350(world.options);return;
    default:throw sgn::FormatError("pause full source entry unsupported");
    }
}
}
