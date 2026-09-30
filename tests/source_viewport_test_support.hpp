#pragma once
#include "sgn/source_viewport_win64.hpp"
#include "sgn/retail_viewport.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>
#include <intrin.h>
#include <xmmintrin.h>
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern "C" void sgn_viewport_test_seed_fpu(std::uint16_t,std::uint16_t,std::uint16_t) noexcept;
namespace viewport_receiving {
using namespace sgn::retail;
inline void require(bool condition,const char* why){if(!condition)throw std::runtime_error(why);}
template<class T>T get(const void* data){T value{};std::memcpy(&value,data,sizeof(value));return value;}
struct alignas(16) FpuImage {std::array<unsigned char,512> data{};};
inline FpuImage capture(){FpuImage image;_fxsave64(image.data.data());return image;}
inline std::uint16_t cw(const FpuImage& image){return get<std::uint16_t>(image.data.data());}
inline std::uint16_t sw(const FpuImage& image){return get<std::uint16_t>(image.data.data()+2);}
inline std::uint32_t mx(const FpuImage& image){return get<std::uint32_t>(image.data.data()+24);}
struct Restore final {
    FpuImage initial=capture();
    Restore(){require((cw(initial)&0x3fu)==0x3fu&&initial.data[4]==0&&(sw(initial)&0x3800u)==0,"receiving host requires masked empty TOP0");}
    ~Restore(){sgn_viewport_test_seed_fpu(cw(initial),sw(initial),0xffffu);_mm_setcsr(mx(initial));}
};
class Records {
    HANDLE handle_{INVALID_HANDLE_VALUE};
public:
    explicit Records(const wchar_t* path){
        handle_=CreateFileW(path,GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);
        require(handle_!=INVALID_HANDLE_VALUE,"receiving output must be new and writable");
    }
    ~Records(){if(handle_!=INVALID_HANDLE_VALUE)CloseHandle(handle_);}
    void write(const std::string& text){
        require(text.size()<0x100000u,"receiving record exceeds bound");
        const auto line=text+"\n";DWORD written{};
        require(WriteFile(handle_,line.data(),static_cast<DWORD>(line.size()),&written,nullptr)!=0&&written==line.size(),"write receiving record");
        require(FlushFileBuffers(handle_)!=0,"flush receiving record");
    }
};
constexpr std::array<std::size_t,6> offsets{offsetof(ViewportState,origin_x),offsetof(ViewportState,origin_y),
    offsetof(ViewportState,center_pixel_x),offsetof(ViewportState,center_pixel_y),offsetof(ViewportState,center_x),offsetof(ViewportState,center_y)};
constexpr std::array<std::uint32_t,6> pcs{0x424beb,0x424bf0,0x424c03,0x424c10,0x424c15,0x424c1b};
inline void word(ViewportState& state,std::size_t offset,std::uint32_t bits){std::memcpy(reinterpret_cast<unsigned char*>(&state)+offset,&bits,4);}
inline std::uint32_t word(const ViewportState& state,std::size_t offset){return get<std::uint32_t>(reinterpret_cast<const unsigned char*>(&state)+offset);}
inline void initialize(ViewportState& state,std::uint32_t sx=0,std::uint32_t sy=0){
    std::memset(&state,0xa5,sizeof(state));word(state,offsetof(ViewportState,scale_x),sx);word(state,offsetof(ViewportState,scale_y),sy);
}
inline std::string observation(unsigned id,const char* group,const SourceViewportObservation& trace,const FpuImage& after){
    std::ostringstream out;out<<"{\"id\":"<<id<<",\"group\":\""<<group<<"\",\"store_count\":"<<trace.store_count<<",\"stores\":[";
    for(std::size_t i=0;i<trace.store_count&&i<6;++i){if(i)out<<',';out<<'['<<trace.stores[i].source_pc<<','<<trace.stores[i].value<<']';}
    out<<"],\"fistp64\":["<<trace.fistp64[0]<<','<<trace.fistp64[1]<<"],\"CW\":"<<cw(after)<<",\"SW\":"<<sw(after)<<",\"tag\":"<<unsigned(after.data[4])<<",\"MXCSR\":"<<mx(after)<<'}';return out.str();
}
}
