#pragma once
// Exact native/source identity codec shared in shape with the qualified queue test.
namespace {
using namespace sgn;using namespace sgn::retail;
constexpr std::uint32_t regular_base=0x62f5d8,background_base=0x632cf8,matrix_base=0x61c7cc;
struct Frame {
    std::uint32_t regular{},background{},matrix{},count{};
    std::array<ModelDrawCommand,168> regular_records{},background_records{};
    std::array<CameraMatrix,384> matrices{};
};
static_assert(sizeof(Frame)==52816);
std::uint64_t words{},calls{},continued{};
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
void read_parts(std::ifstream& in,ModelResource& model,unsigned lod,bool input){
    const auto count=word(in);require(count==model.part_count(lod),"draw model part count mismatch");
    for(unsigned i=0;i<count;++i){const auto pair=word(in);ModelTransform transform;read(in,transform);auto p=model.part(lod,i);auto& flags02=model.source_part_flags_02(lod,i);
        if(input){p.flags=static_cast<std::uint16_t>(pair);flags02=static_cast<std::uint16_t>(pair>>16);p.transform=transform;p.transform.tail[0]=matrix_token(transform.tail[0]);p.transform.tail[1]=matrix_token(transform.tail[1]);}
        else {compare(std::uint32_t(p.flags)|(std::uint32_t(flags02)<<16),pair,"part flag pair");auto actual=p.transform;
            actual.tail[0]=matrix_address(actual.tail[0]);actual.tail[1]=matrix_address(actual.tail[1]);compare(actual,transform,"model transform");}}
}
}