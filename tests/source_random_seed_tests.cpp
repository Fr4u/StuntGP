// Written, not compiled/executed by Session. Tests the imported RNG state
// contract, not wall-clock time, full42F1A0, Session binding or full442120.
#include "sgn/source_random_seed.hpp"
#include "sgn/vehicle_math.hpp"
#include <array>
#include <iostream>
#include <stdexcept>

using namespace sgn::retail;
namespace {
void require(bool value,const char* message){if(!value)throw std::runtime_error(message);}
}
int main(){
    try{
        VehicleRandom random;
        for(const auto seed:std::array<std::uint32_t,6>{0,1,0x7fffffffu,0x80000000u,0xffffffffu,0x12345678u}){
            random.state=0xdeadbeefu;seed_source_random_42f190(random,seed);
            require(random.state==seed,"seed bits changed or implicit random draw occurred");
        }
        // Static witnesses derived from the pinned463289 IMUL/ADD and32-bit
        // wrap. Consume via the EXISTING VehicleRandom, proving the shared owner.
        const std::array<std::uint32_t,4> seeds{0,1,0xffffffffu,0x80000000u};
        const std::array<std::uint32_t,4> first_states{0x00269ec3u,0x0029e2c0u,0x00235ac6u,0x80269ec3u};
        for(std::size_t i=0;i<seeds.size();++i){
            seed_source_random_42f190(random,seeds[i]);(void)random.signed_value(1.0f);
            require(random.state==first_states[i],"existing RNG did not consume seeded state");
        }
        VehicleRandom other;other.state=0x76543210u;
        seed_source_random_42f190(random,123);require(other.state==0x76543210u,"seed escaped supplied owner");
        (void)random.signed_value(1.0f);seed_source_random_42f190(random,123);
        require(random.state==123,"explicit reseed was skipped");
        std::cout<<"4 source seed receiving groups passed\n";return 0;
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
