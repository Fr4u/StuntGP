#include "sgn/retail_replay.hpp"
#include <algorithm>
#include <bit>
#include <cstring>
#include <iostream>
#include <limits>

namespace {
using namespace sgn::retail;
void require(bool value){if(!value)throw std::runtime_error("replay host contract failed");}
void put(ReplayBank& bank,std::uint32_t at,std::uint32_t value){std::memcpy(bank.bytes().data()+at,&value,4);}
template<class F> void reject_unchanged(F action,ReplayBank& bank,VehicleRaceState& race,ReplayWorkspace& workspace) {
    const std::vector<std::uint8_t> before(bank.bytes().begin(),bank.bytes().end());
    const auto recording=race.recording,selected=workspace.recording;
    const auto frame=workspace.frame_offset,previous=workspace.previous_frame_offset;
    bool rejected=false;try{action();}catch(const std::exception&){rejected=true;}
    require(rejected && std::equal(before.begin(),before.end(),bank.bytes().begin()));
    require(recording==race.recording && selected==workspace.recording && frame==workspace.frame_offset && previous==workspace.previous_frame_offset);
}
}
int main() {
    try {
        ReplayBank bank;VehicleRaceState race{};VehicleState vehicle{};
        ReplayObservation observed{};ReplayWorkspace workspace{ReplayHandle{3},100,200};
        begin_recording(bank,{0},race);
        reject_unchanged([&]{begin_recording(bank,{6},race);},bank,race,workspace);
        reject_unchanged([&]{begin_playback(bank,{6},race,vehicle);},bank,race,workspace);
        reject_unchanged([&]{stop_replay(bank,{6});},bank,race,workspace);
        const auto record=[&]{record_vehicle(bank,race,vehicle,observed,9.8f,workspace);};
        unsigned checked=3;
        for(const auto cursor:{0u,0x81u,ReplayBank::stride+0x80}) {
            begin_recording(bank,{0},race);put(bank,0x10,ReplayBank::source_base+cursor);
            reject_unchanged(record,bank,race,workspace);++checked;
        }
        begin_recording(bank,{0},race);put(bank,8,0x7fc00000);
        reject_unchanged(record,bank,race,workspace);++checked;
        begin_recording(bank,{0},race);put(bank,0xc,0x7f800000);
        reject_unchanged(record,bank,race,workspace);++checked;
        begin_recording(bank,{0},race);vehicle.position.x=std::numeric_limits<float>::infinity();
        reject_unchanged(record,bank,race,workspace);++checked;vehicle.position.x=0;
        begin_recording(bank,{5},race);put(bank,5*ReplayBank::stride+0x10,ReplayBank::source_base+ReplayBank::size-ReplayBank::frame_bytes);
        reject_unchanged(record,bank,race,workspace);++checked;
        std::cout<<"{\"rejected_without_mutation\":"<<checked<<",\"passed\":true}\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
