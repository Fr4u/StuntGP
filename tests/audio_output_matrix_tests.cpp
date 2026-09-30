#include "sgn/audio_output_matrix.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
using sgn::audio_host::scale_output_matrix_destination;
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Fixture {
    std::string_view name;unsigned sources,mask;
    std::vector<float> base,left,right,partial_left,partial_right;
};
// Each destination is listed explicitly: FL,FR[,FC,LFE,BL,BR[,SL,SR]].
// Stereo rows contain source L,R in adjacent positions. Expected arrays below
// are labeled examples, not generated using the implementation's index.
const std::array<Fixture,6> fixtures{{
    {"mono->stereo",1,0x3,
        {10,20},{10,0},{0,20},{10,2},{1,20}},
    {"mono->5.1",1,0x3f,
        {10,20,30,40,50,60},
        {10,0,30,40,50,0},{0,20,30,40,0,60},
        {10,2,30,40,50,6},{1,20,30,40,5,60}},
    {"mono->7.1",1,0x63f,
        {10,20,30,40,50,60,70,80},
        {10,0,30,40,50,0,70,0},{0,20,30,40,0,60,0,80},
        {10,2,30,40,50,6,70,8},{1,20,30,40,5,60,7,80}},
    {"stereo->stereo",2,0x3,
        {10,20,30,40},{10,20,0,0},{0,0,30,40},{10,20,3,4},{1,2,30,40}},
    {"stereo->5.1",2,0x3f,
        {10,20,30,40,50,60,70,80,90,100,110,120},
        {10,20,0,0,50,60,70,80,90,100,0,0},
        {0,0,30,40,50,60,70,80,0,0,110,120},
        {10,20,3,4,50,60,70,80,90,100,11,12},
        {1,2,30,40,50,60,70,80,9,10,110,120}},
    {"stereo->7.1",2,0x63f,
        {10,20,30,40,50,60,70,80,90,100,110,120,130,140,150,160},
        {10,20,0,0,50,60,70,80,90,100,0,0,130,140,0,0},
        {0,0,30,40,50,60,70,80,0,0,110,120,0,0,150,160},
        {10,20,3,4,50,60,70,80,90,100,11,12,130,140,15,16},
        {1,2,30,40,50,60,70,80,9,10,110,120,13,14,150,160}}
}};
float existing_gain(std::int32_t value){return value==-10000?0.0f:std::pow(10.0f,static_cast<float>(value)/2000.0f);}
std::vector<float> pan(const Fixture& fixture,std::int32_t value){
    auto matrix=fixture.base;const auto destinations=matrix.size()/fixture.sources;unsigned channel=0;
    constexpr unsigned left=0x1|0x10|0x200,right=0x2|0x20|0x400;
    for(unsigned bit=0;bit<32&&channel<destinations;++bit)if(fixture.mask&(1u<<bit)){
        const unsigned speaker=1u<<bit;float balance=1;
        if((speaker&left)&&value>0)balance=existing_gain(-value);
        if((speaker&right)&&value<0)balance=existing_gain(value);
        require(scale_output_matrix_destination(matrix,fixture.sources,destinations,channel,balance),"valid matrix rejected");
        ++channel;
    }
    return matrix;
}
void labeled_cases(){
    for(const auto& fixture:fixtures){
        require(pan(fixture,0)==fixture.base,"pan0 did not roundtrip all labeled channels");
        require(pan(fixture,-10000)==fixture.left,"extreme-left pan modified the wrong source/destination");
        require(pan(fixture,10000)==fixture.right,"extreme-right pan modified the wrong source/destination");
        require(pan(fixture,-2000)==fixture.partial_left,"partial-left pan modified the wrong source/destination");
        require(pan(fixture,2000)==fixture.partial_right,"partial-right pan modified the wrong source/destination");
        std::cout<<fixture.name<<" five labeled pan cases PASS\n";
    }
}
void basis_and_guards(){
    // The only routed signal is source R -> FrontRight, explicit SDK index3.
    std::array<float,12> basis{0,0,0,1,0,0,0,0,0,0,0,0};
    require(scale_output_matrix_destination(basis,2,6,1,0),"basis matrix rejected");
    require(basis==std::array<float,12>{},"source R -> FR basis was not muted");
    // Exact destination block surrounded by unrelated coefficients/canaries.
    std::array<float,8> guarded{99,10,20,30,40,50,60,88};
    require(scale_output_matrix_destination(std::span(guarded).subspan(1,6),2,3,1,0.5f),"guarded matrix rejected");
    require(guarded==std::array<float,8>{99,10,20,15,20,50,60,88},"scaling wrote another row or guard");
    std::array<float,4> signed_zero{-0.0f,1.25f,-3.5f,0.0f};const auto original=signed_zero;
    require(scale_output_matrix_destination(signed_zero,2,2,0,1)&&scale_output_matrix_destination(signed_zero,2,2,1,1),"unit gain rejected");
    for(std::size_t i=0;i<original.size();++i)require(std::bit_cast<std::uint32_t>(signed_zero[i])==std::bit_cast<std::uint32_t>(original[i]),"unit gain lost signed zero or exact coefficient");
}
void invalid_dimensions(){
    std::array<float,6> values{1,2,3,4,5,6};const auto original=values;
    struct Dimensions {std::size_t sources,destinations,destination;};
    const auto huge=std::numeric_limits<std::size_t>::max();
    const std::array<Dimensions,7> cases{{{0,3,0},{2,0,0},{2,3,3},{1,3,0},{2,4,0},{huge,2,0},{2,huge,0}}};
    for(const auto& dims:cases){
        require(!scale_output_matrix_destination(values,dims.sources,dims.destinations,dims.destination,0),"invalid dimensions accepted");
        require(values==original,"invalid dimensions partially wrote matrix");
    }
    require(!scale_output_matrix_destination({},1,1,0,0)&&!scale_output_matrix_destination({},0,0,0,0),"empty matrix accepted");
}
}
int main(){try{
    labeled_cases();basis_and_guards();invalid_dimensions();
    std::cout<<"AUDIO_OUTPUT_MATRIX_CPU_PASS:30 explicit labeled pan cases;basis;guarded span;bitwise pan0;9 invalid dimensions\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}}
