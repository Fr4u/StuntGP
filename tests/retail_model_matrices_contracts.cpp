#include "sgn/retail_model_matrices.hpp"
#include <cstring>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
unsigned checks{};
void require(bool ok,const char* m){if(!ok)throw FormatError(m);++checks;}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const FormatError&){bad=true;}require(bad,"required matrix guard missing");}
}
int main()try{
    ViewMatrixArena arena;ModelTransform pose;pose.position={1,2,3};pose.forward={4,5,6};pose.up={0,1,0};pose.tail={11,22,33,44};
    for(const auto cursor:{std::optional<std::uint32_t>{},std::optional<std::uint32_t>{1},std::optional<std::uint32_t>{383*64},std::optional<std::uint32_t>{385*64}}){
        arena.cursor=cursor;arena.count=7;const auto before=arena;const auto old_pose=pose;
        rejects([&]{allocate_outer_model_matrices(pose,arena);});require(arena.cursor==before.cursor&&arena.count==7&&arena.matrices==before.matrices&&std::memcmp(&pose,&old_pose,80)==0,"guard mutated matrix owner");}
    arena.cursor=382*64;arena.count=0xffffffffu;allocate_outer_model_matrices(pose,arena);
    require(arena.cursor==384u*64&&arena.count==1&&pose.tail[1]==382*64+2&&pose.tail[2]==383*64+2,"last legal pair or counter wrap");
    require(pose.tail[0]==11&&pose.tail[3]==44,"outer writer changed unrelated tail");
    CameraMatrix identity;identity.fill(7);
    require(&resolve_model_matrix(arena,identity,1)==&identity,"identity reference must be borrowed");
    require(&resolve_model_matrix(arena,identity,383*64+2)==&arena.matrices[383],"last matrix resolution");
    for(const auto token:{0u,3u,384u*64+2,0xffffffffu})rejects([&]{(void)resolve_model_matrix(arena,identity,token);});
    std::uint16_t on=1,off=0,nonboolean=2;Vec3 pivot{};ModelTransform shared;shared.forward={0,0,1};shared.up={0,1,0};
    const std::array<ModelPartBinding,3> bindings{{{on,shared,pivot},{nonboolean,shared,pivot},{off,shared,pivot}}};
    arena.cursor=0;arena.count=0;allocate_model_part_matrices(bindings,arena);
    require(arena.count==4&&arena.cursor==256u&&shared.tail[0]==1&&shared.tail[1]==1,"last disabled alias or repeated allocation lost");
    const auto bytes=arena.matrices;arena.cursor.reset();allocate_model_part_matrices(std::span(bindings).last(1),arena);
    require(!arena.cursor&&arena.count==4&&arena.matrices==bytes,"disabled parts must not allocate");
    allocate_model_part_matrices({},arena);require(!arena.cursor&&arena.count==4,"zero-part dispatch changed arena");
    arena.cursor=382*64;shared.tail={10,20,30,40};const auto before=shared;
    rejects([&]{allocate_model_part_matrices(bindings,arena);});require(std::memcmp(&shared,&before,80)==0&&arena.cursor==382u*64,"capacity preflight must preserve aliased transforms");
    initialize_model_matrix_identity(identity);require(identity==vehicle_identity(),"full source identity writer");
    std::cout<<"matrix_contracts="<<checks<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
