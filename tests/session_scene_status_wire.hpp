#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace session_status_wire {
using Bytes=std::vector<std::uint8_t>;
using Events=std::vector<std::vector<std::uint32_t>>;
inline constexpr std::uint32_t screen_base=0x10000000,screen_bytes=256*272;
inline constexpr std::uint32_t controller_base=0x498248,controller_stride=0x74c,vehicle_base=0x49a1f8,vehicle_stride=0x7724;
inline constexpr std::array<std::uint32_t,38> viewport_addresses={
    0x576b80,0x576b84,0x576b8c,0x576b90,0x576b94,0x576b98,0x576b9c,0x576ba0,0x576ba4,0x576ba8,
    0x576bac,0x576bb0,0x576bb4,0x576bb8,0x576bbc,0x576bc0,0x576bc4,0x576bc8,0x576bd8,0x576bdc,0x576be4,0x576be8,
    0x576c04,0x576c08,0x576c0c,0x576c10,0x576c14,0x576c18,0x576c1c,0x576c20,0x576c24,0x576c28,0x576c2c,0x576c30,
    0x624988,0x62498c,0x624990,0x624994};
inline void require(bool b,const std::string& why){if(!b)throw std::runtime_error(why);}
inline std::uint32_t narrow(std::size_t n){require(n<=0xffffffffu,"status wire size overflow");return static_cast<std::uint32_t>(n);}
inline void write(std::ostream& out,const void* data,std::size_t size){require(bool(out.write(static_cast<const char*>(data),static_cast<std::streamsize>(size))),"status output write");}
inline void read(std::istream& in,void* data,std::size_t size){require(bool(in.read(static_cast<char*>(data),static_cast<std::streamsize>(size))),"truncated status input");}
inline void number(std::ostream& out,std::uint32_t value){write(out,&value,4);}
inline std::uint32_t number(std::istream& in){std::uint32_t value;read(in,&value,4);return value;}
inline std::uint32_t word(const void* data){std::uint32_t v;std::memcpy(&v,data,4);return v;}
struct Block {std::uint32_t address{};std::string name;Bytes bytes;};
struct Snapshot {
    std::vector<Block> blocks;
    void add(std::uint32_t at,std::string name,const void* data,std::size_t size){
        require(size>0&&size<=1024*1024&&std::uint64_t(at)+size<=0x100000000ull,"status block extent");
        for(const auto& b:blocks)require(std::uint64_t(at)+size<=b.address||std::uint64_t(b.address)+b.bytes.size()<=at,"overlapping status owner blocks");
        const auto* p=static_cast<const std::uint8_t*>(data);blocks.push_back({at,std::move(name),Bytes(p,p+size)});
    }
    template<class T>void field(std::uint32_t at,std::string name,const T& value){static_assert(std::is_trivially_copyable_v<T>);add(at,std::move(name),&value,sizeof(value));}
    std::uint32_t get(std::uint32_t at) const{
        for(const auto& b:blocks)if(at>=b.address&&std::uint64_t(at)+4<=std::uint64_t(b.address)+b.bytes.size())return word(b.bytes.data()+at-b.address);
        throw std::runtime_error("status scalar outside exported owners");
    }
    void save(std::ostream& out)const{
        number(out,narrow(blocks.size()));for(const auto& b:blocks){number(out,b.address);number(out,narrow(b.name.size()));write(out,b.name.data(),b.name.size());number(out,narrow(b.bytes.size()));write(out,b.bytes.data(),b.bytes.size());}
    }
    static Snapshot load(std::istream& in){
        Snapshot s;const auto n=number(in);require(n>0&&n<=256,"status block count");
        for(unsigned i=0;i<n;++i){const auto at=number(in),names=number(in);require(names>0&&names<=128,"status owner name");std::string name(names,'\0');read(in,name.data(),names);
            const auto bytes=number(in);require(bytes>0&&bytes<=1024*1024,"status input block size");Bytes data(bytes);read(in,data.data(),bytes);s.add(at,std::move(name),data.data(),data.size());}return s;
    }
};
inline void equal(const Snapshot& a,const Snapshot& b,const std::string& where){
    require(a.blocks.size()==b.blocks.size(),where+" block count");
    for(std::size_t i=0;i<a.blocks.size();++i){const auto& x=a.blocks[i];const auto& y=b.blocks[i];
        require(x.address==y.address&&x.name==y.name&&x.bytes==y.bytes,where+" owner "+x.name);}
}
inline void save_events(std::ostream& out,const Events& events){number(out,narrow(events.size()));for(const auto& e:events){number(out,narrow(e.size()));write(out,e.data(),e.size()*4);}}
inline Events load_events(std::istream& in){
    const auto count=number(in);require(count<=4096,"status event count");Events events;
    for(unsigned i=0;i<count;++i){const auto size=number(in);require(size>0&&size<=4096,"status event size");std::vector<std::uint32_t> e(size);read(in,e.data(),size*4);events.push_back(std::move(e));}return events;
}
inline bool opaque_glyph_word(const std::vector<std::uint32_t>& event,std::size_t word_index){
    return event.size()==45&&event[0]==4&&word_index>=5&&((word_index-5)%10)>=8;
}
inline void magic(std::ostream& out){write(out,"SGNSST01",8);}
inline void magic(std::istream& in){std::array<char,8> s;read(in,s.data(),8);require(std::memcmp(s.data(),"SGNSST01",8)==0,"status corpus magic");}
}
