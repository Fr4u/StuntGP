#include "sgn/retail_frontend_descriptors.hpp"
#include "sgn/retail_frontend_state.hpp"
#include "sgn/retail_world_material_loading.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <iostream>
#include <memory>
#include <stdexcept>

using namespace sgn::retail;
namespace {
std::uint64_t checks=0,compared=0;
void require(bool ok,const char* why){++checks;if(!ok)throw std::runtime_error(why);}
struct Golden {std::uint32_t id,left,top,right,bottom,material;};
// Hand-evaluated instruction examples, not output from the implementation or
// an original-execution corpus. At least one example for every source handler,
// plus endpoints and row/material transitions. PE branch mapping is audited.
constexpr Golden golden[]{
 {1,1,1,255,255,0x10039},{12,1,1,255,255,0x10044},
 {13,0,0,128,64,0x10037},{14,128,0,256,64,0x10037},{15,0,64,128,128,0x10037},{20,128,192,256,256,0x10037},
 {21,0,0,128,64,0x10038},{28,128,192,256,256,0x10038},
 {29,54,83,255,255,0x10051},{30,1,83,214,255,0x10052},{31,54,1,255,179,0x10053},{32,1,1,214,179,0x10054},
 {33,54,83,255,255,0x10055},{34,1,83,214,255,0x10056},{35,54,1,255,179,0x10057},{36,1,1,214,179,0x10058},
 {37,1,64,255,192,0x10045},{48,1,64,255,192,0x10050},
 {49,1,1,63,63,0x1002b},{52,193,1,255,63,0x1002b},{53,1,65,63,127,0x1002b},{56,193,65,255,127,0x1002b},
 {57,0,0,255,256,0x10059},{58,1,0,256,256,0x1005a},{59,0,0,255,256,0x1005b},{60,1,0,256,256,0x1005c},
 {61,1,193,16,208,0x1002b},{64,52,193,67,208,0x1002b},
 {65,193,193,208,208,0x10025},{67,227,193,242,208,0x10025},
 {68,193,210,208,225,0x10025},{70,227,210,242,225,0x10025},
 {71,193,227,208,242,0x10025},{73,227,227,242,242,0x10025},
 {74,65,193,91,207,0x10025},{76,65,225,91,239,0x10025},
 {77,1,1,63,63,0x1002c},{80,193,1,255,63,0x1002c},{81,1,65,63,127,0x1002c},{86,65,129,127,191,0x1002c},
 {87,0,1,256,31,0x10026},{90,0,1,256,31,0x10029},{91,0,1,256,63,0x1002a},
 {92,0,0,256,256,1},{107,0,0,256,256,16},
 {108,1,129,63,191,0x1002b},{109,65,129,127,191,0x1002b},
 {110,1,1,63,63,0x10025},{113,193,1,255,63,0x10025},
 {114,1,193,14,206,0x10025},{117,1,238,14,251,0x10025},{118,129,193,135,199,0x10025},
 {119,30,30,226,226,0x1005d},{217,30,30,226,226,0x100bf},
 {218,0,0,255,256,0x100c0},{241,0,0,255,256,0x100d7},
 {242,1,0,256,256,0x100d8},{265,1,0,256,256,0x100ef},
 {266,0,0,255,256,0x100f0},{286,0,0,255,256,0x10104},
 {287,1,0,256,256,0x10105},{307,1,0,256,256,0x10119},
 {308,0,0,256,256,0x10035},{309,0,0,256,256,0x10036},
 {310,0,0,64,64,0x10035},{313,192,0,256,64,0x10035},{314,0,64,64,128,0x10035},{325,192,192,256,256,0x10035},
 {326,0,0,64,64,0x10036},{341,192,192,256,256,0x10036},
 {342,1,1,127,127,0x1002d},{345,129,129,255,255,0x1002d},{346,1,1,127,127,0x1002e},{354,1,1,127,127,0x10030},
 {355,1,1,127,127,0x10031},{358,129,129,255,255,0x10031},{359,1,1,127,127,0x10032},{367,1,1,127,127,0x10034},
 {368,1,1,127,127,0x1011a},{369,1,1,127,127,0x1011b},{383,1,1,127,127,0x10129},
 {384,1,1,255,255,0x1012a},{386,1,1,255,255,0x1012c}
};
constexpr std::array invalid{0u,387u,388u,511u,0xffffu,0x10000u,0x7ffffffeu,0x7fffffffu,0x80000000u,0x80000001u,0xfffffffeu,0xffffffffu};
void float_word(FrontendDescriptor row,std::size_t at,std::uint32_t value){
    persistence_word(row,at,std::bit_cast<std::uint32_t>(static_cast<float>(value)));
}
void expected_row(FrontendDescriptor row,const Golden& g){
    float_word(row,12,g.right-g.left);float_word(row,16,g.bottom-g.top);
    float_word(row,20,g.left*4);float_word(row,24,g.top*4);
    float_word(row,28,g.right*4);float_word(row,32,g.bottom*4);
    persistence_word(row,36,g.material);
}
void rows(){
    for(unsigned pattern=0;pattern<2;++pattern)for(unsigned alignment=0;alignment<4;++alignment){
        for(const auto& g:golden){
            std::array<std::uint8_t,80> actual{};
            for(std::size_t i=0;i<actual.size();++i)actual[i]=static_cast<std::uint8_t>(pattern?i*37+g.id:0xcd);
            auto expected=actual;const auto offset=8+alignment;
            expected_row(FrontendDescriptor{expected.data()+offset,48},g);
            require(initialize_frontend_descriptor_441380(FrontendDescriptor{actual.data()+offset,48},g.id)==1,"valid descriptor return");
            require(actual==expected,"hand-evaluated descriptor or retained bytes differ");compared+=actual.size();
        }
        for(const auto id:invalid){
            std::array<std::uint8_t,80> actual{};actual.fill(pattern?0xff:0xa5);const auto before=actual;
            require(initialize_frontend_descriptor_441380(FrontendDescriptor{actual.data()+8+alignment,48},id)==0,"invalid descriptor return");
            require(actual==before,"invalid descriptor must not write");compared+=actual.size();
        }
    }
}
void owner_and_bank(){
    auto f=std::make_unique<FrontendSourceStorage>();
    const auto bank=frontend_descriptor_bank(*f);
    require(bank.data()==reinterpret_cast<std::uint8_t*>(f->frontend_descriptors.data()),"sole physical bank");
    for(std::uint32_t i=0;i<0x1224;++i){
        auto& word=f->source_word(0x608d88+i*4);
        require(&word==&f->frontend_descriptors[i],"word identity");word=i^0xba43591fu;
        require(persistence_word(bank,i*4)==word,"word/byte alias");
    }
    require(f->source_bytes(0x608db6,4).data()==bank.data()+46,"same-owner row crossing allowed");
    for(auto [address,size]:std::array<std::pair<std::uint32_t,std::size_t>,5>{{{0x608d84,8},{0x608d87,1},{0x608d88,18577},{0x60d614,8},{0x60d618,1}}}){
        bool rejected=false;try{(void)f->source_bytes(address,size);}catch(const sgn::FormatError&){rejected=true;}
        require(rejected,"unowned outer boundary must reject");
    }
    for(auto address:{0x608d84u,0x608d89u,0x60d618u}){
        bool rejected=false;try{(void)f->source_word(address);}catch(const sgn::FormatError&){rejected=true;}
        require(rejected,"typed word gap/end/alignment must reject");
    }
    initialize_frontend_descriptors_441340(bank);
    require(std::all_of(bank.begin(),bank.begin()+48,[](auto x){return x==0;}),"row zero only cleared");
    for(const auto& g:golden){
        std::array<std::uint8_t,48> expected{};expected_row(expected,g);
        require(std::equal(expected.begin(),expected.end(),bank.begin()+g.id*48),"full bank golden row");compared+=48;
    }
    // Composition and bounds, not independent arithmetic parity for unlisted IDs.
    for(std::uint32_t id=1;id<387;++id){
        std::array<std::uint8_t,48> child{};(void)initialize_frontend_descriptor_441380(child,id);
        require(std::equal(child.begin(),child.end(),bank.begin()+id*48),"full bank visits every child");compared+=48;
        for(auto offset:{0u,4u,8u,40u,44u})require(persistence_word(bank,id*48+offset)==0,"unwritten words cleared");
    }
    const auto stable=f->frontend_descriptors;
    std::fill(bank.begin(),bank.end(),std::uint8_t{0x79});
    initialize_frontend_descriptors_441340(bank);require(f->frontend_descriptors==stable,"reinitialization resets dirty bank");
    const auto prelude=source_world_material_prelude(2);
    require(prelude.size()==301 && source_world_material_prelude(2).data()==prelude.data(),"borrow existing stable frontend prelude");
    for(const auto& entry:prelude)require(entry.name!=nullptr,"pinned prelude nonnull rows");
    f->lookup_a.fill(0x12a38745);f->lookup_b.fill(0x3407bd83);f->lookup_c.fill(0x893ca742);
    const auto a=f->lookup_a;const auto b=f->lookup_b;const auto c=f->lookup_c;
    const auto boundary=initialize_frontend_material_prefix_441190(*f,prelude);
    require(boundary.stop==FrontendInitializationStop::before_4411c0 && boundary.prelude_entries==301,"actual prelude boundary");
    require(f->lookup_a==a && f->lookup_b==b && f->lookup_c==c,"excluded4411C0 must not execute");
    require(f->frontend_descriptors==stable,"actual owner retained after material prefix");
    // Host span bound and null terminator. Names are never dereferenced here.
    const std::string name="sentinel";
    const std::array<WorldMaterialPreludeEntry,3> terminated{{{&name,0,0},{nullptr,0xffffffff,0xffffffff},{&name,9,8}}};
    require(initialize_frontend_material_prefix_441190(*f,terminated).prelude_entries==1,"null terminates view");
    require(initialize_frontend_material_prefix_441190(*f,{}).prelude_entries==0,"empty native view");
}
void absolute(){
    constexpr std::array<std::pair<std::uint32_t,std::uint32_t>,8> cases{{{0,0},{1,1},{0x7fffffff,0x7fffffff},{0x80000000,0x80000000},
        {0x80000001,0x7fffffff},{0xffffffff,1},{0xfffffffe,2},{0xffff0000,0x10000}}};
    for(auto [input,expected]:cases)require(frontend_descriptor_absolute_43d9b0(input)==expected,"absolute modulo32 contract");
}
}
int main()try{
    rows();owner_and_bank();absolute();
    std::cout<<"{\"passed\":true,\"hand_evaluated_vectors\":"<<std::size(golden)<<",\"valid_row_calls\":"<<std::size(golden)*8
        <<",\"invalid_row_calls\":96,\"checks\":"<<checks<<",\"compared_bytes\":"<<compared<<",\"original_execution_parity\":false}\n";
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
