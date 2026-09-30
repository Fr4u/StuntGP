#include "source_viewport_test_support.hpp"
using namespace viewport_receiving;
namespace {
struct Case {
    std::uint32_t id;std::uint16_t control,reserved;
    std::array<std::uint32_t,4> inputs;
    std::array<std::uint32_t,6> expected;
    std::array<std::uint64_t,2> ftol;
};
static_assert(sizeof(Case)==64&&std::endian::native==std::endian::little);
std::vector<Case> load_cases(const wchar_t* path){
    std::ifstream stream(std::filesystem::path(path),std::ios::binary|std::ios::ate);
    require(bool(stream)&&stream.tellg()==7696,"native120 fixture exact size");stream.seekg(0);
    std::array<char,16> header{};stream.read(header.data(),header.size());
    require(std::memcmp(header.data(),"SGNVPC01",8)==0&&get<std::uint32_t>(header.data()+8)==120&&get<std::uint32_t>(header.data()+12)==64,"native120 fixture header");
    std::vector<Case> cases(120);stream.read(reinterpret_cast<char*>(cases.data()),7680);require(bool(stream),"read native120 fixture");
    for(unsigned i=0;i<120;++i)require(cases[i].id==i&&cases[i].reserved==0,"native120 fixture index");return cases;
}
void common(const ViewportState& actual,const ViewportState& before,const SourceViewportObservation& trace,
    const std::array<std::uint32_t,6>& expected,std::uint16_t control,unsigned mxcsr){
    auto state=before;
    for(std::size_t i=0;i<6;++i){word(state,offsets[i],expected[i]);require(trace.stores[i].source_pc==pcs[i]&&trace.stores[i].value==expected[i],"source ordered actual store readbacks");}
    require(std::memcmp(&state,&actual,sizeof(state))==0,"six owner fields and every other byte");
    require(trace.store_count==6&&trace.entry_cw==control&&trace.exit_cw==control,"source store count and CW");
    require(trace.entry_mxcsr==mxcsr&&trace.exit_mxcsr==mxcsr,"MXCSR was touched");
}
void hardware(Records& records){
    struct Hardware {std::uint32_t x,y,sx,sy;std::uint16_t initial,required;};
    // Instruction-derived flags, NOT emulatorSW. Masked sticky IE/DE/PE/OE.
    const std::array<Hardware,6> cases{{
        {0x3fc00000,0xbfc00000,0,0,0,0x20},
        {0x7fc12345,0x7f812345,0,0,0,0x01},
        {1,0x80000001,0,0,0,0x22},
        {0x7f7fffff,0xff7fffff,0,0,0,0x01},
        {0x3f800000,0x40000000,0,0,0x24,0x24},
        {0x7f7fffff,0x7f7fffff,0x7f7fffff,0x7f7fffff,0,0x29}}};
    unsigned index=0;
    for(const auto& c:cases){
        ViewportState state;initialize(state,c.sx,c.sy);SourceViewportObservation trace{};
        sgn_viewport_test_seed_fpu(0x027fu,c.initial,0xffffu);_mm_setcsr(0x1fa1u);
        const auto admitted=set_viewport_origin_win64_424bd0(state,c.x,c.y,0x027fu,&trace);const auto after=capture();
        sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
        records.write(observation(index++,"hardware_flags",trace,after));
        require(admitted==SourceViewportAdmission::accepted,"hardware case admission");
        require((sw(after)&0x3fu)==c.required,"instruction-derived hardware sticky flags");
        require(cw(after)==0x027fu&&mx(after)==0x1fa1u&&after.data[4]==0&&(sw(after)&0x3800u)==0,"hardware CW/MXCSR/stack");
    }
    for(unsigned rc=0;rc<4;++rc){
        ViewportState state;initialize(state);SourceViewportObservation trace{};
        const auto control=static_cast<std::uint16_t>(0x027fu|(rc<<10));
        sgn_viewport_test_seed_fpu(control,0,0xffffu);
        const auto result=set_viewport_origin_win64_424bd0(state,0x80000000u,0x80000000u,control,&trace);const auto after=capture();
        sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
        records.write(observation(rc,"hardware_signed_zero",trace,after));
        const auto zero=rc==1?0x80000000u:0u;
        require(result==SourceViewportAdmission::accepted&&word(state,offsets[4])==zero&&word(state,offsets[5])==zero,"hardware cancellation zero sign");
    }
}
void admission(Records& records){
    struct Rejection{std::uint16_t actual,expected,status,tag;SourceViewportAdmission rejection;};
    const std::array<Rejection,6> cases{{
        {0x027f,0x037f,0,0xffff,SourceViewportAdmission::control_mismatch},
        {0x027e,0x027e,0,0xffff,SourceViewportAdmission::unmasked},
        {0x017f,0x017f,0,0xffff,SourceViewportAdmission::reserved_precision},
        {0x027f,0x027f,0,0xfffc,SourceViewportAdmission::occupied_stack},
        {0x027f,0x027f,0x0800,0xffff,SourceViewportAdmission::nonzero_top},
        {0x027e,0x027e,0x0081,0xffff,SourceViewportAdmission::unmasked}}};
    unsigned index=0;
    for(const auto& c:cases){
        ViewportState state;initialize(state);const auto before_state=state;
        SourceViewportObservation trace{};
        std::array<unsigned char,sizeof(trace)> before_trace{};
        std::memcpy(before_trace.data(),&trace,sizeof(trace));
        sgn_viewport_test_seed_fpu(c.actual,c.status,c.tag);const auto before=capture();
        const auto result=set_viewport_origin_win64_424bd0(state,0,0,c.expected,&trace);const auto after=capture();
        // Reset potentially pending unmasked state BEFORE logging/exception code.
        sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
        records.write(observation(index++,"admission_rejection",trace,after));
        require(result==c.rejection,"exact admission rejection");
        require(std::memcmp(&state,&before_state,sizeof(state))==0&&std::memcmp(&trace,before_trace.data(),sizeof(trace))==0,"rejection mutated owner/observer");
        require(cw(after)==cw(before)&&sw(after)==sw(before)&&after.data[4]==before.data[4]&&mx(after)==mx(before),"rejection mutated FPU state");
    }
}
}
int wmain(int argc,wchar_t** argv)try{
    require(argc==4&&std::wstring(argv[1])==L"--main-reviewed","Main only: --main-reviewed cases.bin fresh-results.jsonl");
    const auto cases=load_cases(argv[2]);Records records(argv[3]);Restore restore;
    unsigned mxcsr_mask=get<std::uint32_t>(restore.initial.data.data()+28);if(!mxcsr_mask)mxcsr_mask=0xffbfu;
    for(const auto& c:cases){
        ViewportState state;initialize(state,c.inputs[2],c.inputs[3]);const auto before=state;
        SourceViewportObservation trace{};
        const auto rc=(c.control>>10)&3u;
        const auto seeded_sse=(0x1fa1u|(((rc+1)%4)<<13)|(mxcsr_mask&0x8040u))&mxcsr_mask;
        sgn_viewport_test_seed_fpu(c.control,0,0xffffu);_mm_setcsr(seeded_sse);
        const auto result=set_viewport_origin_win64_424bd0(state,c.inputs[0],c.inputs[1],c.control,&trace);const auto after=capture();
        sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
        records.write(observation(c.id,"original_value_receipt",trace,after));
        require(result==SourceViewportAdmission::accepted,"native120 admission");
        common(state,before,trace,c.expected,c.control,seeded_sse);
        require(trace.fistp64==c.ftol,"native120 actual FISTP64");
        require(cw(after)==c.control&&mx(after)==seeded_sse&&after.data[4]==0&&(sw(after)&0x3800u)==0,"native120 CW/MXCSR/stack");
    }
    hardware(records);admission(records);
    // Exercise production default null-observer edge without a second owner.
    ViewportState state;initialize(state);sgn_viewport_test_seed_fpu(0x027fu,0,0xffffu);
    require(set_viewport_origin_win64_424bd0(state,0x3fc00000,0x40200000,0x027fu)==SourceViewportAdmission::accepted,"null observer admission");
    require(state.center_pixel_x==1&&state.center_pixel_y==2,"null observer actual values");
    records.write("{\"status\":\"NATIVE_RECEIVING_PASS\",\"original_value_cases\":120,\"hardware_flag_cases\":6,\"signed_zero_cases\":4,\"admission_cases\":6,\"null_observer_cases\":1,\"original_executed\":false,\"cold_parent_qualified\":false}");
    return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
