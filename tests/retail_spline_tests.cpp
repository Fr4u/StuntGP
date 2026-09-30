#include "sgn/retail_spline.hpp"
#include <bit>
#include <iostream>
#include <limits>

using namespace sgn;
using namespace sgn::retail;
namespace {
void check(bool b,const char* s){if(!b)throw std::runtime_error(s);}
template<class F> void rejected(F f){bool caught=false;try{f();}catch(const FormatError&){caught=true;}check(caught,"required contract rejection missing");}
void put(Bytes& b,std::size_t at,float x){const auto u=std::bit_cast<std::uint32_t>(x);for(unsigned i=0;i<4;++i)b.at(at+i)=static_cast<std::uint8_t>(u>>(i*8));}
}
int main(){try{
    Bytes storage(4*16);const std::array<VehicleVector,4> points={{{0,0,0,1},{10,0,0,2},{10,0,10,3},{99,88,77,4}}};
    for(std::size_t i=0;i<points.size();++i){put(storage,i*16,points[i].x);put(storage,i*16+4,points[i].y);put(storage,i*16+8,points[i].z);put(storage,i*16+12,points[i].w);}
    const SplineView s{{1,0,3,{0,0,0},30,0},storage};
    const auto adjacent=sample_spline(s,-30);check(adjacent.adjacent_storage_read&&adjacent.value.x==99&&adjacent.value.w==4,"negative exact multiple silently wrapped");
    const auto indexed=sample_spline_indexed(s,-30);check(indexed.selected_index==3,"indexed sample must expose original one-past index");
    auto bounded=s;bounded.storage=bounded.storage.first(48);rejected([&]{sample_spline(bounded,-30);});
    auto padded=s;padded.point_storage_end=48;rejected([&]{sample_spline(padded,-30);});
    auto invalid=s;invalid.descriptor.point_count=0;rejected([&]{sample_spline(invalid,0);});
    rejected([&]{sample_spline(s,std::numeric_limits<float>::infinity());});
    rejected([&]{sample_spline(s,std::numeric_limits<float>::max());});
    std::array<float,128> mask{};rejected([&]{select_respawn(s,0,mask);});
    mask.fill(1);const auto end=select_respawn(s,2,mask);check(end.adjacent_storage_read&&end.position.w==3&&end.persistent_segment==2,"respawn end was wrapped");
    rejected([&]{select_respawn(bounded,2,mask);});
    const auto neg=select_respawn(s,-1,mask);check(neg.persistent_segment==-1&&neg.selected_index==2,"respawn persistent index replaced by wrapped index");
    const auto grid=starting_grid(s,{1,2,0,-1,1,1},0);check(grid[0]&&!grid[1]&&!grid[2]&&!grid[3]&&grid[4]&&grid[5],"active must equal one");
    check(grid[4]->physical_rank==4&&!grid[4]->copied_position_w,"grid slot or undefined W invented");
    const auto mode3=starting_grid(s,{1,1,1,1,1,1},3);check(mode3[5]->position.x==0&&mode3[5]->copied_position_w==1,"mode3 must copy first point");
    const auto distance_start=prepare_distance_start(bounded,25);
    check(!distance_start.adjacent_storage_read&&distance_start.selected_index==2&&distance_start.progress_current==66&&distance_start.progress_percent==66&&distance_start.progress_previous==65,"distance start progress or wrapped heading changed");
    const auto start_zero=prepare_distance_start(s,0);check(start_zero.progress_previous==0xffffffffu,"distance start previous progress must wrap");
    Bytes zero(48);auto degenerate=s;degenerate.storage=zero;rejected([&]{project_spline(degenerate,{0,0,0},0);});
    rejected([&]{starting_grid(degenerate,{1,1,1,1,1,1},0);});
    rejected([&]{SplineSet bad(Bytes(8));});
    const std::string missing="-WrongKey[] = 1 ";
    rejected([&]{decode_respawn_mask(Bytes(missing.begin(),missing.end()));});
    std::string config="-CarReplacementSplineMask[] = ";for(unsigned i=0;i<128;++i)config+="0.975 ";
    const auto decoded=decode_respawn_mask(Bytes(config.begin(),config.end()));
    check(decoded.front()==0.975f&&decoded.back()==0.975f,"mask parse must return 128 binary32 values");
    config.pop_back();rejected([&]{decode_respawn_mask(Bytes(config.begin(),config.end()));});
    std::cout<<"spline bounds, original adjacent storage, persistent indices, active slots and undefined lanes passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
