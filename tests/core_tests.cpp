#include "sgn/assets.hpp"
#include "sgn/clock.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <functional>
#include <string>
#include <bit>
#include <limits>

using namespace sgn;
namespace {
void expect(bool value,const char* what){if(!value) throw std::runtime_error(what);}
void rejects(const std::function<void()>& f){bool rejected=false;try{f();}catch(const FormatError&){rejected=true;}expect(rejected,"corrupt asset accepted");}
void u16(Bytes& b,unsigned v){b.push_back(static_cast<std::uint8_t>(v));b.push_back(static_cast<std::uint8_t>(v>>8));}
Bytes tm(unsigned w,unsigned h,std::initializer_list<unsigned> words) {
    Bytes b{'T','M','!',26};u16(b,3);u16(b,w);u16(b,h);for(auto v:words)u16(b,v);return b;
}
void contracts(){
    auto b=tm(5,1,{0xfc00,0x4800,1,0}); // red + overlapping three pixels + transparent
    auto im=decode_tm(b);expect(im.rgba.size()==20 && im.rgba[0]==255 && im.rgba[15]==255 && im.rgba[19]==0,"TM overlapping LZ");
    rejects([&]{decode_tm(tm(2,1,{0xfc00,0x4002,0}));});
    rejects([&]{decode_tm(tm(1,1,{0xfc00,0x8001,0}));});
    rejects([&]{decode_tm(tm(1,1,{0xfc00,1,0}));});
    rejects([&]{decode_tm(tm(2,1,{0xfc00,0}));});
    rejects([&]{decode_tm(tm(0,1,{0,0}));});
    for(std::size_t n=0;n<b.size();++n) rejects([&]{decode_tm(std::span(b).first(n));});
    expect(name_hash("ABC")==name_hash("abc"),"hash ASCII case folding");
    expect(name_hash("a")==97 && name_hash("ab")==292,"hash known rotate/add");
    rejects([]{Archive(Bytes(5000));});
    // Ten seconds, exact endpoint, every requested display schedule must yield 600 ticks.
    for(int hz:{24,30,50,59,60,75,90,100,120,144,165,240,360,500,1000}) {
        TickClock clock;std::uint64_t checksum=0;std::int64_t previous=0;
        for(std::int64_t frame=1;frame<=hz*10;++frame) {
            auto now=frame*1'000'000'000/hz;
            clock.advance(std::chrono::nanoseconds(now-previous),[&](auto tick){checksum+=tick;}); previous=now;
        }
        expect(clock.ticks()==600 && checksum==179700 && clock.alpha()==0,"display rate altered simulation");
    }
    TickClock stalled;stalled.advance(std::chrono::seconds(3),[](auto){});
    expect(stalled.ticks()==15 && stalled.discarded_ns()==2'750'000'000,"bounded catchup policy");
    stalled.suspend();expect(stalled.alpha()==0,"suspend phase reset");
    TickClock bounded;std::uint64_t accepted=0;
    const auto steps=bounded.advance(std::chrono::milliseconds(250),[&](auto tick){
        if(tick==2)return false;++accepted;return true;
    });
    expect(steps==2&&accepted==2&&bounded.ticks()==2&&bounded.alpha()==0,"ended session advanced inside catchup batch");
    bounded.advance(std::chrono::milliseconds(10),[](auto){return true;});
    expect(bounded.ticks()==2,"ended session leaked stale catchup time");
    bounded.advance(std::chrono::nanoseconds(6'666'667),[](auto){return true;});
    expect(bounded.ticks()==3,"new time after rejected step was not accepted");
    for(std::int64_t numerator:{24000,30000,60000,120000,144000}){
        TickClock fractional;std::int64_t previous=0;
        for(std::int64_t i=1;i<=10*numerator/1001;++i){auto now=i*1'000'000'000*1001/numerator;fractional.advance(std::chrono::nanoseconds(now-previous),[](auto){});previous=now;}
        fractional.advance(std::chrono::nanoseconds(10'000'000'000-previous),[](auto){});
        expect(fractional.ticks()==600 && fractional.alpha()==0,"fractional refresh changed ticks");
    }
    std::cout<<"core contracts passed; refresh schedules=20; bounded stalls and malformed TM covered\n";
}
void assets(const std::filesystem::path& root){
    AssetStore store(root);std::size_t models=0,lods=0,unsupported=0;std::uint64_t triangles=0;
    for(const auto& name:store.models()) {
        auto input=read_file(root/"meshdata"/(name+".PMD"));
        if(input.size()>=9 && std::string(reinterpret_cast<const char*>(input.data()),9)=="PMD V1.82") {
            rejects([&]{decode_pmd(input);});++unsupported;continue;
        }
        try {
            // Explicit inspector selection for companion meshes; no game-level binding inferred.
            std::string selected_set=name.starts_with("track")&&name.ends_with("m")?"trackset00":"";
            auto model=store.model(name,0,selected_set);++models;
            for(unsigned l=0;l<model.lod_count;++l) {
                auto m=store.model(name,l,selected_set);++lods;triangles+=m.vertices.size()/3;
                for(auto& texture:m.textures) store.texture(texture);
                auto ext=m.maximum-m.minimum;expect(std::isfinite(dot(ext,ext)),"non-finite model bounds");
            }
        }catch(const std::exception& e){throw std::runtime_error(name+": "+e.what());}
    }
    auto pixels=store.validate_images();
    std::cout<<"archives="<<store.archive_count()<<" entries="<<store.entry_count()<<" models="<<models<<" unsupported_v182="<<unsupported<<" lods="<<lods<<" triangles="<<triangles<<" decoded_pixels="<<pixels<<"\n";
    expect(store.archive_count()==62 && models==78 && unsupported==10,"unexpected pinned asset corpus");
    // Real PMD corruption: no fake fixture that merely mirrors the implementation.
    auto good=read_file(root/"meshdata/track1.PMD");
    auto damaged=good;damaged[0x20]=0xff;damaged[0x21]=0xff;damaged[0x22]=0xff;damaged[0x23]=0xff;
    rejects([&]{decode_pmd(damaged);});
    rejects([&]{decode_pmd(std::span(good).first(400));});
    rejects([&]{decode_pmd(good,99);});
    // Finite input scalars can still overflow the authored part transform.
    auto oversized=read_file(root/"meshdata/car16.PMD");
    auto get32=[&](std::size_t at){return std::uint32_t(oversized.at(at))|(std::uint32_t(oversized.at(at+1))<<8)|(std::uint32_t(oversized.at(at+2))<<16)|(std::uint32_t(oversized.at(at+3))<<24);};
    const auto transform_begin=328+std::size_t(get32(0x20+9*4));
    const auto transform_bytes=get32(0xb4+9*4);
    const auto maximum=std::bit_cast<std::uint32_t>(std::numeric_limits<float>::max());
    for(std::size_t i=0;i<transform_bytes;i+=80)
        for(unsigned axis=0;axis<3;++axis)
            for(unsigned byte=0;byte<4;++byte)
                oversized.at(transform_begin+i+16+axis*4+byte)=static_cast<std::uint8_t>(maximum>>(byte*8));
    rejects([&]{decode_pmd(oversized);});
}
}
int main(int argc,char** argv){try{contracts();if(argc>1)assets(argv[1]);return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
