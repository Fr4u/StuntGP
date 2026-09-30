#include "source_viewport_test_support.hpp"
#if SGN_VIEWPORT_EXPECT_BREAK != 1 && SGN_VIEWPORT_EXPECT_BREAK != 2
#error Select one concrete source temporary-CW checkpoint.
#endif
using namespace viewport_receiving;
namespace {
// This is the outer receiving handler. The real production wrapper's __finally
// executes while unwinding the real assembly FRAME before this handler runs.
unsigned catch_probe(ViewportState* state,SourceViewportObservation* trace,std::uint16_t control){
    unsigned exception{};
    __try {
        (void)set_viewport_origin_win64_424bd0(*state,0x3fc00000u,0x40200000u,control,trace);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        exception=GetExceptionCode();
    }
    return exception;
}
}
int wmain(int argc,wchar_t** argv)try{
    require(argc==3&&std::wstring(argv[1])==L"--main-reviewed","Main only: --main-reviewed fresh-results.jsonl");
    Records records(argv[2]);Restore restore;
    unsigned count=0;
    for(const unsigned pc:{0u,2u,3u})for(unsigned rc=0;rc<4;++rc){
        const auto control=static_cast<std::uint16_t>(0x007fu|(pc<<8)|(rc<<10));
        ViewportState state;initialize(state);auto expected=state;
        SourceViewportObservation trace{};
        sgn_viewport_test_seed_fpu(control,0,0xffffu);_mm_setcsr(0x1fa1u);
        const auto exception=catch_probe(&state,&trace,control);const auto after=capture();
        sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
        records.write(observation(count++,"abnormal_unwind",trace,after));
        require(exception==EXCEPTION_BREAKPOINT,"real injected breakpoint reached outer SEH");
        require(cw(after)==control&&mx(after)==0x1fa1u,"abnormal unwind failed entry CW/MXCSR preservation");
        require((sw(after)&0x3800u)==0x3000u&&after.data[4]==0xc0u,"abnormal unwind unexpectedly reset x87 stack");
        word(expected,offsets[0],0x3fc00000u);word(expected,offsets[1],0x40200000u);
        if constexpr(SGN_VIEWPORT_EXPECT_BREAK==2){
            word(expected,offsets[2],1);require(trace.fistp64[0]==1,"first conversion retained before second probe");
            require((sw(after)&0x20u)!=0,"first conversion sticky PE cleared by finalizer");
        }
        constexpr auto stores=SGN_VIEWPORT_EXPECT_BREAK==1?2u:3u;
        require(trace.store_count==stores&&std::memcmp(&state,&expected,sizeof(state))==0,"abnormal unwind partial stores changed or rolled back");
        for(unsigned i=0;i<stores;++i)require(trace.stores[i].source_pc==pcs[i]&&trace.stores[i].value==word(state,offsets[i]),"abnormal unwind source prefix readbacks");
    }
    records.write("{\"status\":\"NATIVE_UNWIND_RECEIVING_PASS\",\"cases\":12,\"probe\":"+std::to_string(SGN_VIEWPORT_EXPECT_BREAK)+",\"production_wrapper\":true,\"test_only_INT3\":true}");return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
