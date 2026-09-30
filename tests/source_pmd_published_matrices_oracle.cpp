#include "sgn/retail_model_matrices.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>
#include <map>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr std::uint32_t base=0x61c7cc,identity_address=0x61c468;
std::uint64_t words{},continuous{},outer_cases{},model_cases{},parts{};
void require(bool v,const char* m){if(!v)throw FormatError(m);}
void read(std::ifstream& in,auto& v){in.read(reinterpret_cast<char*>(&v),sizeof(v));require(bool(in),"truncated matrix corpus");}
std::uint32_t word(std::ifstream& in){std::uint32_t v;read(in,v);return v;}
void compare(const auto& actual,const auto& expected,const char* what){
    static_assert(sizeof(actual)%4==0);const auto* a=reinterpret_cast<const unsigned char*>(&actual);const auto* b=reinterpret_cast<const unsigned char*>(&expected);
    for(std::size_t at=0;at<sizeof(actual);at+=4){std::uint32_t av,bv;std::memcpy(&av,a+at,4);std::memcpy(&bv,b+at,4);++words;
        if(av!=bv){std::cerr<<what<<" outer="<<outer_cases<<" model="<<model_cases<<" offset="<<at<<" actual="<<std::hex<<av<<" expected="<<bv<<std::dec<<'\n';throw FormatError("source matrix state mismatch");}}
}
std::uint32_t guest(std::uint32_t reference){
    if(reference==1)return identity_address;
    require(reference>=2&&(reference-2)%64==0&&(reference-2)/64<ViewMatrixArena::capacity,"invalid matrix output reference");return base+reference-2;
}
void arena_input(std::ifstream& in,ViewMatrixArena& arena,CameraMatrix& identity,bool reset){
    const auto pointer=word(in),count=word(in);decltype(arena.matrices) matrices;CameraMatrix matrix;
    read(in,matrices);read(in,matrix);
    require(pointer>=base&&pointer<=base+ViewMatrixArena::capacity*64&&(pointer-base)%64==0,"invalid source matrix cursor");
    if(reset){arena.cursor=pointer-base;arena.count=count;arena.matrices=matrices;identity=matrix;}
    else {require(arena.cursor&&base+*arena.cursor==pointer&&arena.count==count&&
        std::memcmp(arena.matrices.data(),matrices.data(),sizeof(matrices))==0&&identity==matrix,"continued matrix owner changed");++continuous;}
}
void arena_output(std::ifstream& in,const ViewMatrixArena& arena,const CameraMatrix& identity){
    const auto pointer=word(in),count=word(in);compare(base+*arena.cursor,pointer,"cursor");compare(arena.count,count,"count");
    decltype(arena.matrices) matrices;CameraMatrix matrix;read(in,matrices);read(in,matrix);compare(arena.matrices,matrices,"arena");compare(identity,matrix,"borrowed identity");
}
}
int main(int argc,char** argv)try{
    require(argc==3,"usage: matrix oracle corpus retail-root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);
    require(std::string_view(magic.data(),8)=="SGNMM001","invalid matrix corpus magic");
    const auto count=word(in);require(count>0&&count<=4096,"invalid outer matrix case count");ViewMatrixArena arena;CameraMatrix identity;
    for(unsigned c=0;c<count;++c){const auto reset=word(in);require(reset<=1&&(c!=0||reset==1),"invalid matrix reset flag");arena_input(in,arena,identity,reset==1);
        ModelTransform pose;read(in,pose);allocate_outer_model_matrices(pose,arena);arena_output(in,arena,identity);
        ModelTransform expected;read(in,expected);pose.tail[1]=guest(pose.tail[1]);pose.tail[2]=guest(pose.tail[2]);compare(pose,expected,"outer pose");++outer_cases;}
    const auto models=word(in);require(models>0&&models<=4096,"invalid model matrix case count");AssetStore assets(argv[2]);
    for(unsigned c=0;c<models;++c){
        const auto length=word(in);require(length>0&&length<96,"invalid model name length");std::string name(length,'\0');in.read(name.data(),length);
        require(bool(in)&&name.find_first_of("/\\:")==name.npos&&name.find('\0')==name.npos,"invalid model fixture name");
        const auto slot=word(in),count_parts=word(in),reset=word(in);require(slot<4&&count_parts<=128&&reset<=1&&(c!=0||reset==1),"invalid model binding header");
        SourceAllocationBank bank;bank.initialize_42eab0();ModelResources resources(assets,argv[2]);
        const auto id=resources.load_source(bank,name);const auto model=resources.source_at(*resources.resolve(id));
        const auto lod=model.source_lod_index(slot);require(model.part_count()==count_parts,"source/native part count mismatch");
        arena_input(in,arena,identity,reset==1);
        std::map<std::uint32_t,ModelTransform> alias_inputs;
        for(unsigned i=0;i<count_parts;++i){const auto flags=word(in);require(flags<=65535,"invalid source part flags");ModelTransform transform;read(in,transform);
            auto part=model.part(lod,i);if(auto it=alias_inputs.find(part.transform().location().offset);it!=alias_inputs.end())require(std::memcmp(&transform,&it->second,80)==0,"inconsistent source transform alias input");
            else alias_inputs.emplace(part.transform().location().offset,transform);
            part.flags().set(static_cast<std::uint16_t>(flags));part.transform().set(std::bit_cast<SourcePmdTransform>(transform));}
        allocate_model_part_matrices(model,slot,arena);arena_output(in,arena,identity);
        for(unsigned i=0;i<count_parts;++i){const auto flags=word(in);ModelTransform expected;read(in,expected);const auto p=model.part(lod,i);auto actual=std::bit_cast<ModelTransform>(p.transform().value());
            compare(std::uint32_t(p.flags().value()),flags,"part flags");
            for(unsigned j=0;j<2;++j){const auto& resolved=resolve_model_matrix(arena,identity,actual.tail[j]);
                if(actual.tail[j]==1)require(&resolved==&identity,"shared identity was replaced by a constant");
                else require(&resolved==&arena.matrices[(actual.tail[j]-2)/64],"matrix owner identity differs");actual.tail[j]=guest(actual.tail[j]);}
            compare(actual,expected,"part transform");++parts;}
        ++model_cases;
    }
    const auto identity_cases=word(in);require(identity_cases>0&&identity_cases<=64,"invalid identity writer case count");
    for(unsigned i=0;i<identity_cases;++i){CameraMatrix initial,expected;read(in,initial);read(in,expected);initialize_model_matrix_identity(initial);compare(initial,expected,"identity writer");}
    const auto empty_cases=word(in);require(empty_cases>0&&empty_cases<=64,"invalid zero-part case count");
    for(unsigned i=0;i<empty_cases;++i){arena_input(in,arena,identity,true);allocate_model_part_matrices({},arena);arena_output(in,arena,identity);}
    require(in.peek()==std::char_traits<char>::eof(),"trailing matrix corpus data");
    std::cout<<"outer="<<outer_cases<<" models="<<model_cases<<" parts="<<parts<<" words="<<words<<" continuous="<<continuous<<" identity="<<identity_cases<<" empty="<<empty_cases<<" mismatches=0 tolerance_ulp=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
