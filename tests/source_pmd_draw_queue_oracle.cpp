#include "sgn/retail_model_draw_queue.hpp"
#include <bit>
#include <cstring>
#include <fstream>
#include <iostream>

namespace {
using namespace sgn;using namespace sgn::retail;
constexpr std::uint32_t regular_base=0x62f5d8,background_base=0x632cf8,matrix_base=0x61c7cc;
struct Frame {
    std::uint32_t regular{},background{},matrix{},count{};
    std::array<ModelDrawCommand,168> regular_records{},background_records{};
    std::array<CameraMatrix,384> matrices{};
};
static_assert(sizeof(Frame)==52816);
std::uint64_t words{},calls{},continued{},resets{},flag_calls{};
void require(bool v,const char* m){if(!v)throw FormatError(m);}
void read(std::ifstream& in,auto& value){in.read(reinterpret_cast<char*>(&value),sizeof(value));require(bool(in),"truncated draw queue corpus");}
std::uint32_t word(std::ifstream& in){std::uint32_t value;read(in,value);return value;}
void compare(const auto& actual,const auto& expected,const char* label){
    static_assert(sizeof(actual)%4==0);const auto* a=reinterpret_cast<const unsigned char*>(&actual);const auto* e=reinterpret_cast<const unsigned char*>(&expected);
    for(std::size_t at=0;at<sizeof(actual);at+=4){std::uint32_t av,ev;std::memcpy(&av,a+at,4);std::memcpy(&ev,e+at,4);++words;
        if(av!=ev){std::cerr<<label<<" call="<<calls<<" offset="<<at<<" actual="<<std::hex<<av<<" expected="<<ev<<std::dec<<'\n';throw FormatError("source model draw queue mismatch");}}
}
std::uint32_t matrix_token(std::uint32_t address){
    if(address==0)return 0;if(address==0x61c468)return 1;
    require(address>=matrix_base&&address<matrix_base+384*64&&(address-matrix_base)%64==0,"invalid source queued matrix reference");return address-matrix_base+2;
}
std::uint32_t matrix_address(std::uint32_t token){
    if(token==0)return 0;if(token==1)return 0x61c468;
    require(token>=2&&(token-2)%64==0&&(token-2)/64<384,"invalid native queued matrix reference");return matrix_base+token-2;
}
void queue_input(ModelDrawQueue& queue,std::uint32_t cursor,std::uint32_t base,const std::array<ModelDrawCommand,168>& records,std::uint32_t descriptor,std::uint32_t identity){
    require(cursor==0||(cursor>=base&&cursor<=base+167*84&&(cursor-base)%84==0),"invalid source draw queue cursor");queue.cursor=cursor?std::optional((cursor-base)/84):std::nullopt;queue.records=records;
    for(auto& record:queue.records){require(record.model_identity==0||record.model_identity==descriptor,"unknown retained source model identity");if(record.model_identity)record.model_identity=identity;
        record.pose.tail[1]=matrix_token(record.pose.tail[1]);record.pose.tail[2]=matrix_token(record.pose.tail[2]);}
}
void queue_output(const ModelDrawQueue& queue,std::uint32_t& cursor,std::uint32_t base,std::array<ModelDrawCommand,168>& records,std::uint32_t descriptor,std::uint32_t identity){
    cursor=queue.cursor?base+*queue.cursor*84:0;records=queue.records;
    for(auto& record:records){require(record.model_identity==0||record.model_identity==identity,"unknown retained native model identity");if(record.model_identity)record.model_identity=descriptor;
        record.pose.tail[1]=matrix_address(record.pose.tail[1]);record.pose.tail[2]=matrix_address(record.pose.tail[2]);}
}
void transfer(Frame& state,ModelDrawQueue& regular,ModelDrawQueue& background,ViewMatrixArena& matrices,std::uint32_t descriptor,std::uint32_t identity,bool input){
    if(input){
        queue_input(regular,state.regular,regular_base,state.regular_records,descriptor,identity);queue_input(background,state.background,background_base,state.background_records,descriptor,identity);
        require(state.matrix>=matrix_base&&state.matrix<=matrix_base+384*64&&(state.matrix-matrix_base)%64==0,"invalid draw matrix cursor");
        matrices.cursor=state.matrix-matrix_base;matrices.count=state.count;matrices.matrices=state.matrices;
    }else{
        queue_output(regular,state.regular,regular_base,state.regular_records,descriptor,identity);queue_output(background,state.background,background_base,state.background_records,descriptor,identity);
        state.matrix=matrices.cursor?matrix_base+*matrices.cursor:0;state.count=matrices.count;state.matrices=matrices.matrices;
    }
}
void read_parts(std::ifstream& in,SourcePmdModelView model,unsigned lod,bool input){
    const auto count=word(in);require(count==model.part_count(),"draw model part count mismatch");
    for(unsigned i=0;i<count;++i){const auto pair=word(in);ModelTransform transform;read(in,transform);auto p=model.part(lod,i);const auto flags02=p.flags_02();
        if(input){p.flags().set(static_cast<std::uint16_t>(pair));flags02.set(static_cast<std::uint16_t>(pair>>16));p.transform().set(std::bit_cast<SourcePmdTransform>(transform));}
        else {compare(std::uint32_t(p.flags().value())|(std::uint32_t(flags02.value())<<16),pair,"part flag pair");auto actual=std::bit_cast<ModelTransform>(p.transform().value());
            actual.tail[0]=matrix_address(actual.tail[0]);actual.tail[1]=matrix_address(actual.tail[1]);compare(actual,transform,"model transform");}}
}
}
int main(int argc,char** argv)try{
    require(argc==3,"usage: draw-queue oracle corpus retail-root");std::ifstream in(argv[1],std::ios::binary);std::array<char,8> magic;read(in,magic);require(std::string_view(magic.data(),8)=="SGNMDQ01","invalid draw queue corpus");
    const auto model_count=word(in);require(model_count>0&&model_count<=78,"invalid draw model count");AssetStore assets(argv[2]);
    for(unsigned m=0;m<model_count;++m){const auto length=word(in);require(length>0&&length<96,"invalid draw model name");std::string name(length,'\0');in.read(name.data(),length);
        require(bool(in)&&name.find_first_of("/\\:")==name.npos&&name.find('\0')==name.npos,"unsafe draw model name");
        const auto raw=read_file(std::filesystem::path(argv[2])/"meshdata"/(name+".PMD"));std::uint32_t relative;std::memcpy(&relative,raw.data()+0x4c,4);const auto descriptor=0x10000000u+328+relative;
        SourceAllocationBank bank;bank.initialize_42eab0();ModelResources resources(assets,argv[2]);const auto identity=resources.load_source(bank,name);const auto model=resources.source_at(*resources.resolve(identity));
        ModelDrawQueue regular,background;ViewMatrixArena matrices;const auto count=word(in);require(count>0&&count<=1024,"invalid draw call count");
        for(unsigned n=0;n<count;++n){const auto owner=word(in),slot=word(in),flags=word(in),reset=word(in),reset_matrix=word(in);
            require(owner<=1&&slot<4&&reset<=1&&reset_matrix<=1&&(n!=0||reset==1),"invalid draw invocation header");
            Frame input,expected;read(in,input);if(reset)transfer(input,regular,background,matrices,descriptor,identity,true);
            else {if(reset_matrix)reset_view_matrix_cursor(matrices);Frame actual;transfer(actual,regular,background,matrices,descriptor,identity,false);require(std::memcmp(&input,&actual,sizeof(Frame))==0,"continued draw owner mismatch");++continued;}
            ModelTransform pose;read(in,pose);const auto model_flags=word(in);require(model_flags<=65535,"invalid model flag word");model.descriptor().field<std::uint16_t>(2).set(static_cast<std::uint16_t>(model_flags));
            const auto lod=model.source_lod_index(slot);read_parts(in,model,lod,true);
            append_model_draw(owner?background:regular,model,slot,identity,pose,flags,matrices);
            read(in,expected);Frame actual;transfer(actual,regular,background,matrices,descriptor,identity,false);compare(actual,expected,"draw owners");
            ModelTransform expected_pose;read(in,expected_pose);pose.tail[1]=matrix_address(pose.tail[1]);pose.tail[2]=matrix_address(pose.tail[2]);compare(pose,expected_pose,"caller pose");
            compare(std::uint32_t(model.descriptor().field<std::uint16_t>(2).value()),word(in),"model flags02");read_parts(in,model,lod,false);++calls;
        }
    }
    const auto reset_count=word(in);require(reset_count>0&&reset_count<=256,"invalid queue reset count");
    for(unsigned i=0;i<reset_count;++i){Frame initial,expected;read(in,initial);read(in,expected);ModelDrawQueue regular,background;ViewMatrixArena matrices;
        transfer(initial,regular,background,matrices,0x10001000,1,true);reset_model_draw_queues(regular,background);Frame actual;transfer(actual,regular,background,matrices,0x10001000,1,false);compare(actual,expected,"queue reset");++resets;}
    const auto flag_count=word(in);require(flag_count>0&&flag_count<=4096,"invalid direct draw flag case count");
    for(unsigned n=0;n<flag_count;++n){const auto op=word(in),count=word(in),flags=word(in);require(op<=1&&count<=64,"invalid direct flag invocation");
        std::array<std::uint32_t,65> input,expected;read(in,input);read(in,expected);require(input[0]<=65535,"invalid model flags02");
        auto model=static_cast<std::uint16_t>(input[0]);std::array<std::uint16_t,64> visibility{},part_flags{};std::vector<ModelDrawPartFlags> bound;
        for(unsigned i=0;i<64;++i){visibility[i]=static_cast<std::uint16_t>(input[i+1]);part_flags[i]=static_cast<std::uint16_t>(input[i+1]>>16);
            if(i<count)bound.push_back({visibility[i],part_flags[i]});}
        if(op)clear_model_draw_flags(model);else force_model_draw_flags(model,bound,flags);
        std::array<std::uint32_t,65> actual;actual[0]=model;for(unsigned i=0;i<64;++i)actual[i+1]=visibility[i]|(std::uint32_t(part_flags[i])<<16);compare(actual,expected,"direct flags");++flag_calls;
    }
    require(in.peek()==std::char_traits<char>::eof(),"trailing draw queue corpus");
    std::cout<<"draw_models="<<model_count<<" calls="<<calls<<" words="<<words<<" continued="<<continued<<" resets="<<resets<<" flag_calls="<<flag_calls<<" mismatches=0 tolerance_ulp=0\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
