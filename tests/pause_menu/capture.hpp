#pragma once
#include "native_fixture.hpp"
#include "audio_boundary.hpp"
#include <sstream>

namespace pause_menu_test {
inline std::vector<unsigned> core(const MenuOwners& menu){
    const auto words=capture(menu);std::vector<unsigned> out(words.begin(),words.begin()+5);
    out.insert(out.end(),words.begin()+6,words.end());return out;
}
template<class T>sgn::Bytes object_bytes(const T& object){
    static_assert(std::is_trivially_copyable_v<T>);const auto* p=reinterpret_cast<const std::uint8_t*>(&object);return {p,p+sizeof(T)};
}
inline void save_bytes(const std::filesystem::path& path,std::span<const std::uint8_t> bytes){
    std::ofstream out(path,std::ios::binary);input::require(bool(out.write(reinterpret_cast<const char*>(bytes.data()),static_cast<std::streamsize>(bytes.size()))),"pause native output write");
}
inline unsigned get_word(std::span<const std::uint8_t> bytes,std::size_t offset){
    input::require(offset<=bytes.size()&&bytes.size()-offset>=4,"pause capture word bound");unsigned value;std::memcpy(&value,bytes.data()+offset,4);return value;
}
inline void put_word(std::span<std::uint8_t> bytes,std::size_t offset,unsigned value){
    input::require(offset<=bytes.size()&&bytes.size()-offset>=4,"pause capture word bound");std::memcpy(bytes.data()+offset,&value,4);
}
// Descriptor identities are decoded from the immutable PE table supplied by
// the input packager. The expected post-menu descriptor table is never read.
inline void import_descriptor_seed(NativeFixture& w,const std::filesystem::path& seed,const std::filesystem::path& inputs){
    const auto definitions=sgn::read_file(inputs/"descriptor-definitions.bin");const auto raw=sgn::read_file(seed/"game-text-descriptors.bin");
    input::require(definitions.size()==104*8&&raw.size()==105*8,"pause text descriptor seed extent");
    for(unsigned i=0;i<105;++i){std::uint64_t value;std::memcpy(&value,raw.data()+i*8,8);
        if(value==0){w.game_text.descriptor_order[i]=0xffffffffu;continue;}bool found=false;
        for(unsigned j=0;j<104;++j)if(std::memcmp(raw.data()+i*8,definitions.data()+j*8,8)==0){w.game_text.descriptor_order[i]=j;found=true;break;}
        input::require(found,"pause text descriptor source identity");}
}

class StateCapture {
    sgn::Bytes font_seed_,descriptor_definitions_,race_args_;
    std::vector<unsigned> extra_addresses_;
public:
    StateCapture(const std::filesystem::path& seed,const std::filesystem::path& inputs):
        font_seed_(sgn::read_file(seed/"font-state.bin")),descriptor_definitions_(sgn::read_file(inputs/"descriptor-definitions.bin")),race_args_(sgn::read_file(seed/"race-args.bin")){
        // The PRE input declares the versioned observation schema. Values
        // always come from live owners; no expected POST state is loaded.
        const auto extra=sgn::read_file(seed/"extra-words.bin");
        input::require(extra.size()%8==0,"pause capture extra schema extent");
        for(std::size_t at=0;at<extra.size();at+=8){const auto address=get_word(extra,at);
            input::require(std::find(extra_addresses_.begin(),extra_addresses_.end(),address)==extra_addresses_.end(),"pause capture duplicate extra address");
            extra_addresses_.push_back(address);}
    }
    void save(NativeFixture& w,const std::filesystem::path& folder){
        input::require(!std::filesystem::exists(folder),"fresh native capture folder");std::filesystem::create_directories(folder);
        const auto save=[&](const char* name,std::span<const std::uint8_t> b){save_bytes(folder/(std::string(name)+".bin"),b);};
        save("race-args",race_args_); // input metadata, not the live scene/type words
        save("cars",object_bytes(w.cars));save("selection",object_bytes(w.selection));save("chosen",object_bytes(w.chosen));
        for(unsigned i=0;i<8;++i)bt::transfer(w.vehicle_carrier[i],w.vehicles,i,false,w.models);
        save("vehicles",object_bytes(w.vehicle_carrier));
        rt::globals(w.race_carrier,w.initialization(w.reset),false);save("race-globals",object_bytes(w.race_carrier));
        rt::controllers(w.controller_carrier,w.hud(),false);
        for(unsigned i=0;i<4;++i){const auto base=i*rt::controller_stride;
            std::memcpy(w.controller_carrier.data()+base+0x1e8,&w.overlays[i],40);
            const auto v=get_word(w.controller_carrier,base+0xac);put_word(w.controller_carrier,base+0xac,0x49a1f8+v*0x7724);}
        save("controllers",w.controller_carrier);
        // Restore only the carrier's identity codec for the next native capture.
        // It is not read into any native owner after initial construction.
        for(unsigned i=0;i<4;++i)put_word(w.controller_carrier,i*rt::controller_stride+0xac,w.controls.controllers[i].vehicle);
        at::transfer(w.audio_carrier,w.driver_state,w.audio_initialization,w.maintenance,w.stream,w.level,w.options,false);
        // The older initialization-only codec did not emit these driver
        // fields. They are already owned and updated by the real AudioDriver;
        // capture them here instead of leaving stale PRE-carrier bytes.
        w.audio_carrier[at::at(0x62f5b0)]=w.driver_state.duplicate_count;
        w.audio_carrier[at::at(0x62f5b4)]=w.driver_state.playing_count;
        w.audio_carrier[at::at(0x4897a0)]=w.driver_state.range;
        w.audio_carrier[at::at(0x4897a4)]=w.driver_state.range_squared;
        w.audio_carrier[at::at(0x4897a8)]=std::bit_cast<unsigned>(w.driver_state.inverse_range);
        std::memcpy(w.audio_carrier.data()+at::at(0x4897b8),&w.driver_state.spatial_template,64);
        // These final carrier words are shared world inputs, omitted by the
        // initialization helper's signature. Export their actual live owners.
        w.audio_carrier[0x1a88/4]=w.split;
        w.audio_carrier[0x1a8c/4]=static_cast<unsigned>(w.audio.window);
        save("audio",object_bytes(w.audio_carrier));save("audio-spatial-template",object_bytes(w.driver_state.spatial_template));
        save("physical-bindings",object_bytes(w.frontend.input_bindings));
        save("game-text-arena",object_bytes(w.game_text.arena));std::array<unsigned,105> pointers;
        for(unsigned i=0;i<105;++i)pointers[i]=w.game_text.offsets[i]?0x60f6a0+*w.game_text.offsets[i]:0;
        save("game-text-pointers",object_bytes(pointers));sgn::Bytes descriptors(840);
        for(unsigned i=0;i<105;++i){const auto id=w.game_text.descriptor_order[i];if(id==0xffffffffu)continue;
            input::require(id<104,"pause descriptor output identity");std::memcpy(descriptors.data()+i*8,descriptor_definitions_.data()+id*8,8);}
        save("game-text-descriptors",descriptors);save("game-text-scalar",object_bytes(std::array{w.game_text.remaining_bytes,w.game_text.language}));
        save("menu",object_bytes(std::array{w.menu.physical.token?0x62d580+(w.menu.physical.token-1)*0x1dc:0,w.menu.release,w.menu.selection,w.menu.highlight,w.menu.ready}));
        save("rows",object_bytes(w.menu.rows));save("descriptor",object_bytes(w.ui.quad));
        input::require(w.submission.alpha_vertices.size()==8,"pause native alpha extent");
        save("alpha",{reinterpret_cast<const std::uint8_t*>(w.submission.alpha_vertices.data()),320});
        static_assert(sizeof(r::ViewportState)==152);save("viewport",object_bytes(w.ui.viewport));
        const unsigned font_pointer=w.ui.fonts.current?0x577bd0+(w.ui.fonts.current-1)*0x1ba0:0;
        save("ui-globals",object_bytes(std::array{font_pointer,w.ui.fonts.count,w.ui.fonts.digit_advance_bits,w.ui.selected,w.submission.specular_enabled,w.ui.quality,w.ui.legacy}));
        save("clip",object_bytes(w.ui.clip.words));save("edges",object_bytes(capture_edges(w.menu)));
        save("selected-overlay",object_bytes(w.textures.selected_overlay_texture()));
        auto font=font_seed_;constexpr auto globals=pause_meter_wire::globals;
        for(unsigned i=0;i<2;++i)for(unsigned offset=0;offset<0x1ba0;offset+=4)put_word(font,i*0x1ba0+offset,w.ui.fonts.source_word(i,offset));
        std::memcpy(font.data()+pause_meter_wire::index,w.ui.fonts.index.data(),160);std::memcpy(font.data()+pause_meter_wire::map,w.ui.fonts.glyph_map.data(),256);
        put_word(font,globals,w.ui.fonts.current);put_word(font,globals+4,w.ui.fonts.count);put_word(font,globals+8,w.ui.fonts.digit_advance_bits);
        put_word(font,globals+12,static_cast<unsigned>(w.materials.size()));
        for(unsigned slot=0;slot<255;++slot)for(unsigned lane=0;lane<7;++lane)put_word(font,globals+68+slot*28+lane*4,w.materials.source_word(slot,lane*4));
        save("font-state",font);
        const auto fields=w.extra_fields();sgn::Bytes extra;
        for(const auto address:extra_addresses_){
            if(address==0x62df08){const auto pair=object_bytes(std::array{address,static_cast<unsigned>(w.maintenance.music.buffer.value)});extra.insert(extra.end(),pair.begin(),pair.end());continue;}
            const auto field=fields.find(address);
            input::require(field!=fields.end(),"pause capture unknown extra address");
            unsigned value;std::memcpy(&value,field->second,4);const auto pair=object_bytes(std::array{address,value});extra.insert(extra.end(),pair.begin(),pair.end());}
        save("extra-words",extra);
        for(const auto& [slot,image]:w.texture_device.destinations){
            save_bytes(folder/("texture-"+std::to_string(slot)+".rgba"),image.rgba);
            sgn::Bytes packed(image.rgba.size()/2);
            for(std::size_t i=0;i<image.rgba.size()/4;++i){unsigned value=image.rgba[i*4+3]>=128?0x8000u:0u;
                for(unsigned c=0;c<3;++c)value|=((unsigned(image.rgba[i*4+c])*31+127)/255)<<(10-c*5);
                packed[i*2]=static_cast<std::uint8_t>(value);packed[i*2+1]=static_cast<std::uint8_t>(value>>8);}
            save_bytes(folder/("texture-"+std::to_string(slot)+"-u16.bin"),packed);
            save_bytes(folder/("texture-"+std::to_string(slot)+"-extent.bin"),object_bytes(std::array{image.width,image.height}));
        }
    }
};
template<class T>void json_words(std::ostream& out,const T& values){out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<value;}out<<']';}
inline void json_draw(std::ostream& out,const DrawEvent& e){
    const auto& q=e.request;const unsigned op=q[0];out<<"{\"boundary\":\"draw\",\"operation\":"<<op<<",\"args\":";
    if(op<4)json_words(out,std::span(q).subspan(1));
    else {input::require(op==4&&q[4]==0,"pause source draw primitive boundary");json_words(out,std::array{q[1],q[2],q[3],q[5]});
        out<<",\"local\":"<<(e.local_vertices?"true":"false")<<",\"vertices\":";std::vector<unsigned> words;
        for(const auto& v:e.vertices)for(unsigned j=0;j<10;++j)words.push_back(e.local_vertices&&j>=8?0:v.words[j]);json_words(out,words);}
    out<<",\"state\":";json_words(out,e.state_before);out<<",\"result\":0}\n";
}
inline void json_audio(std::ostream& out,const audio_boundary::Event& e){
    out<<"{\"boundary\":\"audio\",\"operation\":"<<e.operation<<",\"object\":"<<e.object<<",\"args\":";json_words(out,e.arguments);
    out<<",\"state\":";json_words(out,e.state_before);out<<",\"result\":"<<e.result<<",\"output\":"<<e.output<<"}\n";
}
}
