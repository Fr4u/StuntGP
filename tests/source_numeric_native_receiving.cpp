#include "sgn/source_numeric_win64.hpp"
#include <array>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <intrin.h>
#include <xmmintrin.h>

namespace {
void require(bool value,const char* why){if(!value)throw std::runtime_error(why);}
// Receiving-only restoration of the calling thread's control state. The
// production429510 deliberately retains its changes, as the original does.
struct Restore final {
    const std::uint16_t control=sgn_source_read_x87_control();
    const unsigned mxcsr=_mm_getcsr();
    ~Restore(){sgn_source_write_x87_control(control);_mm_setcsr(mxcsr);}
};
}
int main()try{
    unsigned cases=0;
    {
        Restore restore;
        require((restore.control&0x3fu)==0x3fu,"native receiving starts in the masked exception domain");
        alignas(16) std::array<unsigned char,512> saved{};
        _fxsave64(saved.data());
        unsigned supported{};std::memcpy(&supported,saved.data()+28,sizeof(supported));
        if(!supported)supported=0xffbfu; // architectural fallback excludes DAZ
        for(const unsigned precision:{0u,2u,3u})for(unsigned rounding=0;rounding<4;++rounding){
            const auto seeded=static_cast<std::uint16_t>((restore.control&~0xf00u)|(precision<<8)|(rounding<<10)|0x3fu);
            sgn_source_write_x87_control(seeded);
            const auto before_control=sgn_source_read_x87_control();
            const auto seeded_sse=(restore.mxcsr|0xffc0u|rounding)&supported;
            _mm_setcsr(seeded_sse);
            const auto before_sse=_mm_getcsr();
            sgn::retail::prepare_source_numeric_win64_429510();
            const auto after_control=sgn_source_read_x87_control();
            const auto after_sse=_mm_getcsr();
            require((after_control&0x1f3fu)==((before_control&0x103fu)|0x200u),"native x87 precision/rounding/masks changed incorrectly");
            require((after_sse&0xffc0u)==0x1f80u,"native SSE rounding/masks/DAZ/FTZ incorrect");
            require((after_sse&0x3fu)==(before_sse&0x3fu),"native SSE status was cleared");
            sgn::retail::prepare_source_numeric_win64_429510();
            require((sgn_source_read_x87_control()&0x1f3fu)==(after_control&0x1f3fu)&&_mm_getcsr()==after_sse,"second actual429510 differs");
            ++cases;
        }
    }
    std::cout<<"{\"passed\":true,\"native_cases\":"<<cases<<",\"actual_429510_calls\":"<<cases*2<<",\"original_executed\":false,\"gameplay_numeric_equivalence\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
